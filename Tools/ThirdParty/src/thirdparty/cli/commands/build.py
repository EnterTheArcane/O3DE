from __future__ import annotations

import argparse
import importlib.util
import os
import sys
from collections import OrderedDict
from pathlib import Path

# Names that the porter may strip from imports but that recipes still use at runtime.
# We inject them into each recipe module's namespace after loading.
from thirdparty._conan.tools.env.virtualbuildenv import VirtualBuildEnv as _VirtualBuildEnv
from thirdparty._conan.tools.env.virtualrunenv import VirtualRunEnv as _VirtualRunEnv
from thirdparty._conan import conan_version as _conan_version
_RECIPE_INJECT: dict[str, object] = {
    "VirtualBuildEnv": _VirtualBuildEnv,
    "VirtualRunEnv":   _VirtualRunEnv,
    "conan_version":   _conan_version,
}

from thirdparty._conan.api.model.refs import RecipeReference
from thirdparty._conan.internal.graph.graph import CONTEXT_HOST, RECIPE_INCACHE
from thirdparty._conan.internal.model.conan_file import ConanFile
from thirdparty._conan.internal.model.conanfile_interface import ConanFileInterface
from thirdparty._conan.internal.model.dependencies import ConanFileDependencies
from thirdparty._conan.internal.model.requires import Requirement
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


class _FakeNode:
    """Minimal stand-in for conan's graph Node, giving dep conanfiles a ref/context/recipe."""

    def __init__(self, name: str, version: str) -> None:
        self.ref = RecipeReference(name, version)
        self.context = CONTEXT_HOST  # "host"
        self.recipe = RECIPE_INCACHE  # any non-RECIPE_PLATFORM value


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
    p.add_argument(
        "--generate-only",
        action="store_true",
        dest="generate_only",
        help="Run only through generate() (no source download, build, or package)",
    )


@command
def build(args: argparse.Namespace) -> None:
    """Build a recipe (and its dependencies) from source."""
    name: str = args.recipe
    build_type: str = args.build_type
    generate_only: bool = getattr(args, "generate_only", False)

    cwd = Path.cwd()
    recipes_root = cwd / "recipes"
    build_root = cwd / "build"

    if not recipes_root.exists():
        print(f"[thirdparty] error: no 'recipes/' directory in {cwd}", file=sys.stderr)
        sys.exit(1)

    _build_recipe(recipes_root, build_root, name, build_type, set(),
                  jobs=args.jobs, generate_only=generate_only)


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

    # Inject names the porter may have stripped from imports (e.g. VirtualBuildEnv).
    for attr_name, obj in _RECIPE_INJECT.items():
        if not hasattr(module, attr_name):
            setattr(module, attr_name, obj)

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


def _get_requires(recipe: ConanFile) -> tuple[list[str], list[str]]:
    """Call requirements() and build_requirements() and return (host_dep_names, tool_dep_names).

    host_dep_names  — regular library dependencies (build=False)
    tool_dep_names  — tool_requires / build_requires (build=True)
    """
    # Wire up callable wrappers that conan's graph resolution normally sets before calling these
    # methods. Without them, recipe.tool_requires("foo") raises TypeError (calls None).
    from thirdparty._conan.internal.model.requires import (
        BuildRequirements, TestRequirements, ToolRequirements,
    )
    recipe.build_requires = BuildRequirements(recipe.requires)
    recipe.test_requires = TestRequirements(recipe.requires)
    recipe.tool_requires = ToolRequirements(recipe.requires)

    # config_options() / configure() must run before requirements() so that
    # option guards like "if self.options.with_elf" reflect the correct value.
    for method in ("config_options", "configure", "requirements", "build_requirements"):
        if hasattr(recipe, method):
            try:
                getattr(recipe, method)()
            except Exception:
                pass
    host_names: list[str] = []
    tool_names: list[str] = []
    for req in recipe.requires.values():
        name = str(req.ref.name)
        if req.build:
            tool_names.append(name)
        else:
            host_names.append(name)
    return host_names, tool_names


