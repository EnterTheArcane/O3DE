# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Access-control rule: mark declarations `public` so modules can import them.

Slang declarations default to `internal` visibility — invisible across module boundaries — and this
applies to struct *fields* too (a `public struct` with a plain field still can't have that field read
from another module). Since the port turns each file into a module, every symbol another file uses must
be `public`. Rather than a cross-file usage analysis, this marks *everything* public: over-exposing is
harmless in a shader library, and it sidesteps under-marking (which fails the build).

The pass is scope-aware: it publicizes declarations only in *declaration scopes* (file scope and
struct/enum bodies), never local variables inside a function body.
"""

from __future__ import annotations

from ..rewrite import Rewriter

from azslLexer import azslLexer as L  # noqa: N814

_DECL_SCOPE = "decl"  # file scope, struct/class body — members are declarations
_STMT_SCOPE = "stmt"  # function body, control-flow block, initializer — statements, not declarations
_ENUM_SCOPE = "enum"  # enum body — members are cases, not publicizable declarations

_VISIBILITY_WORDS = {"public", "internal", "private"}
# Module directives are statements, not publicizable declarations.
_NON_DECL_WORDS = {"import", "__exported", "__include", "module", "implementing"}
# Tokens that open a matched pair; used to skip over them when scanning backwards for a brace's context.
_CLOSERS = (L.RightParen, L.RightBracket, L.Greater)
_OPENERS = (L.LeftParen, L.LeftBracket, L.Less)


def apply(lexed, registry, rewriter: Rewriter) -> None:
    # Only library modules (headers, `.azsli`/`.srgi`) are imported by other files and need a public
    # API. An entry shader (`.azsl`) is a leaf nobody imports, so its declarations stay `internal`
    # (the default) — publicizing them would advertise an API no one consumes.
    if lexed.path.suffix == ".azsl":
        return

    code = lexed.code_tokens
    scope = [_DECL_SCOPE]
    at_statement_start = True
    index = 0
    while index < len(code):
        token = code[index]
        token_type = token.type

        if token_type == L.LeftBrace:
            scope.append(_brace_scope(code, index))
            at_statement_start = scope[-1] == _DECL_SCOPE
            index += 1
            continue
        if token_type == L.RightBrace:
            if len(scope) > 1:
                scope.pop()
            at_statement_start = scope[-1] == _DECL_SCOPE
            index += 1
            continue
        if token_type == L.Semi:
            at_statement_start = scope[-1] == _DECL_SCOPE
            index += 1
            continue

        if at_statement_start and scope[-1] == _DECL_SCOPE:
            target = _skip_attributes(code, index)
            if target < len(code) and _is_declaration_start(code, target):
                rewriter.insert_before(code[target], "public ")
            at_statement_start = False
        index += 1


def _brace_scope(code, brace_index: int) -> str:
    """Classify a `{`: does it open a declaration body (struct/class/enum) or a statement block?

    Scans backwards over the header, skipping balanced `()`/`[]`/`<>`, until it reaches either a
    type-declaration keyword (declaration scope) or a statement terminator / `)` (statement scope).
    """
    depth = 0
    saw_struct = False
    for cursor in range(brace_index - 1, -1, -1):
        token_type = code[cursor].type
        if token_type in _CLOSERS:
            depth += 1
        elif token_type in _OPENERS:
            if depth > 0:
                depth -= 1
        elif depth == 0:
            # `enum` wins over `class` for the `enum class Name {` form (class appears first walking
            # back), so keep scanning past a struct/class keyword until the statement terminator.
            if token_type == L.Enum:
                return _ENUM_SCOPE
            if token_type in (L.Struct, L.Class):
                saw_struct = True
            elif token_type in (L.Semi, L.LeftBrace, L.RightBrace, L.RightParen):
                return _DECL_SCOPE if saw_struct else _STMT_SCOPE
    return _DECL_SCOPE if saw_struct else _STMT_SCOPE


def _skip_attributes(code, index: int) -> int:
    """Advance past leading `[...]` / `[[...]]` attribute specifiers so `public` lands after them."""
    while index < len(code) and code[index].type in (L.LeftBracket, L.LeftDoubleBracket):
        depth = 0
        while index < len(code):
            token_type = code[index].type
            if token_type in (L.LeftBracket, L.LeftDoubleBracket):
                depth += 1 if token_type == L.LeftBracket else 2
            elif token_type == L.RightBracket:
                depth -= 1
                if depth <= 0:
                    index += 1
                    break
            index += 1
    return index


def _is_declaration_start(code, index: int) -> bool:
    """Whether the token begins a declaration that should be publicized.

    In a declaration scope nearly every statement is a declaration; this mainly guards against
    already-annotated declarations and non-declaration tokens.
    """
    token = code[index]
    if token.type == L.Identifier and token.text in _VISIBILITY_WORDS:
        return False  # already has an explicit visibility modifier
    if token.type == L.Identifier and token.text in _NON_DECL_WORDS:
        return False  # an import/module directive, not a declaration
    if token.type in (L.RightBrace, L.Semi):
        return False
    # A type keyword, `struct`/`enum`/`class`, `static`, `const`, or an identifier (a user type / return
    # type / field type) all begin a declaration in a declaration scope.
    return True
