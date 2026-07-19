# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Include rule (10).

Ports keep `#include` rather than switching to Slang `import`: the include graph is what the
material pipeline's macro stitching relies on, and preserving it keeps the port diffable. Only the
extension changes, so both delimiter styles survive untouched.
"""

from __future__ import annotations

import re

from ..lexing import CHANNEL_PREPROCESSOR
from ..rewrite import Rewriter, Severity

# Per-API prologues have Slang equivalents (ApiPrelude.slang) that the builder injects, so an
# include of one would be wrong in a ported file.
_SKIP_TARGETS = ("AzslcHeader",)

# Includes whose Slang counterpart was hand-written at a different path, or has no counterpart at
# all. Mechanically swapping the extension would point these at files that do not exist.
_DROP = {
    # ShaderResourceGroupSemantic blocks do not survive the port: the frequency now rides on the
    # [AtomShaderResourceGroup] attribute, so there is nothing left to include.
    "SrgSemantics",
}

_REMAP = {
    # Scene/View SRGs were restructured by hand out of the partial-SRG aggregation files.
    "scenesrg_all.srgi": "Atom/Features/Srg/SceneSrg.slangi",
    "scenesrg.srgi": "Atom/Features/Srg/SceneSrg.slangi",
    "viewsrg_all.srgi": "Atom/Features/Srg/ViewSrg.slangi",
    "viewsrg.srgi": "Atom/Features/Srg/ViewSrg.slangi",
    # Bindless is a module, not an include fragment, so it keeps the .slang extension.
    "Atom/Features/Bindless.azsli": "Atom/Features/Bindless.slang",
}

_INCLUDE = re.compile(
    r"""^(?P<lead>\s*\#\s*include\s*)(?P<open>[<"])(?P<path>[^>"]+)(?P<close>[>"])""",
    re.VERBOSE,
)

_SUFFIXES = {".azsli": ".slangi", ".srgi": ".slangi", ".azsl": ".slang"}


def apply(lexed, registry, rewriter: Rewriter) -> None:
    for token in lexed.tokens:
        if token.channel != CHANNEL_PREPROCESSOR:
            continue
        match = _INCLUDE.match(token.text)
        if not match:
            continue

        target = match.group("path")
        if any(skip in target for skip in _SKIP_TARGETS):
            rewriter.note(
                Severity.TODO,
                f"include of {target} left unchanged; the Slang path injects its own prelude",
                token.line,
            )
            continue

        if any(dropped in target for dropped in _DROP):
            rewriter.replace_token(
                token, f"// [slang-port] removed: #include <{target}> has no Slang counterpart"
            )
            continue

        ported = _port_target(target)
        if ported == target:
            continue

        rebuilt = (
            token.text[: match.start("path")] + ported + token.text[match.end("path") :]
        )
        rewriter.replace_token(token, rebuilt)


def _port_target(target: str) -> str:
    normalized = target.replace("\\", "/")
    for original, replacement in _REMAP.items():
        if normalized == original or normalized.endswith("/" + original):
            return replacement
    for suffix, replacement in _SUFFIXES.items():
        if target.endswith(suffix):
            return target[: -len(suffix)] + replacement
    return target
