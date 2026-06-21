#!/usr/bin/env python3
"""Probe all recipes: import them, call requirements(), and print the dep graph.

Usage:  python tools/probe_recipes.py [--dot]
  --dot   emit Graphviz DOT to stdout instead of text tree
"""
from __future__ import annotations

import argparse
import importlib.util
import sys
from collections import OrderedDict
from pathlib import Path

# Make sure the src/ tree is importable
REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "src"))

from thirdparty._internal.model.recipe_base import RecipeBase
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty.env import Environment
from thirdparty._internal.detect import detect_settings, make_conf


class _PassthroughWrapper:
    def wrap(self, cmd, **_kw):
        return cmd


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
        if cls is None or not issubclass(cls, RecipeBase):
            return None
        return cls
    except Exception as exc:
        print(f"  [LOAD ERROR] {name}: {exc}", file=sys.stderr)
        return None


def _probe(cls: type[RecipeBase], name: str, recipes_root: Path) -> list[str]:
    """Return list of direct dep names (best-effort)."""
    try:
        recipe = cls(display_name=name)
        recipe.version = getattr(cls, "version", "0") or "0"
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
            try:
                recipe.config_options()
            except Exception:
                pass
        if hasattr(recipe, "configure"):
            try:
                recipe.configure()
            except Exception:
                pass
        if hasattr(recipe, "requirements"):
            try:
                recipe.requirements()
            except Exception:
                pass

        return [str(req.ref.name) for req in recipe.requires.values()]
    except Exception as exc:
        print(f"  [PROBE ERROR] {name}: {exc}", file=sys.stderr)
        return []


def _topo_sort(graph: dict[str, list[str]]) -> list[str]:
    """Kahn's algorithm — returns leaves first."""
    in_degree = {n: 0 for n in graph}
    for deps in graph.values():
        for d in deps:
            if d in in_degree:
                in_degree[d] += 1  # deps are prerequisites, not dependents

    # Actually we want leaves (no deps) first, so reverse: count how many deps
    # each node has.
    in_degree = {n: len(graph[n]) for n in graph}
    queue = [n for n, d in in_degree.items() if d == 0]
    order: list[str] = []
    while queue:
        queue.sort()
        node = queue.pop(0)
        order.append(node)
        for n, deps in graph.items():
            if node in deps:
                in_degree[n] -= 1
                if in_degree[n] == 0:
                    queue.append(n)

    # Append any cycles / unknowns
    remaining = [n for n in graph if n not in order]
    order.extend(sorted(remaining))
    return order


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dot", action="store_true", help="Emit Graphviz DOT")
    ap.add_argument("--build-order", action="store_true", help="Print topological build order")
    args = ap.parse_args()

    recipes_root = REPO / "recipes"
    names = sorted(d.name for d in recipes_root.iterdir() if d.is_dir() and (d / "recipe.py").exists())

    graph: dict[str, list[str]] = {}
    errors: list[str] = []

    for name in names:
        cls = _load(recipes_root, name)
        if cls is None:
            errors.append(name)
            graph[name] = []
            continue
        deps = _probe(cls, name, recipes_root)
        # Only keep deps that are known recipes
        graph[name] = [d for d in deps if d in set(names)]
        unknown = [d for d in deps if d not in set(names)]
        if unknown:
            print(f"  [UNKNOWN DEPS] {name} -> {unknown}", file=sys.stderr)

    if args.dot:
        print("digraph deps {")
        print('  rankdir=LR; node [shape=box];')
        for name, deps in graph.items():
            for dep in deps:
                print(f'  "{name}" -> "{dep}";')
        print("}")
        return

    order = _topo_sort(graph)

    if args.build_order:
        print("\n=== Build Order (leaf → root) ===")
        for i, n in enumerate(order, 1):
            deps = graph[n]
            dep_str = f"  deps: {deps}" if deps else ""
            print(f"  {i:3d}. {n}{dep_str}")
        return

    # Default: print full dep tree
    print(f"\n=== Recipes ({len(names)}) ===")
    for n in order:
        deps = graph[n]
        dep_str = f" -> {deps}" if deps else ""
        print(f"  {n}{dep_str}")

    if errors:
        print(f"\n=== Load/Parse Errors ({len(errors)}) ===")
        for n in errors:
            print(f"  {n}")

    print(f"\nTotal: {len(names)} recipes, {len(errors)} errors")


if __name__ == "__main__":
    main()
