# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Type-declaration rules (12).

AZSL uses `class` for the struct-with-methods idiom the PBR material code is built on. Slang has
`class` too, but with reference semantics, so ports map it to `struct` — value semantics, matching
what AZSL meant.

AZSL also allows methods to be *defined* outside the class body (`float4 Shadow::GetJitter(...)`,
the C++ spelling). Slang has no out-of-line member definition syntax; the equivalent is an
`extension` block, which is verified to work and to accumulate across several blocks per type.
"""

from __future__ import annotations

from ..lexing import find_matching_brace
from ..rewrite import Rewriter, Severity

from azslLexer import azslLexer as L  # noqa: N814

# Tokens that can precede a return type at file scope; used to find where a definition starts.
_STATEMENT_TERMINATORS = (L.Semi, L.RightBrace, L.LeftBrace)

# Assignment operators: an identifier followed by one of these (past any subscript/member chain) is
# being written, which is what forces `[mutating]` when the identifier is a struct field.
_ASSIGNMENT_TOKENS = (
    L.Assign, L.StarAssign, L.DivAssign, L.ModAssign, L.PlusAssign, L.MinusAssign,
    L.LeftShiftAssign, L.RightShiftAssign, L.AndAssign, L.XorAssign, L.OrAssign,
)


def apply(lexed, registry, rewriter: Rewriter) -> None:
    _convert_class_keyword(lexed.code_tokens, rewriter)
    _normalize_interpolation_modifiers(lexed.code_tokens, rewriter)
    _add_mutating_attributes(lexed.code_tokens, rewriter)


def _add_mutating_attributes(code, rewriter: Rewriter) -> None:
    """Mark struct methods that write a member with `[mutating]`.

    Slang structs are value types and their methods are non-mutating by default, so assigning to a
    member (`m_x = ...`) is an error (E30011) without `[mutating]`. AZSL/HLSL require no such keyword.
    A method needs it if it directly assigns a field, or transitively calls a sibling that does.
    """
    for open_brace, close_brace in _iter_struct_bodies(code):
        fields = _field_names(code, open_brace, close_brace)
        methods = _methods(code, open_brace, close_brace)
        if not methods:
            continue

        mutating: set[str] = set()
        for method in methods:
            if _assigns_a_field(code, method, fields):
                mutating.add(method.name)

        # A method that calls a mutating sibling also mutates; iterate to a fixpoint.
        changed = True
        while changed:
            changed = False
            for method in methods:
                if method.name not in mutating and _calls_any(code, method, mutating):
                    mutating.add(method.name)
                    changed = True

        for method in methods:
            if method.name in mutating and not _already_mutating(code, method):
                rewriter.insert_before(code[method.decl_start], "[mutating] ")


class _Method:
    __slots__ = ("name", "decl_start", "body_open", "body_close")

    def __init__(self, name, decl_start, body_open, body_close):
        self.name = name
        self.decl_start = decl_start
        self.body_open = body_open
        self.body_close = body_close


def _iter_struct_bodies(code):
    """Yield (open-brace index, close-brace index) for every `struct/class Name { ... }` body."""
    for index in range(len(code) - 2):
        if code[index].type not in (L.Struct, L.Class):
            continue
        if code[index + 1].type != L.Identifier:
            continue
        brace = index + 2
        while brace < len(code) and code[brace].type not in (L.LeftBrace, L.Semi):
            brace += 1
        if brace >= len(code) or code[brace].type != L.LeftBrace:
            continue
        close = find_matching_brace(code, brace)
        if close > brace:
            yield brace, close


def _field_names(code, open_brace: int, close_brace: int) -> set[str]:
    """Member variable names declared directly in the struct body (not nested, not methods)."""
    names: set[str] = set()
    depth = 0
    statement_start = open_brace + 1
    for cursor in range(open_brace + 1, close_brace):
        token_type = code[cursor].type
        if token_type == L.LeftBrace:
            depth += 1
        elif token_type == L.RightBrace:
            depth -= 1
        elif token_type == L.LeftParen and depth == 0:
            # A method declaration, not a field; skip to its terminator.
            continue
        elif token_type == L.Semi and depth == 0:
            tokens = code[statement_start:cursor]
            if not any(t.type == L.LeftParen for t in tokens):
                name = _declared_name(tokens)
                if name:
                    names.add(name)
            statement_start = cursor + 1
        elif depth == 0 and token_type == L.RightBrace:
            statement_start = cursor + 1
    return names


def _declared_name(tokens) -> str:
    """Trailing `Type name[..];` -> name (identifier before `[` or end)."""
    for position, token in enumerate(tokens):
        if token.type == L.Identifier and (
            position + 1 >= len(tokens) or tokens[position + 1].type in (L.LeftBracket, L.Semi)
        ):
            last = token.text
    return locals().get("last", "") if tokens else ""


def _methods(code, open_brace: int, close_brace: int) -> list:
    """Methods declared directly in the struct body: `Ret Name(params) { body }`."""
    methods = []
    depth = 0
    statement_start = open_brace + 1
    cursor = open_brace + 1
    while cursor < close_brace:
        token_type = code[cursor].type
        if token_type == L.LeftBrace:
            if depth == 0:
                # A method body: its name is the identifier before the parameter list.
                name = _name_before_params(code, statement_start, cursor)
                body_close = find_matching_brace(code, cursor)
                if name and body_close > cursor:
                    methods.append(_Method(name, statement_start, cursor, body_close))
                    cursor = body_close + 1
                    statement_start = cursor
                    continue
            depth += 1
        elif token_type == L.RightBrace:
            depth -= 1
        elif token_type == L.Semi and depth == 0:
            statement_start = cursor + 1
        cursor += 1
    return methods


def _name_before_params(code, start: int, brace: int) -> str:
    """In `Ret Name(params) {`, the identifier immediately before the `(`."""
    paren = -1
    for cursor in range(start, brace):
        if code[cursor].type == L.LeftParen:
            paren = cursor
            break
    if paren <= start or code[paren - 1].type != L.Identifier:
        return ""
    return code[paren - 1].text


def _assigns_a_field(code, method, fields: set[str]) -> bool:
    """True if the method body assigns to one of `fields` (`m_x =`, `m_x +=`, `this.m_x =`, `m_x[i] =`)."""
    for cursor in range(method.body_open + 1, method.body_close):
        token = code[cursor]
        if token.type != L.Identifier or token.text not in fields:
            continue
        # `obj.field = ` writes some other object's like-named field, not our member — skip it.
        # (`this.field` isn't a case here: AZSL has no `this` keyword.)
        if cursor > 0 and code[cursor - 1].type == L.Dot:
            continue
        following = _skip_subscripts(code, cursor + 1, method.body_close)
        if following < method.body_close and code[following].type in _ASSIGNMENT_TOKENS:
            return True
    return False


def _skip_subscripts(code, cursor: int, limit: int) -> int:
    """Advance past `[...]`, `.member` and `.member[...]` chains after a base identifier."""
    while cursor < limit:
        if code[cursor].type == L.LeftBracket:
            depth = 0
            while cursor < limit:
                if code[cursor].type == L.LeftBracket:
                    depth += 1
                elif code[cursor].type == L.RightBracket:
                    depth -= 1
                    if depth == 0:
                        cursor += 1
                        break
                cursor += 1
        elif code[cursor].type == L.Dot and cursor + 1 < limit and code[cursor + 1].type == L.Identifier:
            cursor += 2
        else:
            break
    return cursor


def _calls_any(code, method, names: set[str]) -> bool:
    """True if the method body calls one of `names` as `Name(`."""
    for cursor in range(method.body_open + 1, method.body_close - 1):
        token = code[cursor]
        if (
            token.type == L.Identifier
            and token.text in names
            and code[cursor + 1].type == L.LeftParen
            and not (cursor > 0 and code[cursor - 1].type in (L.Dot, L.ColonColon))
        ):
            return True
    return False


def _already_mutating(code, method) -> bool:
    """Whether a `[mutating]` (or `mutating`) already precedes the method's return type."""
    for cursor in range(max(0, method.decl_start - 4), method.decl_start + 1):
        if code[cursor].type == L.Identifier and code[cursor].text == "mutating":
            return True
    return False


