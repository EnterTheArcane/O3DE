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
import importlib.util
import subprocess
import sys
import time
import traceback
from collections import OrderedDict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "src"))

from thirdparty._internal.model.recipe_base import RecipeBase
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty.env import Environment
from thirdparty._internal.detect import detect_settings, make_conf


class _PassthroughWrapper:
    def wrap(self, cmd, **_kw): return cmd

class _RecipeRuntime:
    def __init__(self, conf):
        self.cmd_wrapper = _PassthroughWrapper()
        self.global_conf = conf
        self.requester = None
        self.cache = None
        self.home_folder = None
        self.api = None


def _load(recipes_root: Path, name: str) -> type[RecipeBase] | None:
    recipe_path = recipes_root / name / "recipe.py"
    if not recipe_path.exists():
        return None
    try:
        spec = importlib.util.spec_from_file_location(f"_recipe_{name}", recipe_path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        cls = getattr(mod, "Recipe", None)
        return cls if (cls and isinstance(cls, type) and issubclass(cls, RecipeBase)) else None
    except Exception:
        return None


def _get_version(cls: type[RecipeBase]) -> str:
    v = getattr(cls, "version", None)
    return str(v) if v else "latest"


def _probe_deps(cls: type[RecipeBase], name: str, recipes_root: Path) -> list[str]:
    try:
        recipe = cls(display_name=name)
        recipe.version = _get_version(cls)
        recipe.recipe_folder = str(recipes_root / name)
        recipe.settings = detect_settings("Release")
        recipe.settings_build = recipe.settings
        recipe.settings_target = None
        conf = make_conf()
        recipe.conf = conf
        recipe._recipe_runtime = _RecipeRuntime(conf)
        recipe._recipe_dependencies = RecipeDependencies(OrderedDict())
        recipe._recipe_buildenv = Environment()
        recipe._recipe_runenv = Environment()
        if hasattr(recipe, "config_options"):
            try: recipe.config_options()
            except Exception: pass
        if hasattr(recipe, "configure"):
            try: recipe.configure()
            except Exception: pass
        if hasattr(recipe, "requirements"):
            try: recipe.requirements()
            except Exception: pass
        return [str(r.ref.name) for r in recipe.requires.values()]
    except Exception:
        return []


def _topo_sort(graph: dict[str, list[str]]) -> list[str]:
    in_degree = {n: len([d for d in deps if d in graph]) for n, deps in graph.items()}
    queue = sorted(n for n, d in in_degree.items() if d == 0)
    order: list[str] = []
    while queue:
        node = queue.pop(0)
        order.append(node)
        for n, deps in graph.items():
            if n not in order and node in deps:
                in_degree[n] -= 1
                if in_degree[n] == 0:
                    queue.append(n)
                    queue.sort()
    # Append any cycles
    for n in graph:
        if n not in order:
            order.append(n)
    return order


def _is_built(build_root: Path, name: str, version: str) -> bool:
    pkg = build_root / name / version / "package"
    return pkg.exists() and any(pkg.iterdir())


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
                    help="Only run generate() — no source download, cmake build, or package")
    ap.add_argument("--skip-built", action="store_true", default=True,
                    help="Skip already-built packages (default: true)")
    ap.add_argument("--force", action="store_true",
                    help="Rebuild even if already built")
    args = ap.parse_args()

    recipes_root = REPO / "recipes"
    build_root   = REPO / "build"

    names = sorted(d.name for d in recipes_root.iterdir() if d.is_dir())
    known = set(names)

    # Build dep graph
    graph: dict[str, list[str]] = {}
    for name in names:
        cls = _load(recipes_root, name)
        if cls is None:
            graph[name] = []
            continue
        deps = _probe_deps(cls, name, recipes_root)
        graph[name] = [d for d in deps if d in known]

    order = _topo_sort(graph)

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
    for i, n in enumerate(order, 1):
        cls = _load(recipes_root, n)
        version = _get_version(cls) if cls else "?"
        built = _is_built(build_root, n, version)
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
        cls = _load(recipes_root, name)
        if cls is None:
            print(f"\n[build_all] SKIP {name} — cannot load recipe", file=sys.stderr)
            skipped.append(name)
            continue

        version = _get_version(cls)

        if not args.force and _is_built(build_root, name, version):
            print(f"[build_all] {name}/{version} — already built, skipping")
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
                # Don't capture — let output stream live
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
