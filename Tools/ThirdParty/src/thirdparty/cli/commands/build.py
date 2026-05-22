from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

from thirdparty.cli.command import command
from thirdparty.internal.model.recipe import DepInfo, RecipeBase


def setup_parser(p: argparse.ArgumentParser) -> None:
    p.add_argument("recipe", metavar="<recipe>", help="Recipe name to build")
    p.add_argument(
        "--build-type",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo"],
        dest="build_type",
        metavar="<type>",
    )


@command
def build(args: argparse.Namespace) -> None:
    """Build a recipe (and its dependencies) from source."""
    name: str = args.recipe
    build_type: str = args.build_type

    cwd = Path.cwd()
    recipes_root = cwd / "recipes"
    build_root = cwd / "build"

    if not recipes_root.exists():
        print(f"[thirdparty] error: no 'recipes/' directory in {cwd}", file=sys.stderr)
        sys.exit(1)

    _build_recipe(recipes_root, build_root, name, build_type, set())


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _load_recipe_class(recipes_root: Path, name: str) -> type[RecipeBase]:
    recipe_path = recipes_root / name / "recipe.py"
    if not recipe_path.exists():
        print(f"[thirdparty] error: recipe not found: {recipe_path}", file=sys.stderr)
        sys.exit(1)

    spec = importlib.util.spec_from_file_location(f"_recipe_{name}", recipe_path)
    if spec is None or spec.loader is None:
        print(f"[thirdparty] error: cannot load {recipe_path}", file=sys.stderr)
        sys.exit(1)

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[union-attr]

    cls = getattr(module, "Recipe", None)
    if cls is None or not (isinstance(cls, type) and issubclass(cls, RecipeBase)):
        print(
            f"[thirdparty] error: {recipe_path} must define a class named 'Recipe' "
            "that subclasses RecipeBase",
            file=sys.stderr,
        )
        sys.exit(1)

    return cls


def _resolve_version(recipe_cls: type[RecipeBase]) -> str:
    v = getattr(recipe_cls, "version", None)
    return str(v) if v else "latest"


def _instantiate(
    recipe_cls: type[RecipeBase],
    recipes_root: Path,
    build_root: Path,
    name: str,
    version: str,
    build_type: str,
) -> RecipeBase:
    recipe = recipe_cls()
    recipe.version = version
    recipe.recipe_folder = str(recipes_root / name)
    recipe.source_folder = str(build_root / name / version / "source")
    recipe.build_folder = str(build_root / name / version / "build")
    recipe.package_folder = str(build_root / name / version / "package")
    recipe.build_type = build_type
    return recipe


def _is_built(build_root: Path, name: str, version: str) -> bool:
    pkg = build_root / name / version / "package"
    return pkg.exists() and any(pkg.iterdir())


def _collect_dep_paths(
    recipes_root: Path, build_root: Path, deps: list[str]
) -> list[str]:
    paths: list[str] = []
    for dep in deps:
        dep_cls = _load_recipe_class(recipes_root, dep)
        ver = _resolve_version(dep_cls)
        pkg = build_root / dep / ver / "package"
        if pkg.exists():
            paths.append(str(pkg))
    return paths


def _collect_dep_info(
    recipes_root: Path, build_root: Path, deps: list[str], build_type: str = "Release"
) -> dict[str, DepInfo]:
    info: dict[str, DepInfo] = {}
    for dep in deps:
        dep_cls = _load_recipe_class(recipes_root, dep)
        ver = _resolve_version(dep_cls)
        pkg = build_root / dep / ver / "package"
        if pkg.exists():
            dep_recipe = _instantiate(dep_cls, recipes_root, build_root, dep, ver, build_type)
            dep_recipe.package_info()
            info[dep] = DepInfo(
                package_folder=str(pkg),
                name=dep,
                version=ver,
                cpp_info=dep_recipe.cpp_info,
            )
    return info


def _build_recipe(
    recipes_root: Path,
    build_root: Path,
    name: str,
    build_type: str,
    visited: set[str],
) -> None:
    if name in visited:
        return
    visited.add(name)

    recipe_cls = _load_recipe_class(recipes_root, name)
    version = _resolve_version(recipe_cls)

    if _is_built(build_root, name, version):
        print(f"[thirdparty] {name}/{version} already built — skipping")
        return

    # Probe requirements without setting up build folders
    probe = _instantiate(recipe_cls, recipes_root, build_root, name, version, build_type)
    deps = probe.requirements()
    for dep in deps:
        _build_recipe(recipes_root, build_root, dep, build_type, visited)

    print(f"\n[thirdparty] === Building {name}/{version} ({build_type}) ===\n")

    recipe = _instantiate(recipe_cls, recipes_root, build_root, name, version, build_type)
    Path(recipe.source_folder).mkdir(parents=True, exist_ok=True)
    Path(recipe.build_folder).mkdir(parents=True, exist_ok=True)
    Path(recipe.package_folder).mkdir(parents=True, exist_ok=True)

    recipe.dep_package_paths = _collect_dep_paths(recipes_root, build_root, deps)
    recipe.dependencies = _collect_dep_info(recipes_root, build_root, deps, build_type)

    recipe.source()
    recipe.generate()
    recipe.build()
    recipe.package()

    print(f"[thirdparty] {name}/{version} done -> {recipe.package_folder}")
