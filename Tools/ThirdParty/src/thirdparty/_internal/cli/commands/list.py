from __future__ import annotations

import argparse
import fnmatch
import sys
from pathlib import Path

import colorama
from colorama import Fore, Style

from thirdparty._internal.cli.command import command
from thirdparty._internal.graph.recipe_graph import build_recipe_graph, is_built
from thirdparty._internal.detect import detect_platform_tag


def setup_parser(p: argparse.ArgumentParser) -> None:
    p.add_argument("recipe", metavar="<recipe>", nargs="*",
                   help="Recipe name(s) or glob pattern(s) to list (default: all)")
    p.add_argument("--deps", action="store_true",
                   help="Show each recipe's direct dependencies")
    p.add_argument("--build-order", action="store_true", dest="build_order",
                   help="List in dependency (build) order instead of alphabetically")
    p.add_argument(
        "--build-type",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo"],
        dest="build_type",
        metavar="<type>",
    )


@command(name="list")
def list_recipes(args: argparse.Namespace) -> None:
    """List available recipes with version and build status."""
    colorama.init()
    cwd = Path.cwd()
    recipes_root = cwd / "recipes"
    build_root = cwd / "build"
    if not recipes_root.exists():
        print(f"[thirdparty] error: no 'recipes/' directory in {cwd}", file=sys.stderr)
        sys.exit(1)

    all_names = sorted(
        d.name for d in recipes_root.iterdir()
        if d.is_dir() and (d / "recipe.py").exists()
    )

    patterns = args.recipe or ["*"]
    names: list[str] = []
    for pat in patterns:
        if any(c in pat for c in "*?["):
            for m in fnmatch.filter(all_names, pat):
                if m not in names:
                    names.append(m)
        elif pat in all_names:
            if pat not in names:
                names.append(pat)
        else:
            print(f"[thirdparty] warn: no recipe named '{pat}'", file=sys.stderr)

    if not names:
        print("[thirdparty] no recipes matched", file=sys.stderr)
        sys.exit(1)

    graph = build_recipe_graph(recipes_root, names, args.build_type)
    order = graph.topo_order() if args.build_order else sorted(names)

    plat = detect_platform_tag()
    rows: list[tuple[str, str, bool, list[str], list[str]]] = []
    built_count = 0
    for name in order:
        node = graph[name]
        built = node.version != "?" and is_built(build_root, name, node.version, plat)
        built_count += built
        rows.append((name, node.version, built, node.host_deps, node.tool_deps))

    name_w = max(len("recipe"), max(len(r[0]) for r in rows))
    ver_w = max(len("version"), max(len(r[1]) for r in rows))
    print(f"{'recipe':<{name_w}}  {'version':<{ver_w}}  status")
    print(f"{'-' * name_w}  {'-' * ver_w}  ------")
    for name, version, built, host_deps, tool_deps in rows:
        status = (f"{Fore.GREEN}built{Style.RESET_ALL}" if built
                  else f"{Style.DIM}pending{Style.RESET_ALL}")
        print(f"{name:<{name_w}}  {version:<{ver_w}}  {status}")
        if args.deps:
            if host_deps:
                print(f"{'':<{name_w}}    requires: {', '.join(host_deps)}")
            if tool_deps:
                print(f"{'':<{name_w}}    tools:    {', '.join(tool_deps)}")

    print()
    print(f"{len(rows)} recipes ({built_count} built, {len(rows) - built_count} pending)")