def _normalize_interpolation_modifiers(code, rewriter: Rewriter) -> None:
    """Drop `linear` where it sits next to `centroid`.

    Slang treats `centroid` as implying `linear` interpolation, so `linear centroid` (valid in AZSL)
    is a duplicate modifier (E31202). Removing the redundant `linear` keeps the meaning.
    """
    for index, token in enumerate(code):
        if token.type != L.Linear:
            continue
        before_centroid = index > 0 and code[index - 1].type == L.Centroid
        after_centroid = index + 1 < len(code) and code[index + 1].type == L.Centroid
        if before_centroid or after_centroid:
            rewriter.delete_span(token, token)


def apply_prepass(lexed, registry, rewriter: Rewriter) -> None:
    """Structural moves, run before every other rule against a freshly lexed file.

    Relocating a definition and editing its contents cannot share one rewrite pass: the other
    rules stage edits inside the body (an option use site becoming a call, an SRG qualifier
    becoming a member access), which would both collide with the move and be dropped from the
    relocated copy. So the move happens first, the result is re-lexed, and the remaining rules
    then see the code in its final position.
    """
    _convert_out_of_line_definitions(lexed.code_tokens, registry, rewriter)


def _convert_class_keyword(code, rewriter: Rewriter) -> None:
    for index, token in enumerate(code):
        if token.type != L.Class:
            continue
        # `enum class` is a single declaration; its `class` is not a type keyword to rewrite, and
        # the option rules own the `option enum class` form outright.
        if index > 0 and code[index - 1].type == L.Enum:
            continue

        rewriter.replace_token(token, "struct")

        if _has_base_list(code, index):
            base = code[index + 3].text if index + 3 < len(code) else "?"
            rewriter.note(
                Severity.TODO,
                f"class -> struct with inheritance (: {base});"
                " Slang warns E30816 that inheritance is unstable and will be removed",
                token.line,
            )


