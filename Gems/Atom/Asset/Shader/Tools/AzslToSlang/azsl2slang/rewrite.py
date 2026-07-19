# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Span-based rewriting over the token stream.

Rules never touch text directly; they stage edits addressed by token, and the driver replays the
original source with those edits applied. Anything no rule names is copied byte for byte, which is
what keeps the ports diffable against their AZSL originals.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum


class Severity(Enum):
    INFO = "info"
    TODO = "todo"
    MANUAL = "manual"


@dataclass(order=True)
class Edit:
    start: int  # inclusive character offset into the original text
    stop: int  # exclusive
    replacement: str

    def __post_init__(self) -> None:
        if self.stop < self.start:
            raise ValueError("edit stop precedes start")


@dataclass
class Note:
    """Something a reviewer needs to know about this file."""

    severity: Severity
    message: str
    line: int | None = None


@dataclass
class Rewriter:
    text: str
    edits: list[Edit] = field(default_factory=list)
    notes: list[Note] = field(default_factory=list)

    def replace_token(self, token, replacement: str) -> None:
        self.edits.append(Edit(token.start, token.stop + 1, replacement))

    def replace_span(self, first_token, last_token, replacement: str) -> None:
        self.edits.append(Edit(first_token.start, last_token.stop + 1, replacement))

    def insert_before(self, token, addition: str) -> None:
        self.edits.append(Edit(token.start, token.start, addition))

    def insert_after(self, token, addition: str) -> None:
        self.edits.append(Edit(token.stop + 1, token.stop + 1, addition))

    def delete_span(self, first_token, last_token) -> None:
        self.replace_span(first_token, last_token, "")

    def note(self, severity: Severity, message: str, line: int | None = None) -> None:
        self.notes.append(Note(severity, message, line))

    def apply(self) -> str:
        """Replay the source with all staged edits, rejecting overlaps."""
        ordered = sorted(self.edits, key=lambda edit: (edit.start, edit.stop))
        out: list[str] = []
        cursor = 0
        for edit in ordered:
            if edit.start < cursor:
                # Two rules claimed the same characters; emitting either silently would produce
                # output nobody reviewed, so fail loudly instead.
                raise ValueError(
                    f"overlapping edits at offset {edit.start} (already consumed to {cursor})"
                )
            out.append(self.text[cursor : edit.start])
            out.append(edit.replacement)
            cursor = edit.stop
        out.append(self.text[cursor:])
        return "".join(out)

    @property
    def has_todos(self) -> bool:
        return any(note.severity is Severity.TODO for note in self.notes)

    @property
    def needs_manual(self) -> bool:
        return any(note.severity is Severity.MANUAL for note in self.notes)
