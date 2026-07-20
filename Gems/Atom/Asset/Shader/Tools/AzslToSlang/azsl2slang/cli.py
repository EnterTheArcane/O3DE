# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Command line entry point for the AZSL -> Slang porting tool."""

from __future__ import annotations

import argparse
import difflib
import json
import re
import sys
from collections import Counter
from pathlib import Path

from . import registry as registry_module
from .convert import Outcome, convert, write
from .discover import DEFAULT_SEARCH_ROOTS, discover
from .taint import compute_shared_srg_taint

# .../Gems/Atom/Asset/Shader/Tools/AzslToSlang/azsl2slang/cli.py -> repo root
REPO_ROOT = Path(__file__).resolve().parents[7]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="azsl2slang",
        description="Port AZSL shader sources to Slang, writing .slang/.slangi beside the originals.",
    )
    parser.add_argument(
        "paths", nargs="*", type=Path,
        help="files or directories to convert (default: the engine's shader trees)",
    )
    parser.add_argument("--check", action="store_true",
                        help="exit non-zero if any output would differ from what is on disk")
    parser.add_argument("--dry-run", action="store_true",
                        help="print diffs instead of writing files")
    parser.add_argument("--force", action="store_true",
                        help="overwrite existing .slang/.slangi outputs")
    parser.add_argument("--flip-shader", action="store_true",
                        help='rewrite matching .shader "Source" entries to the ported file')
    parser.add_argument("--verify", action="store_true",
                        help="syntax-check each converted file with slangc")
    parser.add_argument("--report", type=Path, help="write a JSON report here")
    parser.add_argument("--quiet", action="store_true", help="only print the summary")
    parser.add_argument(
        "--include-shared-srg", action="store_true",
        help="also convert shaders that depend on Scene/View/Bindless SRGs. Off by default: those "
             "need the hand-authored, ABI-pinned shared-SRG aggregates, so this pass ports only "
             "shaders that stand on their own private SRGs and leaves the rest as AZSL.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    roots = args.paths or [Path(root) for root in DEFAULT_SEARCH_ROOTS]
    entries = discover(roots, REPO_ROOT)
    if not entries:
        print("no AZSL sources found", file=sys.stderr)
        return 1

    print(f"discovered {len(entries)} AZSL source(s)")
    print("scanning for shader options and ShaderResourceGroups...")
    # The registry always covers the whole tree, even when converting a subset: semantics live in
    # SrgSemantics.azsli and options are declared in headers far from their use sites, so a
    # subset-only scan would silently fail to resolve binding slots and option calls.
    corpus = discover([Path(root) for root in DEFAULT_SEARCH_ROOTS], REPO_ROOT)
    shader_files = _shader_files([Path(root) for root in DEFAULT_SEARCH_ROOTS])
    registry = registry_module.build([entry.source for entry in corpus], shader_files)
    print(
        f"  {len(registry.srgs)} ShaderResourceGroups, {len(registry.semantics)} semantics, "
        f"{len(registry.options)} options, {len(registry.enums)} enums"
    )
    for collision in registry.collisions:
        print(f"  note: {collision}")

    # Shaders depending on a shared SRG (Scene/View/Bindless) can't stand alone as Slang yet, so this
    # pass skips them and leaves them as AZSL. Computed over the whole corpus (taint is transitive).
    tainted: set = set()
    if not args.include_shared_srg:
        tainted = compute_shared_srg_taint([entry.source for entry in corpus])
        print(f"  excluding {len(tainted)} shared-SRG-dependent source(s); --include-shared-srg to convert them")

    results = []
    skipped = 0
    shared_srg_skipped = 0
    hand_authored_skipped = 0
    counts: Counter[str] = Counter()

    for entry in entries:
        if entry.source in tainted:
            shared_srg_skipped += 1
            # Remove a .slang left behind by an earlier pass that did convert it.
            if entry.target.exists() and not (args.dry_run or args.check):
                entry.target.unlink()
            continue

        # A hand-authored .slang (converted output the converter cannot produce — macros that must
        # become real functions, etc.) is left untouched, even under --force. The registry still
        # scans its AZSL source, so its options/SRGs/macros stay visible to the rest of the corpus.
        if _is_hand_authored(entry.target):
            hand_authored_skipped += 1
            continue

        if entry.target.exists() and not (args.force or args.dry_run or args.check):
            skipped += 1
            continue

        result = convert(entry, registry)
        results.append(result)
        counts[result.outcome.value] += 1

        relative = _relative(entry.source)
        if result.outcome is Outcome.FAILED:
            print(f"  FAILED   {relative}: {result.error}")
            continue

        if args.dry_run or args.check:
            _show_diff(result, args)
        elif result.text is not None:
            write(result)
            # Modules are `.slang`; remove the `.slangi` an earlier pass wrote for the same source.
            stale = entry.source.with_suffix(".slangi")
            if stale != entry.target and stale.exists():
                stale.unlink()

        if not args.quiet:
            for note in result.notes:
                print(f"  {note.severity.value:6} {relative}"
                      f"{f':{note.line}' if note.line else ''}: {note.message}")

    if args.flip_shader and not (args.dry_run or args.check):
        flipped = _flip_shaders(roots)
        print(f"\nflipped {flipped} .shader Source entry(ies) to .slang")

    print("\nsummary")
    for outcome in Outcome:
        if counts[outcome.value]:
            print(f"  {outcome.value:22} {counts[outcome.value]}")
    if skipped:
        print(f"  {'skipped (exists)':22} {skipped}")
    if shared_srg_skipped:
        print(f"  {'skipped (shared-SRG)':22} {shared_srg_skipped}")
    if hand_authored_skipped:
        print(f"  {'skipped (hand-authored)':22} {hand_authored_skipped}")

    if args.report:
        _write_report(args.report, results)
        print(f"\nreport written to {args.report}")

    if args.check:
        changed = [r for r in results if r.text is not None and _differs(r)]
        if changed:
            print(f"\n{len(changed)} file(s) differ from what is checked in")
            return 1

    return 1 if counts[Outcome.FAILED.value] else 0


# A converted .slang carrying this marker near its top is hand-authored and never regenerated.
HAND_AUTHORED_MARKER = "azsl2slang:hand-authored"


def _is_hand_authored(target: Path) -> bool:
    """Whether an existing .slang output is hand-authored (marked do-not-regenerate)."""
    if not target.exists():
        return False
    try:
        with target.open("r", encoding="utf-8-sig") as handle:
            head = handle.read(2048)
    except OSError:
        return False
    return HAND_AUTHORED_MARKER in head


def _shader_files(roots: list[Path]) -> list[Path]:
    """Every `.shader` under the search roots — the registry reads their entry-point names."""
    seen: set[Path] = set()
    for root in roots:
        root = root if root.is_absolute() else REPO_ROOT / root
        if root.is_dir():
            seen.update(root.rglob("*.shader"))
    return sorted(seen)


_SHADER_SOURCE = re.compile(r'("Source"\s*:\s*")([^"]+)\.azsl(")')
_SHADER_SOURCE_SLANG = re.compile(r'("Source"\s*:\s*")([^"]+)\.slang(")')


def _flip_shaders(roots: list[Path]) -> int:
    """Point each `.shader`'s `"Source"` at the ported file when it exists, and back at the `.azsl`
    when it does not. A shader that stays AZSL (shared-SRG) keeps its `.azsl`; one whose `.slang` was
    removed — e.g. a shared-SRG shader whose sibling *header* had wrongly produced the `.slang` before
    the header was renamed — is reverted so it does not dangle at a missing source. Edits are textual
    to preserve the JSON formatting."""
    flipped = 0
    seen: set[Path] = set()
    for root in roots:
        root = root if root.is_absolute() else REPO_ROOT / root
        if not root.is_dir():
            continue
        for shader in root.rglob("*.shader"):
            if shader in seen:
                continue
            seen.add(shader)
            text = shader.read_text(encoding="utf-8-sig")

            match = _SHADER_SOURCE.search(text)
            if match and (shader.parent / f"{match.group(2)}.slang").exists():
                replacement = f"{match.group(1)}{match.group(2)}.slang{match.group(3)}"
            else:
                # Revert a `.slang` source with no ported file back to its `.azsl` original.
                match = _SHADER_SOURCE_SLANG.search(text)
                if not match or (shader.parent / f"{match.group(2)}.slang").exists():
                    continue
                if not (shader.parent / f"{match.group(2)}.azsl").exists():
                    continue
                replacement = f"{match.group(1)}{match.group(2)}.azsl{match.group(3)}"

            shader.write_text(text[: match.start()] + replacement + text[match.end() :], encoding="utf-8", newline="")
            flipped += 1
    return flipped


def _relative(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def _differs(result) -> bool:
    target = result.entry.target
    if not target.exists():
        return True
    return target.read_text(encoding="utf-8", newline="") != result.text


def _show_diff(result, args) -> None:
    if result.text is None:
        return
    target = result.entry.target
    current = target.read_text(encoding="utf-8", newline="") if target.exists() else ""
    if current == result.text:
        return
    if args.quiet:
        return
    diff = difflib.unified_diff(
        current.splitlines(keepends=True),
        result.text.splitlines(keepends=True),
        fromfile=f"{_relative(target)} (on disk)",
        tofile=f"{_relative(target)} (generated)",
    )
    sys.stdout.writelines(diff)


def _write_report(path: Path, results) -> None:
    payload = [
        {
            "source": _relative(result.entry.source),
            "target": _relative(result.entry.target),
            "outcome": result.outcome.value,
            "error": result.error,
            "notes": [
                {"severity": note.severity.value, "message": note.message, "line": note.line}
                for note in result.notes
            ],
        }
        for result in results
    ]
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