def _has_base_list(code, index: int) -> bool:
    """True for `class Name : Base`."""
    return (
        index + 2 < len(code)
        and code[index + 1].type == L.Identifier
        and code[index + 2].type == L.Colon
    )


def _convert_out_of_line_definitions(code, registry, rewriter: Rewriter) -> None:
    """Wrap `Ret Type::Method(...) { ... }` at file scope in `extension Type { ... }`."""
    # Overloaded methods share a name, so each definition must consume a different prototype;
    # without this every overload would delete the same declaration and the edits would collide.
    claimed_prototypes: set[int] = set()
    depth = 0
    for index in range(len(code) - 3):
        token_type = code[index].type
        if token_type == L.LeftBrace:
            depth += 1
            continue
        if token_type == L.RightBrace:
            depth -= 1
            continue
        if depth != 0:
            continue

        if not (
            code[index].type == L.Identifier
            and code[index + 1].type == L.ColonColon
            and code[index + 2].type == L.Identifier
            and code[index + 3].type == L.LeftParen
        ):
            continue

        type_name = code[index].text
        # SRG-qualified access is the other rule's business and never appears at file scope as a
        # definition; skip it defensively so the two can never both claim these tokens.
        if registry.is_srg(type_name):
            continue

        body_open = _matching_paren_end(code, index + 3)
        if body_open < 0:
            continue
        body_open += 1
        if body_open >= len(code) or code[body_open].type != L.LeftBrace:
            continue  # a declaration, not a definition
        body_close = find_matching_brace(code, body_open)
        if body_close < 0:
            continue

        start = _statement_start(code, index)
        if start < 0:
            continue

        method_name = code[index + 2].text
        prototype = _find_prototype(code, type_name, method_name, claimed_prototypes)

        if prototype is None:
            # The class lives in another file (one case in the tree), so the body cannot be moved
            # into it. An extension carries the definition instead.
            rewriter.insert_before(code[start], f"extension {type_name}\n{{\n")
            rewriter.delete_span(code[index], code[index + 1])
            rewriter.insert_after(code[body_close], "\n}")
            rewriter.note(
                Severity.TODO,
                f"{type_name}::{method_name} defined out of line but {type_name} is not declared"
                " in this file; wrapped in an extension instead of moving it into the struct",
                code[index].line,
            )
            continue

        # AZSL declares the method in the class and defines it below. Slang has no out-of-line
        # member syntax, so the definition moves up to replace its prototype, keeping `static`.
        prototype_start, prototype_end, is_static = prototype
        claimed_prototypes.add(prototype_start)

        definition = _definition_text(
            rewriter.text, code, start, index, body_close, is_static
        )
        rewriter.replace_span(code[prototype_start], code[prototype_end], definition)
        rewriter.delete_span(code[start], code[body_close])


