# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Include/import rule.

Slang discourages `#include` in favor of modules. Clean top-level header includes become
`__exported import` (the `__exported` preserves the transitive visibility a `#include` chain gave —
a file that re-exposed an included file's symbols keeps doing so). Includes that are structurally
preprocessor — indented fragment injection, `#include MACRO` composition, the partial-SRG collection
guards — stay `#include` (Slang still supports it) and are flagged, since they need the interface
redesign rather than a module. `#pragma once` is dropped (modules load once).
"""

from __future__ import annotations

import re

from ..lexing import CHANNEL_PREPROCESSOR
from ..rewrite import Edit, Rewriter, Severity

# Per-API prologues are builder-injected on the Slang path; an include of one would be wrong.
_SKIP_TARGETS = ("AzslcHeader",)

_DROP = {
    # ShaderResourceGroupSemantic blocks do not survive the port: the slot rides on the attribute.
    "SrgSemantics",
}

# Shared-SRG aggregates were hand-authored at different paths; kept as #include (they are parked and
# not imported as modules yet).
_REMAP = {
    "scenesrg_all.srgi": "Atom/Features/Srg/SceneSrg.slang",
    "scenesrg.srgi": "Atom/Features/Srg/SceneSrg.slang",
    "viewsrg_all.srgi": "Atom/Features/Srg/ViewSrg.slang",
    "viewsrg.srgi": "Atom/Features/Srg/ViewSrg.slang",
    "Atom/Features/Bindless.azsli": "Atom/Features/Bindless.slang",
}

_INCLUDE = re.compile(
    r"""^(?P<lead>\s*\#\s*include\s*)(?P<open>[<"])(?P<path>[^>"]+)(?P<close>[>"])""",
    re.VERBOSE,
)
# `#include MACRO` — a bare identifier target (no <> or ""), the composition pattern.
_INCLUDE_MACRO = re.compile(r"""^\s*\#\s*include\s+(?P<macro>[A-Za-z_]\w*)\s*$""")
_PRAGMA_ONCE = re.compile(r"""^\s*\#\s*pragma\s+once\b""")

_SOURCE_SUFFIXES = (".azsli", ".srgi", ".azsl")


def apply(lexed, registry, rewriter: Rewriter) -> None:
    # A library module (`.azsli`/`.srgi`) re-exports its imports so the transitive dependency closure
    # still reaches its consumers (preserving `#include` behavior). An entry shader (`.azsl`) is a
    # leaf nobody imports, so it just consumes with a plain `import` — no re-export to leak.
    import_keyword = "import" if lexed.path.suffix == ".azsl" else "__exported import"

    for token in lexed.tokens:
        if token.channel != CHANNEL_PREPROCESSOR:
            continue

        if _PRAGMA_ONCE.match(token.text):
            _remove_line(rewriter, token)  # modules load once
            continue

        macro_match = _INCLUDE_MACRO.match(token.text)
        if macro_match:
            rewriter.note(
                Severity.TODO,
                f"#include {macro_match.group('macro')}: build-chosen file (composition); left as"
                " #include, needs the material-pipeline generator seam, not a module",
                token.line,
            )
            continue

        match = _INCLUDE.match(token.text)
        if not match:
            continue

        target = match.group("path")

        if any(skip in target for skip in _SKIP_TARGETS):
            rewriter.note(Severity.TODO, f"include of {target} left unchanged (builder-injected prelude)", token.line)
            continue
        if any(dropped in target for dropped in _DROP):
            _remove_line(rewriter, token)  # no Slang counterpart (e.g. SrgSemantics)
            continue

        remapped = _remap(target)
        if remapped is not None:
            # Parked shared-SRG aggregate: keep as #include, normalize extension.
            rebuilt = token.text[: match.start("path")] + remapped + token.text[match.end("path") :]
            rewriter.replace_token(token, rebuilt)
            continue

        # An indented include is textually injected into an enclosing block (a struct body or a
        # preprocessor branch) — a fragment, not a module. Keep it as #include.
        if token.column > 0:
            rebuilt = token.text[: match.start("path")] + _to_slang(target) + token.text[match.end("path") :]
            rewriter.replace_token(token, rebuilt)
            rewriter.note(
                Severity.TODO,
                f"indented #include <{target}> kept (fragment injection); not a module",
                token.line,
            )
            continue

        # An include whose target is not a real source file is build-generated (e.g.
        # GeneratedTransforms/*, produced by the color-pipeline build step) — it cannot be a module.
        stem = target.replace("\\", "/").rsplit("/", 1)[-1].rsplit(".", 1)[0]
        if stem not in registry.source_stems:
            rebuilt = token.text[: match.start("path")] + _to_slang(target) + token.text[match.end("path") :]
            rewriter.replace_token(token, rebuilt)
            rewriter.note(
                Severity.TODO,
                f"#include <{target}> kept: target is build-generated, not a source module",
                token.line,
            )
            continue

        # A header that exports macros an `import` can't carry (a `#if` toggle or a function-like
        # macro) must stay `#include` so the macro still reaches this file.
        basename = target.replace("\\", "/").rsplit("/", 1)[-1]
        if basename in registry.preprocessor_export_files:
            rebuilt = token.text[: match.start("path")] + _to_slang(target) + token.text[match.end("path") :]
            rewriter.replace_token(token, rebuilt)
            rewriter.note(
                Severity.TODO,
                f"#include <{target}> kept: it exports preprocessor macros modules can't carry",
                token.line,
            )
            continue

        # Clean top-level header -> module import. Angle `<A/B/C>` is a virtual path rooted at a
        # ShaderLib root, so it becomes a dotted module name; quote "..." is relative to the importing
        # file, so it becomes a string-form import (resolved from the file's directory first).
        if match.group("open") == '"':
            rewriter.replace_token(token, f'{import_keyword} "{_to_slang(target)}";')
        else:
            rewriter.replace_token(token, f"{import_keyword} {_module_name(target)};")


def _remove_line(rewriter: Rewriter, token) -> None:
    """Delete a preprocessor line entirely, including its trailing newline."""
    end = token.stop + 1
    text = rewriter.text
    while end < len(text) and text[end] in " \t":
        end += 1
    if end < len(text) and text[end] == "\r":
        end += 1
    if end < len(text) and text[end] == "\n":
        end += 1
    rewriter.edits.append(Edit(token.start, end, ""))


def _remap(target: str) -> str | None:
    normalized = target.replace("\\", "/")
    for original, replacement in _REMAP.items():
        if normalized == original or normalized.endswith("/" + original):
            return _to_slang(replacement)
    return None


def _to_slang(target: str) -> str:
    """Normalize a source path's extension to `.slang` (all module/fragment files are .slang)."""
    for suffix in (*_SOURCE_SUFFIXES, ".slangi"):
        if target.endswith(suffix):
            return target[: -len(suffix)] + ".slang"
    return target


def _module_name(target: str) -> str:
    """Dotted module name from an include path: `Atom/Features/Foo.azsli` -> `Atom.Features.Foo`."""
    path = target.replace("\\", "/")
    for suffix in (*_SOURCE_SUFFIXES, ".slangi", ".slang"):
        if path.endswith(suffix):
            path = path[: -len(suffix)]
            break
    return path.strip("/").replace("/", ".")
