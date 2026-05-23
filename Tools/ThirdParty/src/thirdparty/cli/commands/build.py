from __future__ import annotations

import argparse
import importlib.util
import os
import shutil
import sys
import yaml
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
    # export_sources_folder = parent of source_dir; recipes place auxiliary files here
    # (CMakeLists.txt, patches, etc.) via export_sources() in real Conan
    recipe.folders.set_base_export_sources(str(build_root / name / version))

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
    _iface_cache: dict | None = None,
) -> ConanFileDependencies:
    """Create a ConanFileDependencies from a list of already-built packages.

    dep_names  — host (non-build) deps (build=False)
    tool_names — tool_requires (build=True); optional

    _iface_cache is a shared dict[dep_name → (Requirement, ConanFileInterface)] passed through
    recursive calls so that the *same* ConanFileInterface object is reused whenever the same
    package appears at multiple levels of the dep tree.  This is required for
    get_transitive_requires() (used by CMakeDeps) to correctly resolve header-transitive deps:
    it compares ConanFileInterface objects by identity, so the object for e.g. fast_float must
    be the same instance whether it appears in c4core's dep graph or in rapidyaml's.
    """
    deps_dict: OrderedDict = OrderedDict()

    if _iface_cache is None:
        _iface_cache = {}

    def _add_dep(dep_name: str, is_build: bool, direct: bool = True) -> None:
        # If we already created a ConanFileInterface for this dep in this tree, reuse it.
        # This ensures object identity holds across all levels (needed by transitive_requires).
        if dep_name in _iface_cache:
            cached_req, cached_iface = _iface_cache[dep_name]
            # Add to the current level's deps_dict with the appropriate directness flag.
            new_req = Requirement(cached_req.ref, build=is_build, direct=direct)
            if not any(str(r.ref.name) == dep_name for r in deps_dict.keys()):
                deps_dict[new_req] = cached_iface
            return

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

        # Register in cache BEFORE recursing to handle diamond/cycle deps.
        ref = RecipeReference(dep_name, dep_version)
        if is_build:
            req = Requirement(ref, headers=False, libs=False, build=True, run=True,
                              visible=False, direct=direct)
        else:
            req = Requirement(ref, build=False, direct=direct)
        iface = ConanFileInterface(dep, None)
        _iface_cache[dep_name] = (req, iface)
        deps_dict[req] = iface

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
                _iface_cache=_iface_cache,
            )
        except Exception:
            dep._conan_dependencies = ConanFileDependencies(OrderedDict())

        # Also propagate all transitive (non-direct) deps of this dep up to the current
        # deps_dict.  This ensures that when CMakeDeps calls get_transitive_requires() to
        # resolve header-transitive dependencies (transitive_headers=True), the transitive
        # packages are present in the consumer's dep graph with the same interface objects.
        for trans_req, trans_iface in dep._conan_dependencies._data.items():
            trans_name = str(trans_req.ref.name)
            if not any(str(r.ref.name) == trans_name for r in deps_dict.keys()):
                non_direct_req = Requirement(trans_req.ref, build=trans_req.build, direct=False)
                deps_dict[non_direct_req] = trans_iface

        if hasattr(dep, "package_info"):
            try:
                dep.package_info()
            except Exception as exc:
                print(f"[thirdparty] warn: package_info() failed for {dep_name}: {exc}")

        # Make all relative cpp_info paths absolute so generators don't assert.
        dep.cpp_info.set_relative_base_folder(pkg_dir)

    for dep_name in dep_names:
        _add_dep(dep_name, is_build=False, direct=True)
    for dep_name in (tool_names or []):
        _add_dep(dep_name, is_build=True, direct=True)

    return ConanFileDependencies(deps_dict)




def _is_built(build_root: Path, name: str, version: str) -> bool:
    pkg = build_root / name / version / "package"
    return pkg.exists() and any(pkg.iterdir())


def _load_conandata(recipe) -> None:
    """If the recipe directory contains a conandata.yml, load it and expose it via
    recipe.conan_data so that recipes can access custom version-keyed data beyond
    sources/patches (e.g. zlib-ng's ``zlib_compat`` field)."""
    conandata_path = Path(recipe.recipe_folder) / "conandata.yml"
    if conandata_path.is_file():
        with open(conandata_path, "r", encoding="utf-8") as fh:
            data = yaml.safe_load(fh) or {}
        recipe.conan_data = data