def _build_dep_graph(
    recipes_root: Path,
    build_root: Path,
    dep_names: list[str],
    build_type: str,
    jobs: int | None = None,
    tool_names: list[str] | None = None,
) -> ConanFileDependencies:
    """Create a ConanFileDependencies from a list of already-built packages.

    dep_names  — host (non-build) deps (build=False)
    tool_names — tool_requires (build=True); optional
    """
    deps_dict: OrderedDict = OrderedDict()

    def _add_dep(dep_name: str, is_build: bool) -> None:
        recipe_path = recipes_root / dep_name / "recipe.py"
        if not recipe_path.exists():
            print(f"[thirdparty] warn: dep recipe not found, skipping: {dep_name}")
            return

        dep_cls = _load_recipe_class(recipes_root, dep_name)
        dep_version = _resolve_version(dep_cls)
        pkg_dir = str((build_root / dep_name / dep_version / "package").resolve())

        dep = dep_cls(display_name=dep_name)
        dep.version = dep_version
        dep.recipe_folder = str(recipes_root / dep_name)
        dep.folders.set_base_package(pkg_dir)

        dep.settings = detect_settings(build_type)
        dep.settings_build = dep.settings
        dep.settings_target = None
        conf = make_conf(jobs=jobs)
        dep.conf = conf
        dep._conan_helpers = _ConanHelpers(conf)
        dep._conan_dependencies = ConanFileDependencies(OrderedDict())
        dep._conan_buildenv = Environment()
        dep._conan_runenv = Environment()

        dep._conan_node = _FakeNode(dep_name, dep_version)

        for method_name in ("config_options", "configure"):
            if hasattr(dep, method_name):
                try:
                    getattr(dep, method_name)()
                except Exception:
                    pass

        # Populate dep's own transitive dep graph so generators can resolve
        # component dependencies (e.g. spirv-tools-core → spirv-headers).
        try:
            from thirdparty._conan.internal.model.requires import (
                BuildRequirements, TestRequirements, ToolRequirements,
            )
            dep.build_requires = BuildRequirements(dep.requires)
            dep.test_requires = TestRequirements(dep.requires)
            dep.tool_requires = ToolRequirements(dep.requires)
            for _meth in ("requirements", "build_requirements"):
                if hasattr(dep, _meth):
                    try:
                        getattr(dep, _meth)()
                    except Exception:
                        pass
            _sub_host = [str(r.ref.name) for r in dep.requires.values() if not r.build]
            _sub_tools = [str(r.ref.name) for r in dep.requires.values() if r.build]
            dep._conan_dependencies = _build_dep_graph(
                recipes_root, build_root, _sub_host, build_type,
                jobs=jobs, tool_names=_sub_tools,
            )
        except Exception:
            dep._conan_dependencies = ConanFileDependencies(OrderedDict())
        if hasattr(dep, "package_info"):
            try:
                dep.package_info()
            except Exception as exc:
                print(f"[thirdparty] warn: package_info() failed for {dep_name}: {exc}")

        # Make all relative cpp_info paths absolute so generators don't assert.
        dep.cpp_info.set_relative_base_folder(pkg_dir)

        ref = RecipeReference(dep_name, dep_version)
        if is_build:
            # tool_require: build=True, run=True, headers=False, libs=False, visible=False
            req = Requirement(ref, headers=False, libs=False, build=True, run=True,
                              visible=False, direct=True)
        else:
            req = Requirement(ref, build=False, direct=True)
        iface = ConanFileInterface(dep, None)
        deps_dict[req] = iface

    for dep_name in dep_names:
        _add_dep(dep_name, is_build=False)
    for dep_name in (tool_names or []):
        _add_dep(dep_name, is_build=True)

    return ConanFileDependencies(deps_dict)




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
    generate_only: bool = False,
) -> list[str]:
    """Build *name* and all its transitive dependencies.

    Returns the ordered list of all transitive dep names for *name* (deepest
    first) so that the caller can populate its own dep graph.
    """
    if name in visited:
        return []
    visited.add(name)

    recipe_cls = _load_recipe_class(recipes_root, name)
    version = _resolve_version(recipe_cls)

    # Probe the recipe to discover its direct dependencies (even when pre-built,
    # so we can return the correct transitive dep list to our caller).
    probe = _instantiate(recipe_cls, recipes_root, build_root, name, version, build_type, jobs=jobs)
    direct_deps, direct_tools = _get_requires(probe)

    # Recursively build deps and collect their full transitive dep lists.
    transitive: list[str] = []
    for dep_name in direct_deps:
        sub = _build_recipe(recipes_root, build_root, dep_name, build_type, visited,
                            jobs=jobs, generate_only=generate_only)
        for d in sub:
            if d not in transitive:
                transitive.append(d)
        # dep_name itself is in visited after the recursive call (either it was
        # already there or it was just built).  Add it to our list if not yet present.
        if dep_name not in transitive:
            transitive.append(dep_name)

    if not generate_only and _is_built(build_root, name, version):
        print(f"[thirdparty] {name}/{version} already built — skipping")
        return transitive

    # Build the dependency graph for this recipe from all transitive deps.
    dep_graph = _build_dep_graph(recipes_root, build_root, transitive, build_type, jobs=jobs,
                                 tool_names=direct_tools)

    print(f"\n[thirdparty] === Building {name}/{version} ({build_type}) ===\n")

    recipe = _instantiate(recipe_cls, recipes_root, build_root, name, version, build_type, jobs=jobs)
    recipe._conan_dependencies = dep_graph

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

    if not generate_only and hasattr(recipe, "source"):
        recipe.source()
    if hasattr(recipe, "generate"):
        # Conan generators write files with bare filenames and expect CWD == generators_folder
        # (the comment in CMakeDeps says "# Current directory is the generators_folder").
        # We must chdir there before calling generate() so files land in the build tree, not here.
        gen_folder = recipe.generators_folder
        if gen_folder:
            Path(gen_folder).mkdir(parents=True, exist_ok=True)
        _orig_cwd = os.getcwd()
        try:
            if gen_folder:
                os.chdir(gen_folder)
            recipe.generate()
        finally:
            os.chdir(_orig_cwd)
    generate_aggregated_env(recipe)
    if not generate_only:
        if hasattr(recipe, "build"):
            recipe.build()
        if hasattr(recipe, "package"):
            recipe.package()

    print(f"[thirdparty] {name}/{version} done -> {recipe.package_folder}")
    return transitive
