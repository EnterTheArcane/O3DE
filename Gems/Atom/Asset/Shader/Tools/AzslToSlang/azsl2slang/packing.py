# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Constant-buffer offset arithmetic behind the `[[pad_to(N)]]` rule.

AZSLC implements `pad_to` as "grow this struct to N bytes"; Slang has no equivalent, so the port
needs an explicit trailing member of exactly the right size. That means replaying HLSL's constant
buffer packing over the members declared before the attribute.
"""

from __future__ import annotations

import re

from azslLexer import azslLexer as L  # noqa: N814


class PackingError(Exception):
    """The layout could not be determined, so no padding member is emitted."""


_SCALAR_SIZES = {
    "bool": 4,
    "int": 4,
    "uint": 4,
    "dword": 4,
    "float": 4,
    "half": 4,  # promoted to 32-bit in constant buffers unless 16-bit types are enabled
    "double": 8,
}

_VECTOR = re.compile(r"^(?P<scalar>bool|int|uint|dword|float|half|double)(?P<count>[1-4])$")
_MATRIX = re.compile(
    r"^(?P<scalar>bool|int|uint|dword|float|half|double)(?P<rows>[1-4])x(?P<cols>[1-4])$"
)

_PAD_SCALAR = {4: "float", 8: "float2", 12: "float3", 16: "float4"}


def padding_member_for(code, attribute_index: int, alignment: int) -> str | None:
    """The declaration to write in place of `[[pad_to(N)]]`, or None when already aligned.

    `pad_to(N)` grows the struct so its size is a multiple of N — not so it equals N. Vignette's
    `uint2 + float2 + float` occupies 20 bytes, so `pad_to(16)` rounds to 32 and needs 12 bytes of
    padding, which is the `float3 m_pad` the hand-written port declares.
    """
    if alignment <= 0:
        raise PackingError(f"invalid alignment {alignment}")

    members = _members_before(code, attribute_index)
    offset = 0
    for type_text, array_length in members:
        offset = _place(offset, type_text, array_length)

    remaining = _round_up(offset, alignment) - offset
    if remaining == 0:
        return None

    name = "m_pad"
    if remaining in _PAD_SCALAR:
        return f"{_PAD_SCALAR[remaining]} {name};"
    if remaining % 4 == 0:
        return f"float {name}[{remaining // 4}];"
    raise PackingError(f"{remaining} trailing bytes is not a multiple of 4")


def _members_before(code, attribute_index: int) -> list[tuple[str, int | None]]:
    """(type text, array length) for each member declared in the struct holding the attribute."""
    open_brace = _enclosing_open_brace(code, attribute_index)
    if open_brace < 0:
        raise PackingError("no enclosing struct found")

    members: list[tuple[str, int | None]] = []
    cursor = open_brace + 1
    depth = 0
    statement_start = cursor

    while cursor < attribute_index:
        token_type = code[cursor].type
        if token_type == L.LeftBrace:
            depth += 1
        elif token_type == L.RightBrace:
            depth -= 1
        elif token_type == L.Semi and depth == 0:
            member = _parse_member(code, statement_start, cursor)
            if member is not None:
                members.append(member)
            statement_start = cursor + 1
        cursor += 1

    return members


def _enclosing_open_brace(code, index: int) -> int:
    """Index of the innermost `{` still open at `index` — the struct the attribute sits in."""
    depth = 0
    for cursor in range(index - 1, -1, -1):
        token_type = code[cursor].type
        if token_type == L.RightBrace:
            depth += 1
        elif token_type == L.LeftBrace:
            if depth == 0:
                return cursor
            depth -= 1
    return -1


def _parse_member(code, start: int, semi: int) -> tuple[str, int | None] | None:
    """Extract the type and array length of one `Type m_name[N];` declaration."""
    tokens = [token for token in code[start:semi]]
    if len(tokens) < 2:
        return None

    array_length = None
    if tokens[-1].type == L.RightBracket:
        for cursor in range(len(tokens) - 1, -1, -1):
            if tokens[cursor].type == L.LeftBracket:
                inner = tokens[cursor + 1 : len(tokens) - 1]
                if len(inner) == 1 and inner[0].type == L.IntegerLiteral:
                    array_length = int(inner[0].text, 0)
                else:
                    raise PackingError("non-literal array length")
                tokens = tokens[:cursor]
                break

    if len(tokens) < 2:
        return None
    # The member name is the final identifier; everything before it describes the type.
    type_text = "".join(token.text for token in tokens[:-1]).strip()
    if not type_text:
        return None
    return type_text, array_length


def _place(offset: int, type_text: str, array_length: int | None) -> int:
    """Advance the running offset past one member, honouring HLSL's 16-byte row rules."""
    size, alignment = _size_and_alignment(type_text)

    if array_length is not None:
        # Array elements each start on a new 16-byte row.
        stride = _round_up(size, 16)
        offset = _round_up(offset, 16)
        return offset + stride * array_length

    offset = _round_up(offset, alignment)
    # A vector may not straddle a 16-byte boundary.
    if size <= 16 and (offset % 16) + size > 16:
        offset = _round_up(offset, 16)
    return offset + size


def _size_and_alignment(type_text: str) -> tuple[int, int]:
    normalized = type_text.replace("const", "").replace("row_major", "").replace(
        "column_major", ""
    ).strip()

    if normalized in _SCALAR_SIZES:
        size = _SCALAR_SIZES[normalized]
        return size, size

    vector = _VECTOR.match(normalized)
    if vector:
        scalar = _SCALAR_SIZES[vector.group("scalar")]
        return scalar * int(vector.group("count")), scalar

    matrix = _MATRIX.match(normalized)
    if matrix:
        scalar = _SCALAR_SIZES[matrix.group("scalar")]
        rows = int(matrix.group("rows"))
        # Each row occupies a full 16-byte register.
        return _round_up(scalar * int(matrix.group("cols")), 16) * rows, 16

    raise PackingError(f"unknown member type {type_text!r}")


def _round_up(value: int, multiple: int) -> int:
    remainder = value % multiple
    return value if remainder == 0 else value + (multiple - remainder)
