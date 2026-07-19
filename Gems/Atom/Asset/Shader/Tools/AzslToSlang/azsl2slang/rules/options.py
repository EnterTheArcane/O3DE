# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Shader option rules (3, 4, 5, 6).

AZSL declares options as variables with an `option` storage flag; Slang declares them as
`[AtomOption]`-attributed extern functions that the builder implements per lowering mode. The
declaration shape changes and, because the option becomes a function, every use site becomes a
call. `option enum class E {...} o_x;` additionally splits into a standalone enum plus the option.
"""

from __future__ import annotations

from ..lexing import find_matching_brace
from ..registry import Registry
from ..rewrite import Rewriter, Severity

from azslLexer import azslLexer as L  # noqa: N814

# Identifiers that may legitimately precede an option name without it being a use site.
_QUALIFIERS = (L.Dot, L.ColonColon)


def apply(lexed, registry: Registry, rewriter: Rewriter) -> None:
    code = lexed.code_tokens
    declaration_name_indices: set[int] = set()

    index = 0
    while index < len(code):
        if code[index].type == L.Option:
            index = _convert_declaration(code, index, rewriter, declaration_name_indices)
        else:
            index += 1

    _convert_use_sites(code, registry, rewriter, declaration_name_indices)


def _convert_declaration(code, index: int, rewriter: Rewriter, declared: set[int]) -> int:
    """Rewrite one `option ...;` declaration, returning the index just past it."""
    end = _next_of_type(code, index, L.Semi)
    if end < 0:
        rewriter.note(Severity.TODO, "option declaration is not semicolon-terminated", code[index].line)
        return index + 1

    attribute_tokens = _preceding_range_attribute(code, index)
    range_args = _range_arguments(code, attribute_tokens)

    if code[index + 1].type == L.Enum:
        return _convert_enum_declaration(code, index, end, rewriter, declared, range_args, attribute_tokens)

    # `option <type tokens...> o_name [= default];`
    name_index = _option_name_index(code, index + 1, end)
    if name_index < 0:
        rewriter.note(Severity.TODO, "could not identify option name", code[index].line)
        return end + 1
    declared.add(name_index)

    type_text = "".join(token.text for token in code[index + 1 : name_index]).strip()
    name = code[name_index].text
    default_text = _default_text(code, name_index, end)

    first_token = attribute_tokens[0] if attribute_tokens else code[index]
    rewriter.replace_span(
        first_token,
        code[end],
        _option_declaration(name, type_text, default_text, range_args),
    )
    return end + 1


def _convert_enum_declaration(
    code, index: int, end: int, rewriter: Rewriter, declared: set[int], range_args, attribute_tokens
) -> int:
    """Rule 4: `option enum class E { A, B } o_x = E::B;` -> `public enum E {...}` + the option."""
    cursor = index + 1  # at `enum`
    cursor += 1
    if cursor < len(code) and code[cursor].type == L.Class:
        cursor += 1

    enum_name = None
    if cursor < len(code) and code[cursor].type == L.Identifier:
        enum_name = code[cursor].text
        cursor += 1

    brace = _next_of_type(code, cursor, L.LeftBrace)
    close = find_matching_brace(code, brace) if brace >= 0 else -1
    if enum_name is None or close < 0:
        rewriter.note(Severity.TODO, "malformed `option enum` declaration", code[index].line)
        return end + 1

    cases = [token.text for token in code[brace + 1 : close] if token.type == L.Identifier]

    name_index = _option_name_index(code, close + 1, end)
    if name_index < 0:
        rewriter.note(Severity.TODO, "could not identify option name", code[index].line)
        return end + 1
    declared.add(name_index)

    name = code[name_index].text
    default_text = _default_text(code, name_index, end)
    if default_text is None and cases:
        # AZSL defaults an uninitialized enum option to its first case; make that explicit so the
        # builder does not have to infer it.
        default_text = f"{enum_name}::{cases[0]}"

    body = "\n".join(f"    {case}," for case in cases)
    enum_text = f"public enum {enum_name}\n{{\n{body}\n}};\n\n"

    first_token = attribute_tokens[0] if attribute_tokens else code[index]
    rewriter.replace_span(
        first_token,
        code[end],
        enum_text + _option_declaration(name, enum_name, default_text, range_args),
    )
    return end + 1


def _option_declaration(name: str, type_text: str, default_text: str | None, range_args) -> str:
    """Build the `[AtomOption(...)] public extern T name();` form."""
    attributes = []
    if default_text:
        # Enum defaults are written `E::Case` in AZSL; attribute arguments use the dot form.
        attributes.append(f"AtomOption({default_text.replace('::', '.')})")
    else:
        attributes.append("AtomOption")
    if range_args:
        attributes.append(f"AtomRange({range_args[0]}, {range_args[1]})")

    attribute_text = ", ".join(attributes)
    return f"[{attribute_text}]\npublic extern {type_text} {name}();"


def _convert_use_sites(code, registry: Registry, rewriter: Rewriter, declared: set[int]) -> None:
    """Rule 6: a bare reference to an option becomes a call.

    Guarded so member names and existing calls are not touched: the identifier must not be
    qualified by `.`/`::`, must not already be followed by `(`, and must not be the name in the
    declaration this pass just rewrote.
    """
    for index, token in enumerate(code):
        if token.type != L.Identifier or index in declared:
            continue
        if token.text not in registry.options:
            continue
        if index > 0 and code[index - 1].type in _QUALIFIERS:
            continue
        if index + 1 < len(code) and code[index + 1].type == L.LeftParen:
            continue
        rewriter.insert_after(token, "()")


def _option_name_index(code, start: int, end: int) -> int:
    """The option's own name: the last identifier before `=` or the terminating `;`."""
    limit = end
    for cursor in range(start, end):
        if code[cursor].type == L.Assign:
            limit = cursor
            break
    for cursor in range(limit - 1, start - 1, -1):
        if code[cursor].type == L.Identifier:
            return cursor
    return -1


def _default_text(code, name_index: int, end: int) -> str | None:
    for cursor in range(name_index, end):
        if code[cursor].type == L.Assign:
            return "".join(token.text for token in code[cursor + 1 : end]).strip()
    return None


def _preceding_range_attribute(code, index: int) -> list:
    """Tokens of a `[[range(min, max)]]` specifier immediately preceding the option, if any."""
    cursor = index - 1
    if cursor < 1 or code[cursor].type != L.RightBracket:
        return []
    depth = 0
    while cursor >= 0:
        if code[cursor].type == L.RightBracket:
            depth += 1
        elif code[cursor].type == L.LeftDoubleBracket:
            depth -= 2
            if depth <= 0:
                return code[cursor:index]
        elif code[cursor].type == L.LeftBracket:
            depth -= 1
            if depth <= 0:
                return code[cursor:index]
        cursor -= 1
    return []


def _range_arguments(code, attribute_tokens) -> tuple[str, str] | None:
    if not attribute_tokens:
        return None
    texts = [token.text for token in attribute_tokens]
    if "range" not in texts:
        return None
    numbers = [
        token.text
        for token in attribute_tokens
        if token.type in (L.IntegerLiteral, L.FloatLiteral)
    ]
    if len(numbers) >= 2:
        return numbers[0], numbers[1]
    return None


def _next_of_type(code, start: int, token_type: int) -> int:
    for cursor in range(start, len(code)):
        if code[cursor].type == token_type:
            return cursor
    return -1