_RECIPE_SKIP_NAMES = {"recipe.py", "__pycache__"}

def _copy_recipe_export_sources(recipe_dir: Path, export_dir: Path) -> None:
    """Copy auxiliary files (CMakeLists.txt, patches/, etc.) from the recipe directory
    to export_dir (the build-time export_sources_folder).  This mirrors what Conan's
    export_sources() / cache layer does before calling source() and build()."""
    export_dir.mkdir(parents=True, exist_ok=True)
    for item in recipe_dir.iterdir():
        if item.name in _RECIPE_SKIP_NAMES or item.name.startswith("."):
            continue
        dst = export_dir / item.name
        if item.is_file():
            shutil.copy2(item, dst)
        elif item.is_dir():
            shutil.copytree(item, dst, dirs_exist_ok=True)


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

    # Implicitly inject ninja for recipes that use cmake or meson as build tools.
    if any(t in direct_tools for t in ("cmake", "meson")) and "ninja" not in direct_tools:
        if (recipes_root / "ninja" / "recipe.py").exists():
            direct_tools.append("ninja")

    # Recursively build tool dependencies that have local recipes.
    for tool_name in direct_tools:
        if (recipes_root / tool_name / "recipe.py").exists():
            _build_recipe(recipes_root, build_root, tool_name, build_type, visited,
                          jobs=jobs, generate_only=generate_only)

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

    # Propagate conf_info from tool dependencies into recipe.conf so that, e.g.,
    # msys2's conf_info (bash:subsystem, bash:path) is visible when generate()/build() run.
    for _req, _dep_iface in dep_graph._data.items():
        if _req.build:
            _dep_conf_info = _dep_iface.conf_info
            if _dep_conf_info:
                recipe.conf.compose_conf(_dep_conf_info)

    # Wire up build_requires/tool_requires wrappers then call build_requirements() so
    # recipes can set win_bash=True and add further tool_requires.
    from thirdparty._conan.internal.model.requires import (
        BuildRequirements, TestRequirements, ToolRequirements,
    )
    recipe.build_requires = BuildRequirements(recipe.requires)
    recipe.test_requires = TestRequirements(recipe.requires)
    recipe.tool_requires = ToolRequirements(recipe.requires)
    if hasattr(recipe, "build_requirements"):
        try:
            recipe.build_requirements()
        except Exception:
            pass

    # Load conandata.yml from the recipe folder if present.  Some recipes access
    # self.conan_data for custom version data (e.g. zlib-ng's "zlib_compat" field).
    _load_conandata(recipe)

    Path(recipe.source_folder).mkdir(parents=True, exist_ok=True)
    Path(recipe.build_folder).mkdir(parents=True, exist_ok=True)
    Path(recipe.package_folder).mkdir(parents=True, exist_ok=True)

    # Mirror what Conan's export_sources phase does: copy auxiliary recipe files
    # (CMakeLists.txt, patches/, etc.) to export_sources_folder so that:
    #   - cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
    #     can find a CMakeLists.txt one level above source_folder
    #   - apply_patches() / patch tools can locate patch files
    _copy_recipe_export_sources(Path(recipe.recipe_folder), Path(recipe.export_sources_folder))

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
    if hasattr(recipe, "validate"):
        try:
            recipe.validate()
        except Exception as _val_exc:
            from thirdparty._conan.errors import ConanInvalidConfiguration
            if isinstance(_val_exc, ConanInvalidConfiguration):
                print(f"[thirdparty] {name}/{version} not supported on this platform: {_val_exc} — skipping")
                return transitive
            raise

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
            _orig_cwd_build = os.getcwd()
            try:
                os.chdir(recipe.build_folder)
                recipe.build()
            finally:
                os.chdir(_orig_cwd_build)
        if hasattr(recipe, "package"):
            _orig_cwd_pkg = os.getcwd()
            try:
                os.chdir(recipe.build_folder)
                recipe.package()
            finally:
                os.chdir(_orig_cwd_pkg)

    print(f"[thirdparty] {name}/{version} done -> {recipe.package_folder}")
    return transitive
