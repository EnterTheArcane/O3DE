from __future__ import annotations

import argparse
import fnmatch
import importlib.util
import os
import shutil
import sys
import time
import yaml
from collections import OrderedDict
from pathlib import Path

from conan2.api.model.refs import RecipeReference
from thirdparty.cps.cps import CPS
from conan2.internal.graph.graph import CONTEXT_HOST, RECIPE_INCACHE
from conan2.internal.model.conan_file import ConanFile
from conan2.internal.model.conanfile_interface import ConanFileInterface
from conan2.internal.model.dependencies import ConanFileDependencies
from conan2.internal.model.requires import Requirement
from conan2.internal.rest.conan_requester import ConanRequester
from conan2.tools.env import Environment
from conan2.tools.env.environment import generate_aggregated_env
from thirdparty._host.detect import detect_settings, make_conf
from thirdparty.cli.command import command


class _PassthroughWrapper:
    def wrap(self, cmd, **_kw):
        return cmd


class _ConanHelpers:
    def __init__(self, conf):
        self.cmd_wrapper = _PassthroughWrapper()
        self.global_conf = conf
        self.requester = ConanRequester(conf)
        self.cache = None
        self.home_folder = None
        self.conan_api = None


class _FakeNode:
    """Minimal stand-in for conan's graph Node, giving dep conanfiles a ref/context/recipe."""

    def __init__(self, name: str, version: str) -> None:
        self.ref = RecipeReference(name, version)
        self.context = CONTEXT_HOST  # "host"
        self.recipe = RECIPE_INCACHE  # any non-RECIPE_PLATFORM value


class _VersionResolvingRequirements:
    """Wraps conan's Requirements object to accept bare package names (no version).

    Recipes in this system use ``self.requires("abseil")`` without a version.  Conan 2.x
    rejects that, so we intercept every call, look up the matching local recipe to find its
    version, and convert the ref to ``"abseil/20260107.1"`` before forwarding.
    """

    def __init__(self, inner, recipes_root: Path) -> None:
        self._inner = inner
        self._recipes_root = recipes_root

    def _resolve(self, ref: str) -> str:
        if ref and "/" not in ref and "@" not in ref:
            cls = _try_load_recipe_class(self._recipes_root, ref)
            if cls:
                return f"{ref}/{_resolve_version(cls)}"
        return ref

    def __call__(self, str_ref, **kwargs):
        return self._inner(self._resolve(str_ref), **kwargs)

    def tool_require(self, ref, **kwargs):
        resolved = self._resolve(ref)
        # Skip tool deps that have no local recipe and no explicit version — they are
        # system-provided tools (e.g. gperf, pkg-config) and cannot be version-resolved.
        # Passing an unversioned name to Requirements.tool_require() raises an error that
        # would abort the entire build_requirements() call, preventing later tool_requires
        # (e.g. meson) from being registered.
        if resolved and "/" not in resolved:
            return
        return self._inner.tool_require(resolved, **kwargs)

    def __getattr__(self, name):
        return getattr(self._inner, name)


def setup_parser(p: argparse.ArgumentParser) -> None:
    p.add_argument("recipe", metavar="<recipe>", nargs="*",
                   help="Recipe name(s) or glob pattern(s) to build (e.g. 'abseil' or '*'); omit to build all")
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
    p.add_argument("--resume", metavar="NAME", default=None,
                   help="Skip all recipes before NAME in build order (multi-recipe builds)")
    p.add_argument("--dry-run", action="store_true",
                   help="Print build plan without building")
    p.add_argument("--force", action="store_true",
                   help="Rebuild even if already built")
    p.add_argument("--fail-fast", action="store_true", dest="fail_fast",
                   help="Stop after the first recipe failure")


