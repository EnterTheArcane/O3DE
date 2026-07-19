# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""ShaderResourceGroup rules (1, 2, 11, 13).

AZSL's dedicated SRG syntax becomes an attributed struct plus a `ParameterBlock` instance. The
instance keeps the original SRG name because that name is runtime-facing: the reflection walker
names the group after the ParameterBlock variable (SlangReflectionWalker.cpp), which flows into
ShaderResourceGroupLayout::SetName and is what C++ looks up via FindShaderResourceGroupLayout.
The struct therefore takes the `<Name>Layout` spelling instead.
"""

from __future__ import annotations

import re

from ..lexing import CHANNEL_PREPROCESSOR, find_matching_brace
from ..registry import Registry
from ..rewrite import Rewriter, Severity

from azslLexer import azslLexer as L  # noqa: N814

LAYOUT_SUFFIX = "Layout"

# SRGs whose Slang port is a `namespace`, not a `ParameterBlock`, so `Name::member` stays as-is.
# Bindless holds five unsized arrays and Slang permits at most one per struct, so Bindless.slang
# declares them at global scope and wraps the accessors in `namespace Bindless`, keeping AZSL's
# `Bindless::GetTexture2D(42)` call form working unchanged.
NAMESPACE_BACKED_SRGS = frozenset({"Bindless"})


def layout_type_name(srg_name: str) -> str:
    return f"{srg_name}{LAYOUT_SUFFIX}"


def apply(lexed, registry: Registry, rewriter: Rewriter) -> None:
    code = lexed.code_tokens
    # SRG name -> member names hoisted out to global scope (groupshared). Their use sites lose the
    # SRG qualifier entirely, so they are tracked here and handled specially in requalification.
    hoisted_members: dict[str, set[str]] = {}
    index = 0
    while index < len(code):
        token = code[index]
        if token.type == L.ShaderResourceGroupSemantic:
            index = _remove_semantic_block(code, index, rewriter)
        elif token.type == L.ShaderResourceGroup:
            index = _convert_srg(code, index, registry, rewriter, hoisted_members)
        else:
            index += 1

    _requalify_member_access(code, registry, rewriter, hoisted_members)
    _requalify_member_access_in_macros(lexed, registry, rewriter)


def _remove_semantic_block(code, index: int, rewriter: Rewriter) -> int:
    """Rule 11: the frequency now rides on [AtomShaderResourceGroup], so the block goes away."""
    brace = _next_of_type(code, index, L.LeftBrace)
    if brace < 0:
        return index + 1
    close = find_matching_brace(code, brace)
    if close < 0:
        return index + 1

    end = close
    if close + 1 < len(code) and code[close + 1].type == L.Semi:
        end = close + 1

    rewriter.delete_span(code[index], code[end])
    return end + 1


def _convert_srg(code, index: int, registry: Registry, rewriter: Rewriter, hoisted_members: dict) -> int:
    """Rule 1: `ShaderResourceGroup Name : Semantic { ... }` -> attribute + struct + ParameterBlock."""
    is_partial = index > 0 and code[index - 1].type == L.Partial
    if index + 1 >= len(code) or code[index + 1].type != L.Identifier:
        return index + 1

    name_token = code[index + 1]
    srg_name = name_token.text

    cursor = index + 2
    semantic_name = None
    semantic_last_token = None
    if cursor < len(code) and code[cursor].type == L.Colon:
        if cursor + 1 < len(code) and code[cursor + 1].type == L.Identifier:
            semantic_name = code[cursor + 1].text
            semantic_last_token = code[cursor + 1]
            cursor += 2

    brace = _next_of_type(code, cursor, L.LeftBrace)
    if brace < 0:
        return index + 1
    close = find_matching_brace(code, brace)
    if close < 0:
        rewriter.note(
            Severity.MANUAL,
            f"ShaderResourceGroup {srg_name} has no matching closing brace",
            name_token.line,
        )
        return index + 1

    slot_expression = registry.slot_expression(semantic_name)
    if slot_expression is None:
        rewriter.note(
            Severity.TODO,
            f"ShaderResourceGroup {srg_name}: cannot resolve binding slot"
            f" from semantic {semantic_name!r}",
            name_token.line,
        )
        slot_expression = f"/* TODO(slang-port): unresolved semantic {semantic_name} */ 0"

    if is_partial:
        # Slang has no `partial`; composition has to be restructured by hand into an aggregate
        # struct (see Srg/SceneSrg.slangi for the shape). Converting the declaration mechanically
        # would produce a second, conflicting definition of the same SRG.
        rewriter.note(
            Severity.MANUAL,
            f"partial ShaderResourceGroup {srg_name}: compose into the aggregate struct by hand",
            name_token.line,
        )
        partial_token = code[index - 1]
        rewriter.replace_span(
            partial_token,
            semantic_last_token or name_token,
            f"// TODO(slang-port): partial ShaderResourceGroup {srg_name}"
            f" - compose into the aggregate struct\n"
            f"[AtomShaderResourceGroup({slot_expression})]\n"
            f"struct {layout_type_name(srg_name)}",
        )
        _close_struct(code, close, rewriter, srg_name, emit_parameter_block=False)
        return close + 1

    hoisted, hoisted_names = _hoist_groupshared(code, brace, close, rewriter)
    if hoisted_names:
        hoisted_members[srg_name] = hoisted_names
    header_last = semantic_last_token or name_token
    rewriter.replace_span(
        code[index],
        header_last,
        hoisted + f"[AtomShaderResourceGroup({slot_expression})]\nstruct {layout_type_name(srg_name)}",
    )
    _close_struct(code, close, rewriter, srg_name, emit_parameter_block=True)
    return close + 1


def _hoist_groupshared(code, brace: int, close: int, rewriter: Rewriter) -> tuple[str, set[str]]:
    """Move `groupshared` declarations out of the SRG to global scope.

    Slang forbids `groupshared` on a struct/ParameterBlock member (E31201), but AZSL SRGs may declare
    shared memory inside the group. Each declaration is deleted from the body and re-emitted above the
    struct. Returns the text to emit above the struct and the set of hoisted member names, whose use
    sites (`PassSrg::smColor`) must drop the SRG qualifier entirely rather than becoming `PassSrg.`.
    """
    hoisted: list[str] = []
    names: set[str] = set()
    depth = 0
    cursor = brace + 1
    while cursor < close:
        token_type = code[cursor].type
        if token_type == L.LeftBrace:
            depth += 1
        elif token_type == L.RightBrace:
            depth -= 1
        elif token_type == L.Groupshared and depth == 0:
            semi = _next_of_type(code, cursor, L.Semi)
            if 0 <= semi < close:
                hoisted.append(rewriter.text[code[cursor].start : code[semi].stop + 1])
                names.add(_declared_name(code, cursor, semi))
                rewriter.delete_span(code[cursor], code[semi])
                cursor = semi + 1
                continue
        cursor += 1

    if not hoisted:
        return "", set()
    text = (
        "// [slang-port] hoisted out of the ShaderResourceGroup (Slang has no groupshared member)\n"
        + "\n".join(hoisted)
        + "\n"
    )
    names.discard("")
    return text, names


def _declared_name(code, start: int, semi: int) -> str:
    """The variable name of a `groupshared <type> <name>[..];` declaration: the identifier before
    the array subscript or the terminating `;`."""
    for cursor in range(start, semi):
        if code[cursor].type == L.Identifier and (
            code[cursor + 1].type in (L.LeftBracket, L.Semi)
        ):
            return code[cursor].text
    return ""


def _close_struct(code, close: int, rewriter: Rewriter, srg_name: str, emit_parameter_block: bool) -> None:
    """Terminate the struct and declare its ParameterBlock instance.

    AZSL's SRG block may or may not already end with a semicolon; both forms appear in the tree.
    """
    trailing_semi = close + 1 < len(code) and code[close + 1].type == L.Semi
    last_token = code[close + 1] if trailing_semi else code[close]

    suffix = ";"
    if emit_parameter_block:
        suffix += f"\nParameterBlock<{layout_type_name(srg_name)}> {srg_name};"
    rewriter.replace_span(code[close], last_token, "}" + suffix)


def _requalify_member_access(code, registry: Registry, rewriter: Rewriter, hoisted_members: dict) -> None:
    """Rule 2: `PassSrg::m_x` -> `PassSrg.m_x`, only for names known to be SRGs.

    Enum and namespace scoping (`ColorSpaceId::ACEScg`, `Bindless::GetTexture2D`) is valid Slang
    and is deliberately left alone — verified against slangc. Members hoisted out of the SRG
    (groupshared) become globals, so their qualifier is removed entirely rather than turned into `.`.
    """
    for index in range(len(code) - 1):
        token = code[index]
        if token.type != L.Identifier or not registry.is_srg(token.text):
            continue
        if token.text in NAMESPACE_BACKED_SRGS:
            continue
        separator = code[index + 1]
        if separator.type != L.ColonColon:
            continue

        hoisted = hoisted_members.get(token.text)
        member = code[index + 2] if index + 2 < len(code) else None
        if hoisted and member is not None and member.text in hoisted:
            # `PassSrg::smColor` -> bare `smColor` (it is now a global).
            rewriter.delete_span(token, separator)
            continue

        # A partial SRG's members are reached through the aggregate instance under the same name,
        # so the rewrite is identical either way.
        rewriter.replace_token(separator, ".")


def _requalify_member_access_in_macros(lexed, registry: Registry, rewriter: Rewriter) -> None:
    """Rule 2, inside `#define` bodies.

    The token-level pass only sees default-channel tokens, but a macro like
    `#define S(t) t.Sample(PassSrg::PointSampler)` expands into code where the SRG qualifier must be
    `.` -- left as `::`, the expansion is a scope-resolution on a ParameterBlock instance and fails
    (E30100). Preprocessor directives lex as one whole-line token, so this rewrites the define text
    directly for every registered SRG name (namespace-backed SRGs keep `::`).
    """
    srg_names = [name for name in registry.srgs if name not in NAMESPACE_BACKED_SRGS]
    if not srg_names:
        return
    patterns = [(re.compile(rf"(?<![A-Za-z0-9_]){re.escape(name)}::"), name + ".") for name in srg_names]

    for token in lexed.tokens:
        if token.channel != CHANNEL_PREPROCESSOR or "::" not in token.text:
            continue
        stripped = token.text.lstrip()
        if not stripped.startswith("#") or not stripped[1:].lstrip().startswith("define"):
            continue
        new_text = token.text
        for pattern, replacement in patterns:
            new_text = pattern.sub(replacement, new_text)
        if new_text != token.text:
            rewriter.replace_token(token, new_text)


def _next_of_type(code, start: int, token_type: int) -> int:
    for cursor in range(start, len(code)):
        if code[cursor].type == token_type:
            return cursor
    return -1
