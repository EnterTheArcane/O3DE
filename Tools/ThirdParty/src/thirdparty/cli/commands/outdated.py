from __future__ import annotations

import argparse
import importlib.util
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import colorama
from colorama import Fore, Style

from conan.internal.model.conan_file import ConanFile
from conan.internal.model.version import Version
from thirdparty.cli.command import command

_BUMP_COLOR = {
    "major": Fore.RED,
    "minor": Fore.YELLOW,
    "patch": Fore.CYAN,
}


def setup_parser(p: argparse.ArgumentParser) -> None:
    g = p.add_mutually_exclusive_group()
    g.add_argument(
        "--all",
        action="store_true",
        dest="all",
        help="Include recipes that have no latest_version() method.",
    )
    g.add_argument(
        "--missing",
        action="store_true",
        dest="missing",
        help="List only recipes that have no latest_version() method.",
    )


@command
def outdated(args: argparse.Namespace) -> None:
    """Report recipes whose pinned version lags upstream."""
    colorama.init()
    cwd = Path.cwd()
    recipes_root = cwd / "recipes"
    if not recipes_root.exists():
        print(f"[thirdparty] error: no 'recipes/' directory in {cwd}", file=sys.stderr)
        sys.exit(1)

    missing_rows: list[tuple[str, str]] = []
    rows: list[tuple[str, str, str, str, str | None]] = []
    checkable: list[tuple[str, type]] = []

    for recipe_dir in sorted(p for p in recipes_root.iterdir() if p.is_dir()):
        name = recipe_dir.name
        recipe_path = recipe_dir / "recipe.py"
        if not recipe_path.exists():
            continue
        try:
            cls = _load_recipe_class(recipe_path, name)
        except Exception as exc:
            rows.append((name, "?", "?", f"load-error: {exc}", None))
            continue
        if not hasattr(cls, "latest_version"):
            missing_rows.append((name, str(getattr(cls, "version", "?"))))
            if args.all:
                rows.append((name, str(getattr(cls, "version", "?")), "", "no-version-check", None))
            continue
        checkable.append((name, cls))

    if args.missing:
        _print_missing(missing_rows)
        return

    # latest_version() is network-bound; fan out so we don't wait serially on
    # one HTTP round-trip per recipe.
    with ThreadPoolExecutor(max_workers=16) as ex:
        futures = {ex.submit(_check_recipe, name, cls): name for name, cls in checkable}
        for f in as_completed(futures):
            rows.append(f.result())

    rows.sort(key=lambda r: r[0])
    _print_table(rows)


def _bump_level(current: str, latest: str) -> str:
    """Return 'major', 'minor', or 'patch' based on which component first differs."""
    def parts(v: str) -> list[int]:
        segs: list[int] = []
        for x in v.split("."):
            try:
                segs.append(int(x))
            except ValueError:
                break
        return segs

    cp = parts(current)
    lp = parts(latest)
    length = max(len(cp), len(lp), 1)
    cp += [0] * (length - len(cp))
    lp += [0] * (length - len(lp))

    if lp[0] != cp[0]:
        return "major"
    if length > 1 and lp[1] != cp[1]:
        return "minor"
    return "patch"


def _check_recipe(name: str, cls: type) -> tuple[str, str, str, str, str | None]:
    current = getattr(cls, "version", None)
    try:
        recipe = cls(display_name=name)
        latest = recipe.latest_version()
    except Exception as exc:
        return (name, str(current), "?", f"error: {exc}", None)

    try:
        cur_v = Version(str(current))
        lat_v = latest if isinstance(latest, Version) else Version(str(latest))
    except Exception as exc:
        return (name, str(current), str(latest), f"parse-error: {exc}", None)

    if cur_v < lat_v:
        bump = _bump_level(str(cur_v), str(lat_v))
        return (name, str(cur_v), str(lat_v), "OUTDATED", bump)
    if cur_v == lat_v:
        return (name, str(cur_v), str(lat_v), "up-to-date", None)
    return (name, str(cur_v), str(lat_v), "ahead", None)


def _load_recipe_class(recipe_path: Path, name: str) -> type[ConanFile]:
    spec = importlib.util.spec_from_file_location(f"_recipe_{name}", recipe_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {recipe_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    cls = getattr(module, "Recipe", None)
    if cls is None or not (isinstance(cls, type) and issubclass(cls, ConanFile)):
        raise RuntimeError(f"{recipe_path} has no Recipe class")
    return cls


def _print_missing(rows: list[tuple[str, str]]) -> None:
    if not rows:
        print("All recipes have a latest_version() method.")
        return
    name_w = max(len("recipe"), max(len(r[0]) for r in rows))
    ver_w  = max(len("version"), max(len(r[1]) for r in rows))
    print(f"{'recipe':<{name_w}}  {'version':<{ver_w}}")
    print(f"{'-' * name_w}  {'-' * ver_w}")
    for name, version in rows:
        print(f"{name:<{name_w}}  {version:<{ver_w}}")


def _print_table(rows: list[tuple[str, str, str, str, str | None]]) -> None:
    if not rows:
        print("No recipes with latest_version() defined.")
        return
    name_w = max(len("recipe"),  max(len(r[0]) for r in rows))
    cur_w  = max(len("current"), max(len(r[1]) for r in rows))
    lat_w  = max(len("latest"),  max(len(r[2]) for r in rows))

    print(f"{'recipe':<{name_w}}  {'current':<{cur_w}}  {'latest':<{lat_w}}  status")
    print(f"{'-' * name_w}  {'-' * cur_w}  {'-' * lat_w}  ------")

    for name, current, latest, status, bump in rows:
        latest_padded = f"{latest:<{lat_w}}"
        if status == "OUTDATED":
            color = _BUMP_COLOR.get(bump or "patch", Fore.CYAN)
            latest_col = f"{color}{latest_padded}{Style.RESET_ALL}"
            status_col = f"{color}{status}{Style.RESET_ALL}"
        elif status == "up-to-date":
            latest_col = latest_padded
            status_col = f"{Fore.GREEN}{status}{Style.RESET_ALL}"
        elif status == "ahead":
            latest_col = latest_padded
            status_col = f"{Style.DIM}{status}{Style.RESET_ALL}"
        elif status == "no-version-check":
            latest_col = latest_padded
            status_col = f"{Style.DIM}{status}{Style.RESET_ALL}"
        else:
            latest_col = latest_padded
            status_col = f"{Style.DIM}{status}{Style.RESET_ALL}"

        print(f"{name:<{name_w}}  {current:<{cur_w}}  {latest_col}  {status_col}")
