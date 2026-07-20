# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Pass 1: the whole-corpus scan the per-file rewrites depend on.

Two facts cross file boundaries and cannot be recovered from a single file:

* Shader options are declared in one `.azsli` and used from every file that includes it. Turning a
  use site into a call (`o_x` -> `o_x()`) requires knowing every option name up front.
* SRG member access (`PassSrg::m_x`) is only distinguishable from enum or namespace scoping
  (`ColorSpaceId::ACEScg`, which stays untouched) by knowing which names are SRGs.

So every file is lexed once to build these registries before any file is rewritten.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

from .lexing import CHANNEL_PREPROCESSOR, LexedFile, find_matching_brace, lex_file

from azslLexer import azslLexer as L  # noqa: N814

# Macros tested in a preprocessor conditional must stay `#define`s — a `public static const` cannot
# gate a `#if`. This finds the names each conditional reads.
_CONDITIONAL = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b(?P<expr>.*)$")
_IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
# `#define NAME` (object- or function-like); captures the macro name and whether it is function-like.
_DEFINE = re.compile(r"^\s*#\s*define\s+(?P<name>[A-Za-z_]\w*)(?P<paren>\()?")

# Frequencies from Atom/Features/SrgSemantics.azsli, named as the SrgBindingSlot constants declared
# in Atom/RPI/ShaderResourceGroup.slang. Semantics are also scanned from source so project-defined
# ones resolve; this map is the fallback and the source of the symbolic slot names.
FREQUENCY_TO_SLOT_NAME = {
    0: "Draw",
    1: "Object",
    2: "Material",
    3: "SubPass",
    4: "Pass",
    5: "View",
    6: "Scene",
    7: "Bindless",
}


@dataclass
class SemanticInfo:
    """A `ShaderResourceGroupSemantic` block: its frequency and whether it carries a variant key."""

    name: str
    frequency_id: int | None = None
    variant_fallback_bits: int | None = None
    declared_in: Path | None = None

    @property
    def slot_name(self) -> str | None:
        if self.frequency_id is None:
            return None
        return FREQUENCY_TO_SLOT_NAME.get(self.frequency_id)


@dataclass
class OptionInfo:
    """An AZSL `option` declaration, which becomes an `[AtomOption] extern` function in Slang."""

    name: str
    type_text: str
    default_text: str | None = None
    range_min: str | None = None
    range_max: str | None = None
    # Set when declared as `option enum class Mode {...} o_m;` — the enum splits out separately.
    inline_enum_name: str | None = None
    declared_in: Path | None = None


@dataclass
class SrgInfo:
    name: str
    semantic: str | None = None
    is_partial: bool = False
    declared_in: list[Path] = field(default_factory=list)


@dataclass
class Registry:
    srgs: dict[str, SrgInfo] = field(default_factory=dict)
    semantics: dict[str, SemanticInfo] = field(default_factory=dict)
    options: dict[str, OptionInfo] = field(default_factory=dict)
    enums: set[str] = field(default_factory=set)
    collisions: list[str] = field(default_factory=list)
    # Macro names read by any `#if`/`#ifdef`/`#ifndef`/`#elif` anywhere in the corpus; these must
    # remain `#define`s (a static const cannot gate the preprocessor).
    conditional_macros: set[str] = field(default_factory=set)
    # Function-like macro names defined anywhere (`#define NAME(args) ...`).
    function_macros: set[str] = field(default_factory=set)
    # Basenames of files that export a `#define` another module can't receive by `import` — a macro
    # tested in `#if` or a function-like macro. Includes of these stay `#include` so the macro crosses.
    preprocessor_export_files: set[str] = field(default_factory=set)
    # Stems (basename without extension) of every real source file. An include whose target is not
    # among these is build-generated (e.g. GeneratedTransforms/*) and cannot become a module.
    source_stems: set[str] = field(default_factory=set)

    def slot_expression(self, semantic_name: str | None) -> str | None:
        """`SrgBindingSlot.Pass` for a semantic, or None when it cannot be resolved."""
        if semantic_name is None:
            return None
        semantic = self.semantics.get(semantic_name)
        if semantic is None or semantic.slot_name is None:
            return None
        return f"SrgBindingSlot.{semantic.slot_name}"

    def is_srg(self, name: str) -> bool:
        return name in self.srgs