@command
def build(args: argparse.Namespace) -> None:
    """Build a recipe (and its dependencies) from source."""
    patterns: list[str] = args.recipe or ["*"]
    build_type: str = args.build_type
    generate_only: bool = getattr(args, "generate_only", False)
    force: bool = getattr(args, "force", False)
    resume: str | None = getattr(args, "resume", None)
    dry_run: bool = getattr(args, "dry_run", False)
    fail_fast: bool = getattr(args, "fail_fast", False)

    cwd = Path.cwd()
    recipes_root = cwd / "recipes"
    build_root = cwd / "build"

    if not recipes_root.exists():
        print(f"[thirdparty] error: no 'recipes/' directory in {cwd}", file=sys.stderr)
        sys.exit(1)

    all_names = sorted(d.name for d in recipes_root.iterdir() if d.is_dir())
    all_names_set = set(all_names)
    names: list[str] = []
    is_multi = len(patterns) > 1 or any(c in pat for pat in patterns for c in ('*', '?', '['))
    for pat in patterns:
        if any(c in pat for c in ('*', '?', '[')):
            matched = fnmatch.filter(all_names, pat)
            if not matched:
                print(f"[thirdparty] warn: no recipes match '{pat}'")
            for m in matched:
                if m not in names:
                    names.append(m)
        else:
            if pat not in all_names_set:
                print(f"[thirdparty] error: recipe not found: {pat}", file=sys.stderr)
                sys.exit(1)
            if pat not in names:
                names.append(pat)

    if not names:
        print("[thirdparty] error: no recipes matched", file=sys.stderr)
        sys.exit(1)

    if is_multi or resume or dry_run:
        _build_ordered(
            recipes_root, build_root, names, build_type,
            jobs=args.jobs, resume=resume, dry_run=dry_run,
            force=force, generate_only=generate_only, fail_fast=fail_fast,
        )
    else:
        _build_recipe(recipes_root, build_root, names[0], build_type, set(),
                      jobs=args.jobs, generate_only=generate_only, force=force)


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
    recipe.requires = _VersionResolvingRequirements(recipe.requires, recipes_root)

    return recipe


