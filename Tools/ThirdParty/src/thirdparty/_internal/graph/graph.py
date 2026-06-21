from collections import OrderedDict

from thirdparty._internal.model.refs import PkgReference
from thirdparty._internal.model.refs import RecipeReference

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
    """A node in the dependency graph: a recipe's identity.

    This system resolves dependencies by name (see ``recipe_graph``/``build.py``) rather than
    via Conan's transitive graph algorithm, so the heavy conflict-resolution and propagation
    machinery has been removed.  ``Node`` is a lean holder of the per-recipe identity that
    recipe methods read back through ``self._recipe_node``: the ``ref``, the build ``context``
    and the recipe-origin ``recipe`` state (e.g. ``RECIPE_CONSUMER``/``RECIPE_INCACHE``).
    """

    def __init__(self, ref, recipe_instance, context, recipe_state=None):
        self.ref = ref
        self.context = context
        # Recipe-origin state (RECIPE_CONSUMER / RECIPE_INCACHE / RECIPE_PLATFORM / ...).
        self.recipe = recipe_state
        self._package_id = None
        if recipe_instance is not None:
            recipe_instance._recipe_node = self  # back-reference, so recipes can read ref/context

    @property
    def package_id(self):
        return self._package_id

    @package_id.setter
    def package_id(self, pkg_id):
        assert self._package_id is None, "Trying to override an existing package_id"
        self._package_id = pkg_id

    @property
    def name(self):
        return self.ref.name if self.ref else None

    @property
    def pref(self):
        assert self.ref is not None and self.package_id is not None, "Node %s" % self.recipe
        return PkgReference(self.ref, self.package_id)


class Edge:
    def __init__(self, src, dst, require):
        self.src = src
        self.dst = dst
        self.require = require


class Overrides:
    def __init__(self):
        self._overrides = {}  # {require_ref: {override_ref1, override_ref2}}

    def __bool__(self):
        return bool(self._overrides)

    def __repr__(self):
        return repr(self.serialize())

    @staticmethod
    def create(nodes):
        overrides = {}
        for n in nodes:
            for r in n.recipe.requires.values():
                if r.override and not r.overriden_ref:  # overrides are not real graph edges
                    continue
                if r.overriden_ref:
                    overrides.setdefault(r.overriden_ref, set()).add(r.override_ref)
                else:
                    overrides.setdefault(r.ref, set()).add(None)

        # reduce, eliminate those overrides definitions that only override to None, that is, not
        # really an override
        result = Overrides()
        for require, override_info in overrides.items():
            if len(override_info) != 1 or None not in override_info:
                result._overrides[require] = override_info
        return result

    def get(self, require):
        return self._overrides.get(require)

    def update(self, other):
        """
        :type other: Overrides
        """
        for require, override_info in other._overrides.items():
            self._overrides.setdefault(require, set()).update(override_info)

    def items(self):
        return self._overrides.items()

    def serialize(self):
        return {k.repr_notime(): sorted([e.repr_notime() if e else None for e in v],
                                        key=lambda e: "" if e is None else e)
                for k, v in self._overrides.items()}

    @staticmethod
    def deserialize(data):
        result = Overrides()
        result._overrides = {RecipeReference.loads(k):
                             set([RecipeReference.loads(e) if e else None for e in v])
                             for k, v in data.items()}
        return result


class DepsGraph:
    def __init__(self):
        self.nodes = []
        self.aliased = {}
        self.resolved_ranges = {}
        self.replaced_requires = {}
        self.options_conflicts = {}
        self.visibility_conflicts = {}
        self.error = False

    def lockfile(self):
        from thirdparty._internal.model.lockfile import Lockfile
        return Lockfile(self)

    def overrides(self):
        return Overrides.create(self.nodes)

    def __repr__(self):
        return "\n".join((repr(n) for n in self.nodes))

    @property
    def root(self):
        return self.nodes[0] if self.nodes else None

    def add_node(self, node):
        self.nodes.append(node)

    @staticmethod
    def add_edge(src, dst, require):
        edge = Edge(src, dst, require)
        src.add_edge(edge)
        dst.add_edge(edge)

    def ordered_iterate(self):
        ordered = self.by_levels()
        for level in ordered:
            for node in level:
                yield node

    def by_levels(self):
        """ order by node degree. The first level will be the one which nodes dont have
        dependencies. Second level will be with nodes that only have dependencies to
        first level nodes, and so on
        return [[node1, node34], [node3], [node23, node8],...]
        """
        result = []
        # We make it a dict to preserve insertion order and be deterministic, s
        # sets are not deterministic order. dict is fast for look up operations
        opened = dict.fromkeys(self.nodes)
        while opened:
            current_level = []
            for o in opened:
                o_neighs = o.neighbors()
                if not any(n in opened for n in o_neighs):
                    current_level.append(o)

            # TODO: SORTING seems only necessary for test order
            current_level.sort()
            result.append(current_level)
            # now start new level, removing the current level items
            for item in current_level:
                opened.pop(item)

        return result

    def report_graph_error(self):
        if self.error:
            raise self.error

    def serialize(self):
        for i, n in enumerate(self.nodes):
            n.id = str(i)
        result = OrderedDict()
        result["nodes"] = {n.id: n.serialize() for n in self.nodes}
        result["root"] = {self.root.id: repr(self.root.ref)}  # TODO: ref of consumer/virtual
        result["overrides"] = self.overrides().serialize()
        result["resolved_ranges"] = {repr(r): s.repr_notime()
                                     for r, s in self.resolved_ranges.items()}
        result["replaced_requires"] = {k: v for k, v in self.replaced_requires.items()}
        result["error"] = self.error.serialize() if isinstance(self.error, GraphError) else None
        return result
