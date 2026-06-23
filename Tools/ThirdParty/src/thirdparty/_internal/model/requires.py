from thirdparty._internal.model.refs import RecipeReference
from thirdparty.errors import RecipeException


class Requirement:
    """ A user definition of a requires in a recipe
    """

    def __init__(
        self, ref, *, headers=None, libs=None, build=False, run=None, visible=None,
        transitive_headers=None, transitive_libs=None, package_id_mode=None,
        force=None, override=None, direct=None, options=None, no_skip=False,
        consistent=None):
        # * prevents the usage of more positional parameters, always ref + **kwargs
        # By default this is a generic library requirement
        self.ref = ref
        self._required_ref = ref  # Store the original reference
        self._headers = headers  # This dependent node has headers that must be -I<headers-path>
        self._libs = libs
        self._build = build  # This dependent node is a build tool that runs at build time only
        self._run = run  # node contains executables, shared libs or data necessary at host run time
        self._visible = visible  # Even if not libsed or visible, the node is unique, can conflict
        self._transitive_headers = transitive_headers
        self._transitive_libs = transitive_libs
        self._package_id_mode = package_id_mode
        self._force = force
        self._override = override
        self._direct = direct
        self._consistent = consistent
        self.options = options
        # Meta and auxiliary information
        # The "defining_require" is the require that defines the current value. If this require is
        # overriden/forced, this attribute will point to the overriding/forcing requirement.
        self.defining_require = self  # if not overriden, it points to itself
        self.overriden_ref = None  # to store if the requirement has been overriden (store old ref)
        self.override_ref = None  # to store if the requirement has been overriden (store new ref)
        self.skip = False
        self.required_nodes = set()  # store which intermediate nodes are required, to compute "Skip"
        self.no_skip = no_skip
        # computed ones, not default ones
        self.consistent_policy_new = False
        if self.visible and not self.consistent:
            raise RecipeException(
                f"Requirement {ref} with visible=True and consistent=False is not"
                f" supported. Please open a Github ticket to report it")

    @property
    def files(self):  # require needs some files in dependency package
        return self.headers or self.libs or self.run or self.build

    @staticmethod
    def _default_if_none(field, default_value):
        return field if field is not None else default_value

    @property
    def headers(self):
        return self._default_if_none(self._headers, True)

    @headers.setter
    def headers(self, value):
        self._headers = value

    @property
    def libs(self):
        return self._default_if_none(self._libs, True)

    @libs.setter
    def libs(self, value):
        self._libs = value

    @property
    def visible(self):
        return self._default_if_none(self._visible, True)

    @visible.setter
    def visible(self, value):
        self._visible = value

    @property
    def force(self):
        return self._default_if_none(self._force, False)

    @force.setter
    def force(self, value):
        self._force = value

    @property
    def override(self):
        return self._default_if_none(self._override, False)

    @override.setter
    def override(self, value):
        self._override = value

    @property
    def direct(self):
        return self._default_if_none(self._direct, True)

    @direct.setter
    def direct(self, value):
        self._direct = value

    @property
    def consistent(self):
        # Host by default has to be consistent too
        if self.consistent_policy_new:
            default_consistent = self.visible or not self.build
        else:
            default_consistent = self.visible
        return self._default_if_none(self._consistent, default_consistent)

    @consistent.setter
    def consistent(self, value):
        self._consistent = value

    @property
    def build(self):
        return self._build

    @build.setter
    def build(self, value):
        self._build = value

    @property
    def run(self):
        return self._default_if_none(self._run, False)

    @run.setter
    def run(self, value):
        self._run = value

    @property
    def transitive_headers(self):
        return self._transitive_headers

    @transitive_headers.setter
    def transitive_headers(self, value):
        self._transitive_headers = value

    @property
    def transitive_libs(self):
        return self._transitive_libs

    @transitive_libs.setter
    def transitive_libs(self, value):
        self._transitive_libs = value

    @property
    def package_id_mode(self):
        return self._package_id_mode

    @package_id_mode.setter
    def package_id_mode(self, value):
        self._package_id_mode = value

    def __repr__(self):
        return repr(self.__dict__)

    def __str__(self):
        traits = (f'build={self.build}, headers={self.headers}, libs={self.libs}, '
                  f'run={self.run}, visible={self.visible}')
        return f"{self.ref}, Traits: {traits}"

    def serialize(self):
        result = {
            "ref": str(self.ref),
            "require": str(self._required_ref),
        }
        serializable = (
            "run", "libs", "skip", "force", "direct", "build",
            "transitive_headers", "transitive_libs", "headers",
            "package_id_mode", "visible",
        )
        for attribute in serializable:
            result[attribute] = getattr(self, attribute)
        return result

    def copy_requirement(self):
        return Requirement(
            self.ref, headers=self.headers, libs=self.libs, build=self.build,
            run=self.run, visible=self.visible,
            transitive_headers=self.transitive_headers,
            transitive_libs=self.transitive_libs,
            consistent=self.consistent)

    @property
    def alias(self):
        version = repr(self.ref.version)
        if version.startswith("(") and version.endswith(")"):
            return RecipeReference(self.ref.name, version[1:-1])

    def __hash__(self):
        return hash((self.ref.name, self.build))

    def __eq__(self, other):
        """If the name is the same and they are in the same context, and if both of them are
        propagating includes or libs or run info or both are visible or the reference is the same,
        we consider the requires equal, so they can conflict"""
        return (self.ref.name == other.ref.name and self.build == other.build and
                (self.override or  # an override with same name and context, always match
                 (self.headers and other.headers) or
                 (self.libs and other.libs) or
                 (self.run and other.run) or
                 (self.consistent and other.consistent) or
                 (self.ref == other.ref and self.options == other.options)))

    def aggregate(self, other):
        """ when closing loop and finding the same dependency on a node, the information needs
        to be aggregated
        :param other: is the existing Require that the current node has, which information has to be
        appended to "self", which is the requires that is being propagated to the current node
        from upstream
        """
        assert self.build == other.build
        if other.override:
            # If the other aggregated is an override, it shouldn't add information
            # it already did override upstream, and the actual information used in this node is
            # the propagated one.
            self.force = True
            return
        self.headers |= other.headers
        self.libs |= other.libs
        self.run = self.run or other.run
        self.visible |= other.visible
        self.consistent |= other.consistent
        self.force |= other.force
        self.direct |= other.direct
        self.transitive_headers = self.transitive_headers or other.transitive_headers
        self.transitive_libs = self.transitive_libs or other.transitive_libs
        # package_id_mode is not being propagated downstream. So it is enough to check if the
        # current require already defined it or not
        if self.package_id_mode is None:
            self.package_id_mode = other.package_id_mode
        self.required_nodes.update(other.required_nodes)