def build(paths: list[Path]) -> Registry:
    registry = Registry()
    # (path basename, object-like define names, function-like define names) per file, so that after
    # the whole corpus is scanned we can mark files that export macros no `import` can carry.
    per_file_defines: list[tuple[str, set[str], set[str]]] = []
    for path in paths:
        registry.source_stems.add(path.stem)
        try:
            lexed = lex_file(path)
        except Exception:  # a file that will not lex is reported later, during conversion
            continue
        scan_file(lexed, registry)
        object_defines, function_defines = _scan_defines(lexed, registry)
        _scan_conditionals(lexed, registry)
        per_file_defines.append((path.name, object_defines, function_defines))

    # A file must stay `#include`d if it defines a macro that another module reads and `import` can't
    # carry: a toggle tested in `#if`, or any function-like macro.
    for basename, object_defines, function_defines in per_file_defines:
        if function_defines or (object_defines & registry.conditional_macros):
            registry.preprocessor_export_files.add(basename)

    return registry


def _scan_defines(lexed: LexedFile, registry: Registry) -> tuple[set[str], set[str]]:
    object_defines: set[str] = set()
    function_defines: set[str] = set()
    for token in lexed.tokens:
        if token.channel != CHANNEL_PREPROCESSOR:
            continue
        match = _DEFINE.match(token.text)
        if not match:
            continue
        if match.group("paren"):
            function_defines.add(match.group("name"))
            registry.function_macros.add(match.group("name"))
        else:
            object_defines.add(match.group("name"))
    return object_defines, function_defines


def _scan_conditionals(lexed: LexedFile, registry: Registry) -> None:
    for token in lexed.tokens:
        if token.channel != CHANNEL_PREPROCESSOR:
            continue
        match = _CONDITIONAL.match(token.text)
        if not match:
            continue
        for name in _IDENTIFIER.findall(match.group("expr")):
            if name != "defined":
                registry.conditional_macros.add(name)


def scan_file(lexed: LexedFile, registry: Registry) -> None:
    code = lexed.code_tokens
    index = 0
    while index < len(code):
        token = code[index]
        if token.type == L.ShaderResourceGroupSemantic:
            index = _scan_semantic(code, index, lexed.path, registry)
        elif token.type == L.ShaderResourceGroup:
            index = _scan_srg(code, index, lexed.path, registry)
        elif token.type == L.Option:
            index = _scan_option(code, index, lexed.path, registry)
        elif token.type == L.Enum:
            _record_enum_name(code, index, registry)
            index += 1
        else:
            index += 1


def _scan_semantic(code: list, index: int, path: Path, registry: Registry) -> int:
    """`ShaderResourceGroupSemantic Name { FrequencyId = N; ShaderVariantFallback = M; };`"""
    if index + 1 >= len(code) or code[index + 1].type != L.Identifier:
        return index + 1
    info = SemanticInfo(name=code[index + 1].text, declared_in=path)

    brace = _next_of_type(code, index + 2, L.LeftBrace)
    if brace < 0:
        return index + 1
    close = find_matching_brace(code, brace)
    if close < 0:
        return index + 1

    for cursor in range(brace + 1, close):
        if code[cursor].type in (L.FrequencyId, L.ShaderVariantFallback):
            value = _literal_after_assign(code, cursor, close)
            if value is None:
                continue
            if code[cursor].type == L.FrequencyId:
                info.frequency_id = value
            else:
                info.variant_fallback_bits = value

    registry.semantics[info.name] = info
    return close + 1


