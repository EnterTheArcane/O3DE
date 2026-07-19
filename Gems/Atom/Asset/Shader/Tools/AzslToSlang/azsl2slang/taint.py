# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Identifying sources that depend on a shared SRG (Scene/View/Bindless), transitively.

Shared SRGs are `partial ShaderResourceGroup`s with no Slang equivalent; porting them needs a
hand-authored, ABI-pinned aggregate. Until that path is committed, a shader that reads Scene/View/
Bindless cannot become a `.slang` (Slang can't `#include` an AZSL partial SRG). This pass finds every
such shader -- directly *or* through an included helper -- so the converter can skip them and port
only what stands on its own private SRGs.

Over-approximation is the safe direction: marking a borderline file tainted just leaves it as AZSL,
whereas missing a dependency would emit a `.slang` that cannot resolve its SRG.
"""

from __future__ import annotations

import re
from pathlib import Path

# A shader uses a shared SRG when it scope-qualifies one of these names.
_SHARED_SRG_USE = re.compile(r"\b(?:ViewSrg|SceneSrg|Bindless)::")

# The shared-SRG definition/aggregation files themselves are tainted at the root: any file that
# pulls one in is consuming a shared SRG.
_SHARED_SRG_FILENAME = re.compile(
    r"(?:^|[\\/])(?:scenesrg\w*\.srgi|viewsrg\w*\.srgi|Bindless\.azsli"
    r"|(?:Scene|View)Srg(?:All|IncludesAll)?\.azsli)$",
    re.IGNORECASE,
)

_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def compute_shared_srg_taint(sources: list[Path]) -> set[Path]:
    """Return the subset of `sources` that depend on a shared SRG, directly or transitively."""
    by_basename: dict[str, list[Path]] = {}
    for path in sources:
        by_basename.setdefault(path.name.lower(), []).append(path)

    includes: dict[Path, set[Path]] = {}
    included_by: dict[Path, set[Path]] = {path: set() for path in sources}
    tainted: set[Path] = set()

    for path in sources:
        try:
            text = path.read_text(encoding="utf-8-sig", errors="replace")
        except OSError:
            continue

        if _SHARED_SRG_FILENAME.search(str(path)) or _SHARED_SRG_USE.search(text):
            tainted.add(path)

        resolved: set[Path] = set()
        for target in _INCLUDE.findall(text):
            resolved.update(_resolve(target, by_basename))
        includes[path] = resolved
        for target in resolved:
            included_by.setdefault(target, set()).add(path)

    # Propagate taint from included files up to their includers until it stops growing.
    frontier = list(tainted)
    while frontier:
        current = frontier.pop()
        for includer in included_by.get(current, ()):
            if includer not in tainted:
                tainted.add(includer)
                frontier.append(includer)

    return tainted


def _resolve(target: str, by_basename: dict[str, list[Path]]) -> list[Path]:
    """Resolve an include target to source files by basename (conservative: matches all candidates)."""
    basename = target.replace("\\", "/").rsplit("/", 1)[-1].lower()
    return by_basename.get(basename, [])
