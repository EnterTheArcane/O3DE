from __future__ import annotations

import importlib.util
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path

from thirdparty._internal.detect import detect_settings, make_conf
from thirdparty._internal.graph.graph import CONTEXT_HOST, RECIPE_INCACHE
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty._internal.model.recipe_base import RecipeBase
from thirdparty._internal.model.refs import RecipeReference
from thirdparty._internal.rest.http_requester import HttpRequester
from thirdparty.env import Environment


# ---------------------------------------------------------------------------
# Runtime shims
#
# This system has no Conan cache, remotes, or profiles, so recipes are driven
# directly.  These small stand-ins supply the handful of attributes the recipe
# methods touch during graph resolution and building.
# ---------------------------------------------------------------------------
class PassthroughWrapper:
    def wrap(self, cmd, **_kw):
        return cmd


class RecipeRuntime:
    def __init__(self, conf):
        self.cmd_wrapper = PassthroughWrapper()
        self.global_conf = conf
        self.requester = HttpRequester(conf)
        self.cache = None
        self.home_folder = None
        self.recipe_api = None


class FakeNode:
    """Minimal stand-in for the dependency graph Node, giving dep recipes a ref/context/recipe."""

    def __init__(self, name: str, version: str) -> None:
        self.ref = RecipeReference(name, version)
        self.context = CONTEXT_HOST  # "host"
        self.recipe = RECIPE_INCACHE  # any non-RECIPE_PLATFORM value


class VersionResolvingRequirements:
    """Wraps the requirements object to accept bare package names (no version).

    Recipes in this system use ``self.requires("abseil")`` without a version.  The underlying
    Requirements model rejects that, so we intercept every call, look up the matching local
    recipe to find its version, and convert the ref to ``"abseil/20260107.1"`` before forwarding.
    """

    def __init__(self, inner, recipes_root: Path) -> None:
        self._inner = inner
        self._recipes_root = recipes_root

    def _resolve(self, ref: str) -> str:
        if ref and "/" not in ref and "@" not in ref:
            cls = try_load_recipe_class(self._recipes_root, ref)
            if cls:
                return f"{ref}/{resolve_version(cls)}"
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


# ---------------------------------------------------------------------------
# Recipe class loading / version resolution
# ---------------------------------------------------------------------------
def try_load_recipe_class(recipes_root: Path, name: str) -> type[RecipeBase] | None:
    """Load the ``Recipe`` class from ``recipes/<name>/recipe.py``.

    Returns ``None`` if the recipe does not exist or fails to import/validate.
    """
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
        return cls if (cls and isinstance(cls, type) and issubclass(cls, RecipeBase)) else None
    except Exception:
        return None


def resolve_version(recipe_cls: type[RecipeBase]) -> str:
    v = getattr(recipe_cls, "version", None)
    return str(v) if v else "latest"


# ---------------------------------------------------------------------------
# Requires discovery
# ---------------------------------------------------------------------------
def make_probe_recipe(
    recipe_cls: type[RecipeBase],
    recipes_root: Path,
    name: str,
    version: str,
    build_type: str,
    jobs: int | None = None,
) -> RecipeBase:
    """Instantiate a recipe with just enough state (settings, conf, requires shim) to
    drive ``config_options()``/``configure()``/``requirements()``/``build_requirements()``.

    No build folders are created — this is for dependency discovery only.  ``build.py``
    layers folder setup on top of this for actual builds.
    """
    recipe = recipe_cls(display_name=name)
    recipe.version = version
    recipe.recipe_folder = str(recipes_root / name)

    recipe.settings = detect_settings(build_type)
    recipe.settings_build = recipe.settings
    recipe.settings_target = None
    conf = make_conf(jobs=jobs)
    recipe.conf = conf
    recipe._recipe_runtime = RecipeRuntime(conf)
    recipe._recipe_dependencies = RecipeDependencies(OrderedDict())
    recipe._recipe_buildenv = Environment()
    recipe._recipe_runenv = Environment()
    recipe.requires = VersionResolvingRequirements(recipe.requires, recipes_root)
    return recipe


