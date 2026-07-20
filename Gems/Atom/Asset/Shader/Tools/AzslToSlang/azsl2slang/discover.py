# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Locating the AZSL sources to port, and mapping them to their Slang counterparts."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator

# Asset-processor output and CMake trees mirror the real sources; converting those would roughly
# double the file count with duplicates.
EXCLUDED_DIR_NAMES = frozenset(
    {"Cache", "build", "External", "Intermediate", ".git", "_deps", "cmake-build-debug"}
)

SOURCE_SUFFIXES = frozenset({".azsl", ".azsli", ".srgi"})

# Everything is a Slang module now (imported or #included), and modules resolve to `.slang`, so all
# ported files take the `.slang` extension — the old `.slangi` include-only convention is retired.
SUFFIX_MAP = {".azsl": ".slang", ".azsli": ".slang", ".srgi": ".slang"}

# Per-API compiler prologues defining macros like UNBOUNDED_SIZE. The Slang path has its own
# ApiPrelude.slang files, so these are never ported.
EXCLUDED_STEMS = frozenset({"AzslcHeader"})

DEFAULT_SEARCH_ROOTS = ("Gems", "Templates", "AutomatedTesting")


@dataclass(frozen=True)
class SourceFile:
    """An AZSL source and the Slang file it maps to."""

    source: Path
    target: Path

    @property
    def is_include(self) -> bool:
        return self.source.suffix in (".azsli", ".srgi")


def is_excluded(path: Path) -> bool:
    return any(part in EXCLUDED_DIR_NAMES for part in path.parts)


def target_for(source: Path) -> Path:
    return source.with_suffix(SUFFIX_MAP[source.suffix])


def discover(roots: Iterable[Path], repo_root: Path) -> list[SourceFile]:
    """Walk `roots` for AZSL sources, skipping generated trees and non-portable prologues.

    Paths that are themselves files are taken as-is, so `--file` and directory arguments share
    this entry point.
    """
    found: list[SourceFile] = []
    seen: set[Path] = set()

    for root in roots:
        root = root if root.is_absolute() else repo_root / root
        if root.is_file():
            candidates: Iterator[Path] = iter([root])
        elif root.is_dir():
            candidates = _walk(root)
        else:
            continue

        for path in candidates:
            if path.suffix not in SOURCE_SUFFIXES or is_excluded(path):
                continue
            if path.stem in EXCLUDED_STEMS:
                continue
            resolved = path.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            found.append(SourceFile(source=path, target=target_for(path)))

    found.sort(key=lambda entry: str(entry.source).lower())
    return found


def _walk(root: Path) -> Iterator[Path]:
    """os.walk with in-place pruning, so excluded trees are never descended into."""
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in EXCLUDED_DIR_NAMES]
        for filename in filenames:
            yield Path(dirpath) / filename