def _definition_text(
    text: str, code, start: int, qualified_index: int, body_close: int, is_static: bool
) -> str:
    """Render an out-of-line definition as an in-struct member.

    The original source is reused verbatim apart from dropping the `Type::` qualifier, so comments
    and formatting inside the body survive; the whole block is then indented one level to sit
    correctly among the struct's members.
    """
    head = text[code[start].start : code[qualified_index].start]
    tail = text[code[qualified_index + 2].start : code[body_close].stop + 1]
    body = (head + tail).strip()

    if is_static:
        body = "static " + body

    indented = "\n".join(
        ("    " + line if line.strip() else line) for line in body.split("\n")
    )
    return indented.lstrip()


def _find_prototype(code, type_name: str, method_name: str, claimed: set[int]):
    """Locate an unclaimed `[static] Ret method(...);` inside `struct/class <type_name> { ... }`.

    Returns (start index, end index, is_static) or None.
    """
    body_open = -1
    for index in range(len(code) - 1):
        if code[index].type not in (L.Struct, L.Class):
            continue
        if code[index + 1].type != L.Identifier or code[index + 1].text != type_name:
            continue
        for cursor in range(index + 2, min(index + 8, len(code))):
            if code[cursor].type == L.LeftBrace:
                body_open = cursor
                break
        if body_open >= 0:
            break

    if body_open < 0:
        return None
    body_close = find_matching_brace(code, body_open)
    if body_close < 0:
        return None

    statement_start = body_open + 1
    depth = 0
    for cursor in range(body_open + 1, body_close):
        token_type = code[cursor].type
        if token_type == L.LeftBrace:
            depth += 1
        elif token_type == L.RightBrace:
            depth -= 1
        elif token_type == L.Semi and depth == 0:
            tokens = code[statement_start : cursor + 1]
            if statement_start not in claimed and _is_prototype_of(tokens, method_name):
                is_static = any(token.type == L.Static for token in tokens)
                return statement_start, cursor, is_static
            statement_start = cursor + 1
    return None


def _is_prototype_of(tokens, method_name: str) -> bool:
    """True for a bodyless `Ret name(...);` declaring `method_name`."""
    for position, token in enumerate(tokens):
        if (
            token.type == L.Identifier
            and token.text == method_name
            and position + 1 < len(tokens)
            and tokens[position + 1].type == L.LeftParen
        ):
            return not any(inner.type == L.LeftBrace for inner in tokens)
    return False


def _statement_start(code, index: int) -> int:
    """Index of the first token of the definition (its return type)."""
    for cursor in range(index - 1, -1, -1):
        if code[cursor].type in _STATEMENT_TERMINATORS:
            return cursor + 1
    return 0


def _matching_paren_end(code, open_index: int) -> int:
    depth = 0
    for cursor in range(open_index, len(code)):
        if code[cursor].type == L.LeftParen:
            depth += 1
        elif code[cursor].type == L.RightParen:
            depth -= 1
            if depth == 0:
                return cursor
    return -1