def discover_requires(recipe: RecipeBase) -> tuple[list[str], list[str]]:
    """Call the requirement-declaring methods and return ``(host_dep_names, tool_dep_names)``.

    host_dep_names — regular library dependencies (build=False)
    tool_dep_names — tool_requires / build_requires (build=True)
    """
    # Wire up the callable wrappers that graph resolution normally sets before calling these
    # methods.  Without them, recipe.tool_requires("foo") raises TypeError (calls None).
    from thirdparty._internal.model.requires import (
        BuildRequirements, TestRequirements, ToolRequirements,
    )
    recipe.build_requires = BuildRequirements(recipe.requires)
    recipe.test_requires = TestRequirements(recipe.requires)
    recipe.tool_requires = ToolRequirements(recipe.requires)

    # config_options() / configure() must run before requirements() so that option guards
    # like "if self.options.with_elf" reflect the correct value.
    for method in ("config_options", "configure", "requirements", "build_requirements"):
        if hasattr(recipe, method):
            try:
                getattr(recipe, method)()
            except Exception:
                pass
    host_names: list[str] = []
    tool_names: list[str] = []
    for req in recipe.requires.values():
        dep_name = str(req.ref.name)
        if req.build:
            tool_names.append(dep_name)
        else:
            host_names.append(dep_name)
    return host_names, tool_names


# ---------------------------------------------------------------------------
# Graph model
# ---------------------------------------------------------------------------
@dataclass
class RecipeGraphNode:
    name: str
    version: str
    recipe_cls: type[RecipeBase] | None
    host_deps: list[str] = field(default_factory=list)
    tool_deps: list[str] = field(default_factory=list)

    @property
    def all_deps(self) -> list[str]:
        return self.host_deps + self.tool_deps


class RecipeGraph:
    """A resolved dependency graph over a set of local recipes.

    Holds each recipe's direct host and tool dependencies and provides a stable
    topological ordering.  Reusable by ``build``, a future ``list``/``graph`` command,
    and any other consumer that needs dependency-aware ordering.
    """

    def __init__(self, nodes: dict[str, RecipeGraphNode]) -> None:
        self.nodes = nodes

    def __contains__(self, name: str) -> bool:
        return name in self.nodes

    def __getitem__(self, name: str) -> RecipeGraphNode:
        return self.nodes[name]

    def names(self) -> list[str]:
        return list(self.nodes.keys())

    def topo_order(self) -> list[str]:
        """Return node names in dependency order (deps before dependants).

        Only edges between nodes present in this graph are considered; external
        dependencies are ignored for ordering.  Ties are broken alphabetically for
        deterministic output.
        """
        graph: dict[str, list[str]] = {
            name: [d for d in node.all_deps if d in self.nodes]
            for name, node in self.nodes.items()
        }
        return _topo_sort(graph)


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


def build_recipe_graph(
    recipes_root: Path,
    names: list[str],
    build_type: str,
    jobs: int | None = None,
) -> RecipeGraph:
    """Resolve the direct dependencies of each recipe in ``names`` and return a graph.

    Recipes that fail to load or probe are still included as nodes with no dependencies,
    so callers can report them rather than silently dropping them.
    """
    nodes: dict[str, RecipeGraphNode] = {}
    for name in names:
        cls = try_load_recipe_class(recipes_root, name)
        if cls is None:
            nodes[name] = RecipeGraphNode(name=name, version="?", recipe_cls=None)
            continue
        version = resolve_version(cls)
        try:
            probe = make_probe_recipe(cls, recipes_root, name, version, build_type, jobs=jobs)
            host_deps, tool_deps = discover_requires(probe)
        except Exception:
            host_deps, tool_deps = [], []
        nodes[name] = RecipeGraphNode(
            name=name, version=version, recipe_cls=cls,
            host_deps=host_deps, tool_deps=tool_deps,
        )
    return RecipeGraph(nodes)