def _scan_srg(code: list, index: int, path: Path, registry: Registry) -> int:
    """`[partial] ShaderResourceGroup Name [: Semantic] { ... }`"""
    is_partial = index > 0 and code[index - 1].type == L.Partial
    if index + 1 >= len(code) or code[index + 1].type != L.Identifier:
        return index + 1

    name = code[index + 1].text
    semantic = None
    cursor = index + 2
    if cursor < len(code) and code[cursor].type == L.Colon:
        if cursor + 1 < len(code) and code[cursor + 1].type == L.Identifier:
            semantic = code[cursor + 1].text

    existing = registry.srgs.get(name)
    if existing is None:
        registry.srgs[name] = SrgInfo(
            name=name, semantic=semantic, is_partial=is_partial, declared_in=[path]
        )
    else:
        existing.declared_in.append(path)
        existing.is_partial = existing.is_partial or is_partial
        if existing.semantic is None:
            existing.semantic = semantic
    return index + 2


def _scan_option(code: list, index: int, path: Path, registry: Registry) -> int:
    """`option <type> o_name [= default];` including the `option enum class E {...} o_name` form."""
    cursor = index + 1
    inline_enum_name = None

    if cursor < len(code) and code[cursor].type == L.Enum:
        cursor += 1
        if cursor < len(code) and code[cursor].type == L.Class:
            cursor += 1
        if cursor < len(code) and code[cursor].type == L.Identifier:
            inline_enum_name = code[cursor].text
            registry.enums.add(inline_enum_name)
            cursor += 1
        brace = _next_of_type(code, cursor, L.LeftBrace)
        if brace < 0:
            return index + 1
        close = find_matching_brace(code, brace)
        if close < 0:
            return index + 1
        cursor = close + 1
        type_text = inline_enum_name or ""
    else:
        type_start = cursor
        while cursor < len(code) and code[cursor].type != L.Identifier:
            cursor += 1
        # The option's own name is the last identifier before `=` or `;`, so walk type tokens first.
        type_end = cursor
        type_text = "".join(token.text for token in code[type_start:type_end]) or (
            code[type_start].text if type_start < len(code) else ""
        )
        if type_end < len(code) and code[type_end].type == L.Identifier:
            # A user-defined type (e.g. `option MyEnum o_x;`) shows up as Identifier Identifier.
            if type_end + 1 < len(code) and code[type_end + 1].type == L.Identifier:
                type_text = code[type_end].text
                cursor = type_end + 1
            else:
                cursor = type_end

    if cursor >= len(code) or code[cursor].type != L.Identifier:
        return index + 1

    name = code[cursor].text
    default_text = None
    cursor += 1
    if cursor < len(code) and code[cursor].type == L.Assign:
        end = _next_of_type(code, cursor, L.Semi)
        if end > 0:
            default_text = "".join(token.text for token in code[cursor + 1 : end]).strip()

    # The same option name is legitimately declared in several files with different defaults —
    # those files are independent translation units. Declarations are always rewritten in place
    # from the local text, so only the global set of option *names* matters here (rule 6); a
    # differing type or default is worth noting but is not an error.
    previous = registry.options.get(name)
    if previous is not None and (
        previous.type_text != type_text.strip() or previous.default_text != default_text
    ):
        registry.collisions.append(
            f"option {name}: {previous.declared_in} declares"
            f" ({previous.type_text}, default {previous.default_text});"
            f" {path} declares ({type_text.strip()}, default {default_text})"
        )

    registry.options[name] = OptionInfo(
        name=name,
        type_text=type_text.strip(),
        default_text=default_text,
        inline_enum_name=inline_enum_name,
        declared_in=path,
    )
    return cursor


def _record_enum_name(code: list, index: int, registry: Registry) -> None:
    cursor = index + 1
    if cursor < len(code) and code[cursor].type == L.Class:
        cursor += 1
    if cursor < len(code) and code[cursor].type == L.Identifier:
        registry.enums.add(code[cursor].text)


def _next_of_type(code: list, start: int, token_type: int) -> int:
    for cursor in range(start, len(code)):
        if code[cursor].type == token_type:
            return cursor
    return -1


def _literal_after_assign(code: list, start: int, limit: int) -> int | None:
    for cursor in range(start, limit):
        if code[cursor].type == L.Assign and cursor + 1 < limit:
            try:
                return int(code[cursor + 1].text, 0)
            except ValueError:
                return None
    return None
