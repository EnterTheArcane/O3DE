# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Pass 2: convert one file by running every rule over its token stream."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path

from .discover import SourceFile
from .lexing import lex_file, lex_text
from .registry import Registry
from .rewrite import Note, Rewriter, Severity
from .rules import constants, includes, members, options, srg, types, visibility


class Outcome(Enum):
    CONVERTED = "converted"
    CONVERTED_WITH_TODOS = "converted-with-todos"
    NEEDS_MANUAL = "needs-manual"
    FAILED = "failed"


@dataclass
class Result:
    entry: SourceFile
    outcome: Outcome
    text: str | None = None
    notes: list[Note] = field(default_factory=list)
    error: str | None = None


# Order matters only where rules could contend for the same tokens: SRG conversion rewrites the
# declaration header, options rewrite whole declarations, and the rest touch disjoint spans.
_RULES = (includes, constants, srg, options, members, types)


def convert(entry: SourceFile, registry: Registry) -> Result:
    try:
        lexed = lex_file(entry.source)
    except Exception as exc:  # noqa: BLE001 — any lexer failure is a per-file report, not a crash
        return Result(entry, Outcome.FAILED, error=f"{type(exc).__name__}: {exc}")

    if lexed.lex_errors:
        return Result(
            entry,
            Outcome.FAILED,
            error=f"{len(lexed.lex_errors)} lex error(s); first: {lexed.lex_errors[0]}",
        )

    # Structural moves run first against their own rewriter, then the file is re-lexed so the
    # remaining rules address code where it finally sits (see types.apply_prepass).
    prepass = Rewriter(text=lexed.text)
    types.apply_prepass(lexed, registry, prepass)
    prepass_notes = list(prepass.notes)
    if prepass.edits:
        try:
            lexed = lex_text(prepass.apply(), entry.source)
        except ValueError as exc:
            return Result(entry, Outcome.FAILED, notes=prepass_notes, error=str(exc))

    rewriter = Rewriter(text=lexed.text)
    rewriter.notes.extend(prepass_notes)

    if not lexed.brace_depth_balanced():
        # Braces that do not pair up mean a construct is split across #if/#else arms. Token-level
        # scanning cannot follow that safely, so the file is reported instead of guessed at.
        rewriter.note(
            Severity.MANUAL,
            f"unbalanced braces (final depth {lexed.final_brace_depth()});"
            " likely a construct split across preprocessor branches",
        )
        return Result(entry, Outcome.NEEDS_MANUAL, notes=rewriter.notes)

    for rule in _RULES:
        rule.apply(lexed, registry, rewriter)

    try:
        text = rewriter.apply()
    except ValueError as exc:
        return Result(entry, Outcome.FAILED, notes=rewriter.notes, error=str(exc))

    # Access control runs last, on the converted text: it publicizes the *final* declarations
    # (structs, extern options, ParameterBlocks), so it must see them in their ported form, not the
    # raw AZSL the other rules restructure.
    post_lexed = lex_text(text, entry.source)
    postpass = Rewriter(text=text)
    visibility.apply(post_lexed, registry, postpass)
    if postpass.edits:
        try:
            text = postpass.apply()
        except ValueError as exc:
            return Result(entry, Outcome.FAILED, notes=rewriter.notes, error=str(exc))

    if rewriter.needs_manual:
        outcome = Outcome.NEEDS_MANUAL
    elif rewriter.has_todos:
        outcome = Outcome.CONVERTED_WITH_TODOS
    else:
        outcome = Outcome.CONVERTED

    return Result(entry, outcome, text=text, notes=rewriter.notes)


def write(result: Result, target: Path | None = None) -> None:
    destination = target or result.entry.target
    if result.text is None:
        raise ValueError(f"no converted text for {result.entry.source}")
    destination.write_text(result.text, encoding="utf-8", newline="")