class ToolRequirements:
    def __init__(self, requires):
        self._requires = requires

    def __call__(
        self, ref, package_id_mode=None, visible=False, run=True, options=None,
        override=None):
        # TODO: Check which arguments could be user-defined
        self._requires.tool_require(
            ref, package_id_mode=package_id_mode, visible=visible, run=run,
            options=options, override=override)


class Requirements:
    """ User definitions of all requires in a recipe
    """

    def __init__(self, declared=None, declared_tool=None):
        self._requires = {}
        # Construct from the class definitions
        if declared is not None:
            if isinstance(declared, str):
                self.__call__(declared)
            else:
                try:
                    for item in declared:
                        if not isinstance(item, str):
                            # TODO (2.X): Remove protection after transition from 1.X
                            raise RecipeException(f"Incompatible 1.X requires declaration '{item}'")
                        self.__call__(item)
                except TypeError:
                    raise RecipeException(
                        "Wrong 'requires' definition, "
                        "did you mean 'requirements()'?")
        if declared_tool is not None:
            if isinstance(declared_tool, str):
                self.tool_require(declared_tool)
            else:
                try:
                    for item in declared_tool:
                        self.tool_require(item)
                except TypeError:
                    raise RecipeException(
                        "Wrong 'tool_requires' definition, "
                        "did you mean 'requirements()'?")

    def reindex(self, require, new_name):
        """ This operation is necessary when the reference name of a package is changed
        as a result of an "alternative" replacement of the package name, otherwise the dictionary
        gets broken by modified key
        """
        result = {}
        for k, v in self._requires.items():
            if k is require:
                k.ref.name = new_name
            result[k] = v
        self._requires = result

    def values(self):
        return self._requires.values()

    def __call__(self, str_ref, **kwargs):
        if str_ref is None:
            return
        assert isinstance(str_ref, str)
        ref = RecipeReference.loads(str_ref)
        req = Requirement(ref, **kwargs)
        if self._requires.get(req):
            raise RecipeException(f"Duplicated requirement: {ref}")
        self._requires[req] = req

    def tool_require(
        self, ref, raise_if_duplicated=True, package_id_mode=None, visible=False,
        run=True, options=None, override=None):
        """
         Represent a build tool like "cmake".

         visible = False => Only the direct consumer can see it, won't conflict
         build = True => They run in the build machine (e.g cmake)
         libs = False => We won't link with it, is a tool, no propagate the libs.
         headers = False => We won't include headers, is a tool, no propagate the includes.
        """
        if ref is None:
            return
        # FIXME: This raise_if_duplicated is ugly, possibly remove
        ref = RecipeReference.loads(ref)
        req = Requirement(
            ref, headers=False, libs=False, build=True, run=run, visible=visible,
            package_id_mode=package_id_mode, options=options, override=override)
        if raise_if_duplicated and self._requires.get(req):
            raise RecipeException(f"Duplicated requirement: {ref}")
        self._requires[req] = req

    def __repr__(self):
        return repr(self._requires.values())

    def serialize(self):
        return [v.serialize() for v in self._requires.values()]

    def __len__(self):
        return len(self._requires)
