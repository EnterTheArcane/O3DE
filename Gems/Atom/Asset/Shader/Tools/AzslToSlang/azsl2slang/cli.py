# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Command line entry point for the AZSL -> Slang porting tool."""

from __future__ import annotations

import argparse
import difflib
import json
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
    registry = registry_module.build([entry.source for entry in corpus])
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
    counts: Counter[str] = Counter()

    for entry in entries:
        if entry.source in tainted:
            shared_srg_skipped += 1
            # Remove a .slang left behind by an earlier pass that did convert it.
            if entry.target.exists() and not (args.dry_run or args.check):
                entry.target.unlink()
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

    print("\nsummary")
    for outcome in Outcome:
        if counts[outcome.value]:
            print(f"  {outcome.value:22} {counts[outcome.value]}")
    if skipped:
        print(f"  {'skipped (exists)':22} {skipped}")
    if shared_srg_skipped:
        print(f"  {'skipped (shared-SRG)':22} {shared_srg_skipped}")

    if args.report:
        _write_report(args.report, results)
        print(f"\nreport written to {args.report}")

    if args.check:
        changed = [r for r in results if r.text is not None and _differs(r)]
        if changed:
            print(f"\n{len(changed)} file(s) differ from what is checked in")
            return 1

    return 1 if counts[Outcome.FAILED.value] else 0


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
