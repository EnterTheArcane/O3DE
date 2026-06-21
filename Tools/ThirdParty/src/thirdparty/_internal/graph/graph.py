from __future__ import annotations

from typing import TYPE_CHECKING

from thirdparty._internal.model.refs import PkgReference, RecipeReference

if TYPE_CHECKING:
    from pathlib import Path  # noqa: F401  (annotation only)
    from thirdparty._internal.model.recipe_base import RecipeBase  # noqa: F401

RECIPE_DOWNLOADED = "Downloaded"
RECIPE_INCACHE = "Cache"  # The previously installed recipe in cache is being used
RECIPE_UPDATED = "Updated"
RECIPE_INCACHE_DATE_UPDATED = "Cache (Updated date)"
RECIPE_NEWER = "Newer"  # The local recipe is  modified and newer timestamp than server
RECIPE_UPDATEABLE = "Update available"  # The update of recipe is available (only in recipe info)
# These recipes do not have a full reference, not in the cache
RECIPE_EDITABLE = "Editable"
RECIPE_CONSUMER = "Consumer"  # A recipe from the user
RECIPE_VIRTUAL = "Cli"  # A virtual recipe (dynamic in memory recipe)
RECIPE_PLATFORM = "Platform"

BINARY_CACHE = "Cache"
BINARY_DOWNLOAD = "Download"
BINARY_UPDATE = "Update"
BINARY_BUILD = "Build"
BINARY_MISSING = "Missing"
BINARY_SKIP = "Skip"
BINARY_EDITABLE = "Editable"
BINARY_EDITABLE_BUILD = "EditableBuild"
BINARY_INVALID = "Invalid"
BINARY_PLATFORM = "Platform"

CONTEXT_HOST = "host"
CONTEXT_BUILD = "build"


class Node:
    """A node in the dependency graph: one recipe's identity and its direct dependencies.

    Serves two roles with a single type:
      * the recipe's identity that recipe methods read back via ``self._recipe_node``
        (``ref``/``context``/recipe-origin ``recipe`` state), and
      * a vertex in a resolved :class:`Graph` (recipe class + direct host/tool dep names)
        used for build ordering.
    Dependency-graph nodes are produced by :meth:`Graph.build`; identity nodes are created by
    ``build.py`` and assigned to ``recipe._recipe_node``.
    """

    def __init__(self, name: str, version: str, recipe_cls: "type[RecipeBase] | None" = None,
                 host_deps: "list[str] | None" = None, tool_deps: "list[str] | None" = None,
                 *, context: str = CONTEXT_HOST, recipe_state: "str | None" = None) -> None:
        self._name: str = name
        self._version: str = version
        self.recipe_cls: "type[RecipeBase] | None" = recipe_cls
        self.host_deps: list[str] = list(host_deps or [])
        self.tool_deps: list[str] = list(tool_deps or [])
        self.context: str = context
        # Recipe-origin state (RECIPE_CONSUMER / RECIPE_INCACHE / RECIPE_PLATFORM / ...).
        self.recipe: "str | None" = recipe_state
        self._package_id: "str | None" = None

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> str:
        return self._version

    @property
    def ref(self) -> RecipeReference:
        return RecipeReference(self._name, self._version)

    @property
    def all_deps(self) -> list[str]:
        return self.host_deps + self.tool_deps

    @property
    def package_id(self) -> "str | None":
        return self._package_id

    @package_id.setter
    def package_id(self, pkg_id: str) -> None:
        assert self._package_id is None, "Trying to override an existing package_id"
        self._package_id = pkg_id

    @property
    def pref(self) -> PkgReference:
        assert self._package_id is not None, "Node %s has no package_id" % self._name
        return PkgReference(self.ref, self.package_id)


COMPLETE_MARKER = ".complete"


def is_built(build_root: Path, name: str, version: str, platform_tag: str) -> bool:
    """True if ``name/version`` has a completed build for *platform_tag*.

    The build-phase marker lives at ``build/<name>/<version>/<platform_tag>/build/.complete``.
    """
    return (build_root / name / version / platform_tag / "build" / COMPLETE_MARKER).is_file()


def discover_requires(recipe: RecipeBase) -> tuple[list[str], list[str]]:
    """Drive the recipe's config phase and return ``(host_dep_names, tool_dep_names)``.

    host_dep_names — regular library dependencies (build=False)
    tool_dep_names — tool_requires / build_requires (build=True)

    The whole config phase (config_options/configure + default auto-fPIC handling +
    requirements/build_requirements) is delegated to ``run_configure_method``.  Errors are
    swallowed — dependency discovery is best-effort and must not abort graph resolution.
    """
    from thirdparty._internal.methods import run_configure_method

    try:
        run_configure_method(recipe)
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
    # Add tools implied by the recipe's imported build-system helpers (e.g. CMakeToolchain
    # -> "cmake"), skipping any already declared or the recipe's own name.
    own_name = getattr(recipe, "name", None)
    for tool in getattr(type(recipe), "_implicit_tool_requires", ()):
        if tool != own_name and tool not in tool_names:
            tool_names.append(tool)
    return host_names, tool_names


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


class Graph:
    """A resolved dependency graph over a set of local recipes.

    Holds each recipe's direct host and tool dependencies (as :class:`Node` objects) and
    provides a stable topological ordering.  Reusable by ``build``, ``graph``, ``list``,
    and any other consumer that needs dependency-aware ordering.
    """

    def __init__(self, nodes: dict[str, Node]) -> None:
        self.nodes = nodes

    def __contains__(self, name: str) -> bool:
        return name in self.nodes

    def __getitem__(self, name: str) -> Node:
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

    @staticmethod
    def build(
        recipes_root: Path,
        names: list[str],
        build_type: str,
        jobs: int | None = None,
        transitive: bool = False,
        target_os: str | None = None,
        target_arch: str | None = None,
    ) -> "Graph":
        """Resolve the dependencies of each recipe in ``names`` and return a graph.

        With ``transitive=False`` only the listed recipes become nodes.  With
        ``transitive=True`` the graph is expanded to the full transitive closure of the
        listed recipes (every reachable local dependency becomes a node too).

        ``target_os``/``target_arch`` select the platform used for requirement discovery
        (conditional ``requires`` may branch on ``settings.os``/``arch``).

        Recipes/deps that fail to load or probe are still included as nodes with no
        dependencies, so callers can report them rather than silently dropping them.
        """
        from thirdparty._internal.loader import (
            try_load_recipe_class, resolve_version, make_probe_recipe,
        )

        nodes: dict[str, Node] = {}
        queue: list[str] = list(names)
        seen: set[str] = set()
        while queue:
            name = queue.pop(0)
            if name in seen:
                continue
            seen.add(name)
            cls = try_load_recipe_class(recipes_root, name)
            if cls is None:
                nodes[name] = Node(name=name, version="?", recipe_cls=None)
                continue
            version = resolve_version(cls)
            try:
                probe = make_probe_recipe(cls, recipes_root, name, version, build_type, jobs=jobs,
                                          target_os=target_os, target_arch=target_arch)
                host_deps, tool_deps = discover_requires(probe)
            except Exception:
                host_deps, tool_deps = [], []
            nodes[name] = Node(name=name, version=version, recipe_cls=cls,
                               host_deps=host_deps, tool_deps=tool_deps)
            if transitive:
                for dep in host_deps + tool_deps:
                    if dep not in seen:
                        queue.append(dep)
        return Graph(nodes)

