# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Constant rule: object-like `#define` value macros become `public static const`.

Slang modules cannot export `#define`s, so value constants (`#define INV_4PI 0.0795`) are promoted to
`public static const float INV_4PI = 0.0795;` — importable, typed, and scoped. Slang requires an
explicit type on a `static const`, so the type is inferred from the value.

Left as `#define` (a module still has a working preprocessor internally):
  * macros read by any `#if`/`#ifdef` (registry.conditional_macros) — a const can't gate the preprocessor;
  * function-like macros (handled by the macro rule);
  * empty / non-value bodies, and bodies that reference resources or unknown identifiers whose type
    cannot be inferred (flagged, kept).
"""

from __future__ import annotations

import re

from ..lexing import CHANNEL_PREPROCESSOR
from ..rewrite import Rewriter, Severity

# `#define NAME body` with NAME not followed by `(` (object-like, not function-like).
_DEFINE = re.compile(r"^(?P<lead>\s*\#\s*define\s+)(?P<name>[A-Za-z_]\w*)(?P<gap>[ \t]+)(?P<body>.*\S)\s*$")

_INT = re.compile(r"^[+-]?\d+$")
_UINT = re.compile(r"^\d+[uU]$")
_HEX = re.compile(r"^0[xX][0-9a-fA-F]+[uU]?$")
_FLOAT = re.compile(r"^[+-]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fF]?$")
# A typed vector/scalar constructor: float3(...), real4(...), int2(...), etc. -> the constructor type.
_CTOR = re.compile(r"^(?P<type>[a-zA-Z_]\w*[0-9](?:x[0-9])?|float|half|int|uint|bool|real)\s*\(")
# A bare type name (`float3`, `half4x4`, `uint`), i.e. a `#define`d type alias.
_TYPE_NAME = re.compile(r"^(?:float|half|double|int|uint|bool|real)(?:[1-4](?:x[1-4])?)?$")


def apply(lexed, registry, rewriter: Rewriter) -> None:
    for token in lexed.tokens:
        if token.channel != CHANNEL_PREPROCESSOR:
            continue
        match = _DEFINE.match(token.text)
        if not match:
            continue

        name = match.group("name")
        if name in registry.conditional_macros:
            continue  # a preprocessor toggle — must stay a #define

        # The preprocessor token is the whole line, so a trailing `//` or `/* */` comment is part of
        # the body. Split it off for typing, but preserve it after the emitted `;`.
        value, comment = _split_comment(match.group("body"))
        if not value:
            continue  # comment-only or empty body

        # A `#define`d type alias (`#define real3 float3`) becomes a `typealias`, not a const.
        if _TYPE_NAME.match(value):
            trailer = f" {comment}" if comment else ""
            rewriter.replace_token(token, f"public typealias {name} = {value};{trailer}")
            continue

        type_name = _infer_type(value, registry.const_types)
        if type_name is None:
            # A value we can't type (resource alias, cross-macro expression); leave it and flag.
            rewriter.note(
                Severity.TODO,
                f"#define {name} kept: cannot infer a type for `{value}` to make it a static const",
                token.line,
            )
            continue

        trailer = f" {comment}" if comment else ""
        rewriter.replace_token(token, f"public static const {type_name} {name} = {value};{trailer}")


def _split_comment(body: str) -> tuple[str, str]:
    """Split a `#define` body into (value, trailing comment)."""
    line_comment = re.search(r"//.*$", body)
    if line_comment:
        return body[: line_comment.start()].strip(), line_comment.group(0).strip()
    block_comment = re.search(r"/\*.*?\*/\s*$", body)
    if block_comment:
        return body[: block_comment.start()].strip(), block_comment.group(0).strip()
    return body.strip(), ""


def _infer_type(body: str, consts: dict[str, str] | None = None) -> str | None:
    """Infer the Slang type of a constant's value expression, or None if it can't be typed.

    `consts` maps already-typed constant names to their Slang type, so a value that references another
    converted constant (`(NVLC_MAX_BINS - 1)`) resolves instead of being left as a `#define` — which
    matters because a `#define` cannot cross a module boundary but a `static const` can.
    """
    consts = consts or {}
    text = body.strip()
    # Parenthesized expression: infer from the inside.
    if text.startswith("(") and text.endswith(")"):
        return _infer_type(text[1:-1], consts)

    if _HEX.match(text) or _UINT.match(text):
        return "uint"
    if _INT.match(text):
        return "int"
    if _FLOAT.match(text):
        return "float"
    if text in ("true", "false"):
        return "bool"
    if text in consts:
        return consts[text]  # a bare reference to another converted constant

    ctor = _CTOR.match(text)
    if ctor:
        return ctor.group("type")

    # Arithmetic/bitwise expression over literals and known constants -> type from its operands.
    if re.fullmatch(r"[-+*/%()<>&|^~\s\w.]+", text) and any(op in text for op in "+-*/%<>&|^~"):
        operands = re.findall(r"0[xX][0-9a-fA-F]+[uU]?|\d+\.?\d*[fFuU]?|[A-Za-z_]\w*", text)
        types = []
        for operand in operands:
            operand_type = _operand_type(operand, consts)
            if operand_type is None:
                return None  # an unresolved operand (unknown identifier) -> can't type the expression
            types.append(operand_type)
        if not types:
            return None
        if "float" in types:
            return "float"
        if all(t == "uint" for t in types):
            return "uint"
        return "int"  # all int, or an int/uint mix -> int
    return None


def _operand_type(operand: str, consts: dict[str, str]) -> str | None:
    """Type of a single operand in a constant expression, or None if it cannot be resolved."""
    if _HEX.match(operand) or _UINT.match(operand):
        return "uint"
    if re.fullmatch(r"\d+", operand):
        return "int"
    if re.fullmatch(r"\d+\.\d*[fF]?|\.\d+[fF]?|\d+[fF]|\d+\.?\d*[eE][+-]?\d+[fF]?", operand):
        return "float"
    if re.fullmatch(r"[A-Za-z_]\w*", operand):
        return consts.get(operand)  # None when it is not a known constant
    return None
