from __future__ import annotations

import argparse
import importlib.util
import sys
from collections import OrderedDict
from pathlib import Path

from thirdparty._conan.internal.model.conan_file import ConanFile
from thirdparty._conan.internal.model.dependencies import ConanFileDependencies
from thirdparty._conan.tools.env import Environment
from thirdparty._conan.tools.env.environment import generate_aggregated_env
from thirdparty._host.detect import detect_settings, make_conf
from thirdparty.cli.command import command


class _PassthroughWrapper:
    def wrap(self, cmd, **_kw):
        return cmd


class _ConanHelpers:
    def __init__(self, conf):
        self.cmd_wrapper = _PassthroughWrapper()
        self.global_conf = conf
        self.requester = None
        self.cache = None
        self.home_folder = None
        self.conan_api = None


def setup_parser(p: argparse.ArgumentParser) -> None:
    p.add_argument("recipe", metavar="<recipe>", help="Recipe name to build")
    p.add_argument(
        "--build-type",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo"],
        dest="build_type",
        metavar="<type>",
    )
    p.add_argument(
        "--jobs", "-j",
        type=int,
        default=None,
        dest="jobs",
        metavar="<N>",
        help="Parallel build jobs (default: cpu count)",
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

    _build_recipe(recipes_root, build_root, name, build_type, set(), jobs=args.jobs)


def _load_recipe_class(recipes_root: Path, name: str) -> type[ConanFile]:
    recipe_path = recipes_root / name / "recipe.py"
    if not recipe_path.exists():
        print(f"[thirdparty] error: recipe not found: {recipe_path}", file=sys.stderr)
        sys.exit(1)

    spec = importlib.util.spec_from_file_location(f"_recipe_{name}", recipe_path)
    if spec is None or spec.loader is None:
        print(f"[thirdparty] error: cannot load {recipe_path}", file=sys.stderr)
        sys.exit(1)

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    cls = getattr(module, "Recipe", None)
    if cls is None or not (isinstance(cls, type) and issubclass(cls, ConanFile)):
        print(
            f"[thirdparty] error: {recipe_path} must define a class named 'Recipe' "
            "that subclasses ConanFile (or RecipeBase)",
            file=sys.stderr,
        )
        sys.exit(1)

    return cls


def _resolve_version(recipe_cls: type[ConanFile]) -> str:
    v = getattr(recipe_cls, "version", None)
    return str(v) if v else "latest"


def _instantiate(
    recipe_cls: type[ConanFile],
    recipes_root: Path,
    build_root: Path,
    name: str,
    version: str,
    build_type: str,
    jobs: int | None = None,
) -> ConanFile:
    recipe = recipe_cls(display_name=name)
    recipe.version = version
    recipe.recipe_folder = str(recipes_root / name)

    source_dir = str(build_root / name / version / "source")
    build_dir = str(build_root / name / version / "build")
    pkg_dir = str(build_root / name / version / "package")
    gen_dir = build_dir

    recipe.folders.set_base_source(source_dir)
    recipe.folders.set_base_build(build_dir)
    recipe.folders.set_base_package(pkg_dir)
    recipe.folders.set_base_generators(gen_dir)
    recipe.folders.set_base_recipe_metadata(str(build_root / name / version / ".metadata"))

    recipe.settings = detect_settings(build_type)
    recipe.settings_build = recipe.settings
    recipe.settings_target = None
    conf = make_conf(jobs=jobs)
    recipe.conf = conf
    recipe._conan_helpers = _ConanHelpers(conf)
    recipe._conan_dependencies = ConanFileDependencies(OrderedDict())
    recipe._conan_buildenv = Environment()
    recipe._conan_runenv = Environment()

    return recipe


def _get_requires(recipe: ConanFile) -> list[str]:
    try:
        recipe.requirements()
    except Exception:
        pass
    names: list[str] = []
    for req in recipe.requires.values():
        ref = req.ref
        names.append(str(ref.name))
    return names


def _is_built(build_root: Path, name: str, version: str) -> bool:
    pkg = build_root / name / version / "package"
    return pkg.exists() and any(pkg.iterdir())


def _build_recipe(
    recipes_root: Path,
    build_root: Path,
    name: str,
    build_type: str,
    visited: set[str],
    jobs: int | None = None,
) -> None:
    if name in visited:
        return
    visited.add(name)

    recipe_cls = _load_recipe_class(recipes_root, name)
    version = _resolve_version(recipe_cls)

    if _is_built(build_root, name, version):
        print(f"[thirdparty] {name}/{version} already built — skipping")
        return

    probe = _instantiate(recipe_cls, recipes_root, build_root, name, version, build_type, jobs=jobs)
    deps = _get_requires(probe)
    for dep in deps:
        _build_recipe(recipes_root, build_root, dep, build_type, visited, jobs=jobs)

    print(f"\n[thirdparty] === Building {name}/{version} ({build_type}) ===\n")

    recipe = _instantiate(recipe_cls, recipes_root, build_root, name, version, build_type, jobs=jobs)
    Path(recipe.source_folder).mkdir(parents=True, exist_ok=True)
    Path(recipe.build_folder).mkdir(parents=True, exist_ok=True)
    Path(recipe.package_folder).mkdir(parents=True, exist_ok=True)

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

    if hasattr(recipe, "source"):
        recipe.source()
    if hasattr(recipe, "generate"):
        recipe.generate()
    generate_aggregated_env(recipe)
    if hasattr(recipe, "build"):
        recipe.build()
    if hasattr(recipe, "package"):
        recipe.package()

    print(f"[thirdparty] {name}/{version} done -> {recipe.package_folder}")
