# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""The token stream the rewrite rules scan, built from the vendored AZSL lexer.

Formatting preservation rests entirely on this layer: the lexer routes whitespace and newlines to
the HIDDEN channel, comments to COMMENTS and preprocessor directives to PREPROCESSOR, so every byte
of the original file is present as some token. Rules edit only the tokens they name, and everything
else is re-emitted verbatim.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator, Sequence

from antlr4 import CommonTokenStream, InputStream, Token
from antlr4.error.ErrorListener import ErrorListener

_GENERATED_DIR = Path(__file__).resolve().parent.parent / "generated"
if str(_GENERATED_DIR) not in sys.path:
    sys.path.insert(0, str(_GENERATED_DIR))

from azslLexer import azslLexer  # noqa: E402  (path must be set up first)

# Channel numbering follows the `channels { PREPROCESSOR, COMMENTS }` declaration in azslLexer.g4;
# ANTLR assigns user channels from 2 upward, after DEFAULT (0) and HIDDEN (1).
CHANNEL_DEFAULT = 0
CHANNEL_HIDDEN = 1
CHANNEL_PREPROCESSOR = 2
CHANNEL_COMMENTS = 3


class LexError(Exception):
    """Raised when the lexer cannot tokenize a file, which demotes it to needs-manual."""


class _CollectingErrorListener(ErrorListener):
    def __init__(self) -> None:
        super().__init__()
        self.errors: list[str] = []

    def syntaxError(self, recognizer, offendingSymbol, line, column, msg, e):  # noqa: N802
        self.errors.append(f"line {line}:{column} {msg}")


@dataclass
class LexedFile:
    """A fully tokenized source file plus the cursors the rules navigate it with."""

    path: Path
    text: str
    tokens: list  # antlr4 CommonToken, in source order, all channels
    stream: CommonTokenStream
    lex_errors: list[str] = field(default_factory=list)

    @property
    def code_tokens(self) -> list:
        """Default-channel tokens only: the actual AZSL code, no trivia or directives."""
        return [token for token in self.tokens if token.channel == CHANNEL_DEFAULT]

    def text_of(self, token) -> str:
        return token.text

    def brace_depth_balanced(self) -> bool:
        """False when `{`/`}` do not pair up over the whole file.

        Unbalanced braces mean a construct is split across `#if`/`#else` arms, which is the one
        situation where token-level scanning can silently desync. Such files are reported rather
        than converted.
        """
        return self.final_brace_depth() == 0

    def final_brace_depth(self) -> int:
        depth = 0
        for token in self.code_tokens:
            if token.type == azslLexer.LeftBrace:
                depth += 1
            elif token.type == azslLexer.RightBrace:
                depth -= 1
        return depth


def lex_text(text: str, path: Path) -> LexedFile:
    listener = _CollectingErrorListener()
    lexer = azslLexer(InputStream(text))
    lexer.removeErrorListeners()
    lexer.addErrorListener(listener)

    stream = CommonTokenStream(lexer)
    stream.fill()
    # The trailing EOF token carries no source text and would confuse span arithmetic.
    tokens = [token for token in stream.tokens if token.type != Token.EOF]

    return LexedFile(
        path=path, text=text, tokens=tokens, stream=stream, lex_errors=listener.errors
    )


def lex_file(path: Path) -> LexedFile:
    # newline="" keeps the original line endings intact so a round trip is byte-exact on files
    # with CRLF, which most of the engine's shaders use.
    text = path.read_text(encoding="utf-8-sig", newline="")
    return lex_text(text, path)


def iter_code_with_index(lexed: LexedFile) -> Iterator[tuple[int, object]]:
    """Yield (index into the code-token list, token), the addressing rules use for lookahead."""
    yield from enumerate(lexed.code_tokens)


def token_is(token, *type_ids: int) -> bool:
    return token.type in type_ids


def find_matching_brace(code_tokens: Sequence, open_index: int) -> int:
    """Index of the `}` closing the `{` at `open_index`, or -1 if it is never closed."""
    if code_tokens[open_index].type != azslLexer.LeftBrace:
        raise ValueError("find_matching_brace must start on a LeftBrace token")

    depth = 0
    for index in range(open_index, len(code_tokens)):
        token_type = code_tokens[index].type
        if token_type == azslLexer.LeftBrace:
            depth += 1
        elif token_type == azslLexer.RightBrace:
            depth -= 1
            if depth == 0:
                return index
    return -1
