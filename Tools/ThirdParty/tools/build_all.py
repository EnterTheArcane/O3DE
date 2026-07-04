#!/usr/bin/env python3
"""Build all ThirdParty recipes in dependency order.

Usage:
    python tools/build_all.py [options]

Options:
    --build-type <Release|Debug|RelWithDebInfo>
    --jobs/-j <N>
    --resume <name>   Start from this recipe (skip everything before it)
    --only <name,...> Comma-separated subset to build
    --dry-run         Print build order without building
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "src"))

from thirdparty._internal.detect import detect_platform_tag
from thirdparty._internal.graph.graph import Graph, is_built


def main() -> None:
    ap = argparse.ArgumentParser(description="Build all ThirdParty recipes in dependency order")
    ap.add_argument("--build-type", default="Release",
                    choices=["Debug", "Release", "RelWithDebInfo"])
    ap.add_argument("--jobs", "-j", type=int, default=None)
    ap.add_argument("--resume", metavar="NAME",
                    help="Skip all recipes before this one in build order")
    ap.add_argument("--only", metavar="NAMES",
                    help="Comma-separated list of recipe names to build (plus their deps)")
    ap.add_argument("--dry-run", action="store_true",
                    help="Print build order without actually building")
    ap.add_argument("--generate-only", action="store_true",
                    help="Only run generate() - no source download, cmake build, or package")
    ap.add_argument("--skip-built", action="store_true", default=True,
                    help="Skip already-built packages (default: true)")
    ap.add_argument("--force", action="store_true",
                    help="Rebuild even if already built")
    args = ap.parse_args()

    recipes_root = REPO / "recipes"
    build_root   = REPO / "build"

    names = sorted(d.name for d in recipes_root.iterdir()
                   if d.is_dir() and (d / "recipe.py").exists())

    # Resolve the dependency graph + build order via the library.
    g = Graph.build(recipes_root, names, args.build_type, transitive=True)
    known = set(g.names())
    graph: dict[str, list[str]] = {n: [d for d in g[n].all_deps if d in known]
                                   for n in g.names()}
    order = g.topo_order()

    # Apply --only filter (include transitive deps)
    if args.only:
        targets = set(args.only.split(","))
        needed: set[str] = set()
        def _collect(n: str) -> None:
            if n in needed: return
            needed.add(n)
            for d in graph.get(n, []):
                _collect(d)
        for t in targets:
            _collect(t)
        order = [n for n in order if n in needed]

    # Apply --resume
    if args.resume:
        try:
            idx = order.index(args.resume)
            order = order[idx:]
        except ValueError:
            print(f"[build_all] ERROR: --resume '{args.resume}' not found in build order",
                  file=sys.stderr)
            sys.exit(1)

    print(f"\n=== Build Plan: {len(order)} recipes ({args.build_type}) ===")
    plat = detect_platform_tag()
    for i, n in enumerate(order, 1):
        version = g[n].version
        built = is_built(build_root, n, version, plat)
        status = "[built]" if built else "[pending]"
        if args.force:
            status = "[force]"
        print(f"  {i:3d}. {n}/{version}  {status}")

    if args.dry_run:
        return

    print()

    # Build python executable
    python = sys.executable

    results: list[tuple[str, str, float, str | None]] = []  # name, version, secs, error
    skipped: list[str] = []

    for name in order:
        node = g[name]
        if node.recipe_cls is None:
            print(f"\n[build_all] SKIP {name} - cannot load recipe", file=sys.stderr)
            skipped.append(name)
            continue

        version = node.version

        if not args.force and is_built(build_root, name, version, plat):
            print(f"[build_all] {name}/{version} - already built, skipping")
            skipped.append(name)
            continue

        cmd = [python, "-m", "thirdparty", "build", name,
               "--build-type", args.build_type]
        if args.jobs:
            cmd += ["--jobs", str(args.jobs)]
        if args.generate_only:
            cmd += ["--generate-only"]

        print(f"\n{'='*70}")
        print(f"[build_all] Building {name}/{version} ...")
        print(f"{'='*70}")

        t0 = time.time()
        try:
            result = subprocess.run(
                cmd,
                cwd=str(REPO),
                # Don't capture - let output stream live
            )
            elapsed = time.time() - t0
            if result.returncode == 0:
                results.append((name, version, elapsed, None))
                print(f"[build_all] OK  {name}/{version}  ({elapsed:.1f}s)")
            else:
                error = f"exit code {result.returncode}"
                results.append((name, version, elapsed, error))
                print(f"[build_all] FAIL {name}/{version}: {error}")
        except Exception as exc:
            elapsed = time.time() - t0
            results.append((name, version, elapsed, str(exc)))
            print(f"[build_all] FAIL {name}/{version}: {exc}")

    # Summary
    ok    = [(n, v, t) for n, v, t, e in results if e is None]
    fail  = [(n, v, e) for n, v, t, e in results if e is not None]

    print(f"\n{'='*70}")
    print(f"=== Summary: {len(ok)} built, {len(fail)} failed, {len(skipped)} skipped ===")
    print(f"{'='*70}")

    if ok:
        print(f"\nBuilt ({len(ok)}):")
        for n, v, t in ok:
            print(f"  OK   {n}/{v}  ({t:.1f}s)")

    if skipped:
        print(f"\nSkipped ({len(skipped)}):")
        for n in skipped:
            print(f"  SKIP {n}")

    if fail:
        print(f"\nFailed ({len(fail)}):")
        for n, v, e in fail:
            print(f"  FAIL {n}/{v}: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