def _get_requires(recipe: ConanFile) -> tuple[list[str], list[str]]:
    """Call requirements() and build_requirements() and return (host_dep_names, tool_dep_names).

    host_dep_names  — regular library dependencies (build=False)
    tool_dep_names  — tool_requires / build_requires (build=True)
    """
    # Wire up callable wrappers that conan's graph resolution normally sets before calling these
    # methods. Without them, recipe.tool_requires("foo") raises TypeError (calls None).
    from conan2.internal.model.requires import (
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
        dep.requires = _VersionResolvingRequirements(dep.requires, recipes_root)

        dep._conan_node = _FakeNode(dep_name, dep_version)

        for method_name in ("config_options", "configure"):
            if hasattr(dep, method_name):
                try:
                    getattr(dep, method_name)()
                except Exception:
                    pass

        try:
            from conan2.internal.model.pkg_type import PackageType
            PackageType.compute_package_type(dep)
        except Exception:
            pass

        # Register in cache BEFORE recursing to handle diamond/cycle deps.
        ref = RecipeReference(dep_name, dep_version)
        if is_build:
            req = Requirement(ref, headers=False, libs=False, build=True, run=True,
                              visible=False, direct=direct)
        else:
            # Set run=True if the package contains shared libraries so that
            # VirtualRunEnv adds its lib dir to DYLD_LIBRARY_PATH / LD_LIBRARY_PATH.
            import platform as _platform
            _lib_path = Path(pkg_dir) / "lib"
            _sys = _platform.system()
            if _sys == "Darwin":
                _pattern = "*.dylib"
            elif _sys == "Windows":
                _pattern = "*.dll"
            else:
                _pattern = "*.so*"
            _is_shared = _lib_path.is_dir() and any(_lib_path.glob(_pattern))
            req = Requirement(ref, build=False, run=_is_shared, direct=direct)
        iface = ConanFileInterface(dep, None)
        _iface_cache[dep_name] = (req, iface)
        deps_dict[req] = iface

        # Populate dep's own transitive dep graph so generators can resolve
        # component dependencies (e.g. spirv-tools-core → spirv-headers).
        try:
            from conan2.internal.model.requires import (
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


_COMPLETE_MARKER = ".complete"


class _CPSDepProxy:
    """Minimal duck-type proxy satisfying CPS.from_conan()'s attribute requirements."""

    class _EmptyHost:
        def values(self):
            return []
        def items(self):
            return []
        def __bool__(self):
            return False

    class _EmptyDependencies:
        def __init__(self):
            self.host = _CPSDepProxy._EmptyHost()
        def __bool__(self):
            return False

    def __init__(self, recipe, name: str, version: str):
        self.ref = RecipeReference(name, version)
        self.package_folder = recipe.package_folder
        self.license = getattr(recipe, "license", None)
        self.description = getattr(recipe, "description", None)
        self.homepage = getattr(recipe, "homepage", None)
        self.settings = recipe.settings
        self.cpp_info = recipe.cpp_info
        self.languages = getattr(recipe, "languages", [])
        try:
            from conan2.internal.model.pkg_type import PackageType as _PT
            _PT.compute_package_type(recipe)
        except Exception:
            pass
        self.package_type = getattr(recipe, "package_type", None)
        self.dependencies = _CPSDepProxy._EmptyDependencies()

    def __str__(self):
        return str(self.ref)


def _cps_file(pkg_dir: Path, name: str) -> Path:
    return pkg_dir / "cps" / name / f"{name}.cps"


def _generate_cps(recipe, name: str, version: str, pkg_dir: Path) -> None:
    from ...cmake.cmake_config import generate as generate_cmake_config
    try:
        if hasattr(recipe, "package_info"):
            recipe.package_info()
        recipe.cpp_info.set_relative_base_folder(str(pkg_dir))
        proxy = _CPSDepProxy(recipe, name, version)
        cps = CPS.from_conan(proxy)
        cps.cps_path = f"@prefix@/cps"
        cps_dir = pkg_dir / "cps"
        cps_dir.mkdir(parents=True, exist_ok=True)
        cps.save(str(cps_dir))
        #generate_cmake_config(cps, pkg_dir)
    except Exception as exc:
        print(f"[thirdparty] warn: CPS generation failed for {name}: {exc}")


def _is_built(build_root: Path, name: str, version: str) -> bool:
    return (build_root / name / version / "build" / _COMPLETE_MARKER).is_file()


def _is_sourced(build_root: Path, name: str, version: str) -> bool:
    return (build_root / name / version / "source" / _COMPLETE_MARKER).is_file()


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


def _try_load_recipe_class(recipes_root: Path, name: str) -> type[ConanFile] | None:
    recipe_path = recipes_root / name / "recipe.py"
    if not recipe_path.exists():
        return None
    try:
        spec = importlib.util.spec_from_file_location(f"_recipe_{name}", recipe_path)
        if spec is None or spec.loader is None:
            return None
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        cls = getattr(module, "Recipe", None)
        return cls if (cls and isinstance(cls, type) and issubclass(cls, ConanFile)) else None
    except Exception:
        return None


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
    for n in graph:
        if n not in order:
            order.append(n)
    return order


def _build_ordered(
    recipes_root: Path,
    build_root: Path,
    names: list[str],
    build_type: str,
    jobs: int | None,
    resume: str | None,
    dry_run: bool,
    force: bool,
    generate_only: bool,
    fail_fast: bool = False,
) -> None:
    known = set(names)
    graph: dict[str, list[str]] = {}
    for name in names:
        cls = _try_load_recipe_class(recipes_root, name)
        if cls is None:
            graph[name] = []
            continue
        version = _resolve_version(cls)
        try:
            probe = _instantiate(cls, recipes_root, build_root, name, version, build_type, jobs=jobs)
            host_deps, tool_deps = _get_requires(probe)
        except Exception:
            host_deps, tool_deps = [], []
        graph[name] = [d for d in host_deps + tool_deps if d in known]

    order = _topo_sort(graph)

    if resume:
        if resume not in order:
            print(f"[thirdparty] error: --resume '{resume}' not found in build order",
                  file=sys.stderr)
            sys.exit(1)
        order = order[order.index(resume):]

    print(f"\n=== Build Plan: {len(order)} recipes ({build_type}) ===")
    for i, name in enumerate(order, 1):
        cls = _try_load_recipe_class(recipes_root, name)
        version = _resolve_version(cls) if cls else "?"
        built = _is_built(build_root, name, version)
        status = "[force]" if force else ("[built]" if built else "[pending]")
        print(f"  {i:3d}. {name}/{version}  {status}")

    if dry_run:
        return

    print()

    visited: set[str] = set()
    results: list[tuple[str, str, float, str | None]] = []
    skipped: list[str] = []

    for name in order:
        cls = _try_load_recipe_class(recipes_root, name)
        if cls is None:
            print(f"[thirdparty] SKIP {name} — cannot load recipe", file=sys.stderr)
            skipped.append(name)
            continue
        version = _resolve_version(cls)
        if not force and _is_built(build_root, name, version):
            skipped.append(name)
            visited.add(name)
            pkg_dir = build_root / name / version / "package"
            if not _cps_file(pkg_dir, name).exists():
                probe_cps = _instantiate(cls, recipes_root, build_root, name, version, build_type, jobs=jobs)
                _generate_cps(probe_cps, name, version, pkg_dir)
            continue
        t0 = time.time()
        try:
            _build_recipe(recipes_root, build_root, name, build_type, visited,
                          jobs=jobs, generate_only=generate_only, force=force)
            elapsed = time.time() - t0
            results.append((name, version, elapsed, None))
        except Exception as exc:
            elapsed = time.time() - t0
            results.append((name, version, elapsed, str(exc)))
            print(f"[thirdparty] FAIL {name}/{version}: {exc}")
            if fail_fast:
                import traceback
                traceback.print_exc()
                sys.exit(1)

    ok   = [(n, v, t) for n, v, t, e in results if e is None]
    fail = [(n, v, e) for n, v, t, e in results if e is not None]

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


def _build_recipe(
    recipes_root: Path,
    build_root: Path,
    name: str,
    build_type: str,
    visited: set[str],
    jobs: int | None = None,
    generate_only: bool = False,
    force: bool = False,
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
                          jobs=jobs, generate_only=generate_only, force=force)

    # Recursively build deps and collect their full transitive dep lists.
    transitive: list[str] = []
    for dep_name in direct_deps:
        if not (recipes_root / dep_name / "recipe.py").exists():
            print(f"[thirdparty] warn: dep recipe not found, skipping: {dep_name}")
            continue
        sub = _build_recipe(recipes_root, build_root, dep_name, build_type, visited,
                            jobs=jobs, generate_only=generate_only, force=force)
        for d in sub:
            if d not in transitive:
                transitive.append(d)
        # dep_name itself is in visited after the recursive call (either it was
        # already there or it was just built).  Add it to our list if not yet present.
        if dep_name not in transitive:
            transitive.append(dep_name)

    if not generate_only and not force and _is_built(build_root, name, version):
        print(f"[thirdparty] {name}/{version} already built — skipping")
        pkg_dir_cps = build_root / name / version / "package"
        if not _cps_file(pkg_dir_cps, name).exists():
            _generate_cps(probe, name, version, pkg_dir_cps)
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
    from conan2.internal.model.requires import (
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

    pkg_dir = Path(recipe.package_folder)
    build_dir = Path(recipe.build_folder)
    src_dir = Path(recipe.source_folder)

    if force:
        # Wipe all build artifacts so source(), build(), and package() run fresh.
        for _dir in (src_dir, build_dir, pkg_dir):
            shutil.rmtree(_dir, ignore_errors=True)
    elif pkg_dir.exists() and not (build_dir / _COMPLETE_MARKER).is_file():
        # If the package directory exists but the completion marker is absent, the
        # previous build was interrupted or failed mid-package().  Remove the partial
        # package directory so we start clean.
        shutil.rmtree(pkg_dir, ignore_errors=True)

    # Run config_options / configure / validate before creating any directories so
    # that packages which are not supported on this platform (ConanInvalidConfiguration)
    # don't leave empty build trees behind.
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
            from conan2.errors import ConanInvalidConfiguration
            if isinstance(_val_exc, ConanInvalidConfiguration):
                print(f"[thirdparty] {name}/{version} not supported on this platform: {_val_exc} — skipping")
                return transitive
            raise

    # Mirror what Conan's export_sources phase does: copy auxiliary recipe files
    # (CMakeLists.txt, patches/, etc.) to export_sources_folder so that:
    #   - cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
    #     can find a CMakeLists.txt one level above source_folder
    #   - apply_patches() / patch tools can locate patch files
    Path(recipe.export_sources_folder).mkdir(parents=True, exist_ok=True)
    _copy_recipe_export_sources(Path(recipe.recipe_folder), Path(recipe.export_sources_folder))

    if not generate_only and hasattr(recipe, "source"):
        src_folder = Path(recipe.source_folder)
        src_folder.mkdir(parents=True, exist_ok=True)
        # Only run source() once per package; skip if already completed successfully.
        if not _is_sourced(build_root, name, version):
            # Wipe any partial state from a previous failed source() attempt
            shutil.rmtree(src_folder, ignore_errors=True)
            src_folder.mkdir(parents=True, exist_ok=True)
            recipe.source()
            (src_folder / _COMPLETE_MARKER).write_text("")
    gen_folder = recipe.generators_folder if hasattr(recipe, "generate") else None
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
    # system conan 2.27.1 generates conandeps_legacy.cmake but recipes may expect conan_deps.cmake
    if gen_folder:
        _legacy = Path(gen_folder) / "conandeps_legacy.cmake"
        _conan_deps = Path(gen_folder) / "conan_deps.cmake"
        if _legacy.is_file() and not _conan_deps.is_file():
            _conan_deps.write_text(_legacy.read_text(encoding="utf-8"), encoding="utf-8")
    generate_aggregated_env(recipe)
    if not generate_only:
        if hasattr(recipe, "build"):
            Path(recipe.build_folder).mkdir(parents=True, exist_ok=True)
            _orig_cwd_build = os.getcwd()
            try:
                os.chdir(recipe.build_folder)
                recipe.build()
            except Exception:
                # Clean up so that the next run starts fresh rather than resuming a broken state.
                shutil.rmtree(recipe.build_folder, ignore_errors=True)
                shutil.rmtree(recipe.package_folder, ignore_errors=True)
                raise
            finally:
                os.chdir(_orig_cwd_build)
        if hasattr(recipe, "package"):
            Path(recipe.build_folder).mkdir(parents=True, exist_ok=True)
            shutil.rmtree(recipe.package_folder, ignore_errors=True)
            Path(recipe.package_folder).mkdir(parents=True, exist_ok=True)
            _orig_cwd_pkg = os.getcwd()
            try:
                os.chdir(recipe.build_folder)
                recipe.package()
            except Exception:
                shutil.rmtree(recipe.package_folder, ignore_errors=True)
                raise
            finally:
                os.chdir(_orig_cwd_pkg)
        # Write the completion marker only after both build() and package() succeed.
        build_dir.mkdir(parents=True, exist_ok=True)
        (build_dir / _COMPLETE_MARKER).write_text("")

    _generate_cps(recipe, name, version, pkg_dir)
    print(f"[thirdparty] {name}/{version} done -> {recipe.package_folder}")
    return transitive
