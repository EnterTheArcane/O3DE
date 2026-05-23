
import re

from thirdparty._conan.errors import ConanException


class RecipeReference:
    """A reference identifying a recipe by name only.
    Version is stored as a plain attribute for use by generators, but is never parsed
    from a reference string — it is always set from the recipe's own version attribute.
    """

    def __init__(self, name=None, version=None):
        self.name: str = name
        self.version = version  # plain string set externally by _FakeNode, not parsed here

    def __repr__(self):
        return self.name or ""

    def __str__(self):
        return self.name or ""

    def __eq__(self, ref):
        if ref is None:
            return False
        return self.name == ref.name

    def __hash__(self):
        return hash(self.name)

    def __lt__(self, ref):
        return (self.name or "") < (ref.name or "")

    @staticmethod
    def loads(rref):
        """Parse a package name from a reference string.
        Everything after the first '/', '@', '#', or '%' is ignored — versions, users,
        channels, and revisions are not used by this tool."""
        name = re.split(r"[/@#%]", rref.strip())[0]
        if not name:
            raise ConanException(
                f"'{rref}' is not a valid recipe reference — expected a package name")
        return RecipeReference(name)

    def validate_ref(self, allow_uppercase=False):
        from thirdparty._conan.api.output import ConanOutput
        if not self.name:
            raise ConanException("Empty package name")
        if not allow_uppercase and self.name != self.name.lower():
            raise ConanException(f"Conan package name '{self.name}' must be all lowercase")
        pattern = re.compile(r"^[a-zA-Z0-9_][a-zA-Z0-9_+.-]{1,100}\Z" if allow_uppercase
                             else r"^[a-z0-9_][a-z0-9_+.-]{1,100}\Z")
        if pattern.match(self.name) is None:
            raise ConanException(f"Invalid package name '{self.name}'")


class PkgReference:

    def __init__(self, ref=None, package_id=None, revision=None, timestamp=None):
        self.ref = ref
        self.package_id = package_id
        self.revision = revision
        self.timestamp = timestamp  # float, Unix seconds UTC

    def __repr__(self):
        """ long repr like pkg/0.1@user/channel#rrev%timestamp """
        if self.ref is None:
            return ""
        result = repr(self.ref)
        if self.package_id:
            result += ":{}".format(self.package_id)
        if self.revision is not None:
            result += "#{}".format(self.revision)
        if self.timestamp is not None:
            result += "%{}".format(self.timestamp)
        return result

    def repr_notime(self):
        if self.ref is None:
            return ""
        result = self.ref.repr_notime()
        if self.package_id:
            result += ":{}".format(self.package_id)
        if self.revision is not None:
            result += "#{}".format(self.revision)
        return result

    def repr_humantime(self):
        result = self.repr_notime()
        assert self.timestamp
        result += " ({})".format(timestamp_to_str(self.timestamp))
        return result

    def __str__(self):
        """ shorter representation, excluding the revision and timestamp """
        if self.ref is None:
            return ""
        result = str(self.ref)
        if self.package_id:
            result += ":{}".format(self.package_id)
        return result

    def __lt__(self, ref):
        # The timestamp goes before the revision for ordering revisions chronologically
        raise Exception("WHO IS COMPARING PACKAGE REFERENCES?")
        # return (self.name, self.version, self.user, self.channel, self.timestamp, self.revision) \
        #       < (ref.name, ref.version, ref.user, ref.channel, ref._timestamp, ref.revision)

    def __eq__(self, other):
        # TODO: In case of equality, should it use the revision and timestamp?
        # Used:
        #    at "graph_binaries" to check: cache_latest_prev != pref
        #    at "installer" to check: if pkg_layout.reference != pref (probably just optimization?)
        #    at "revisions_test"
        return self.ref == other.ref and self.package_id == other.package_id and \
               self.revision == other.revision

    def __hash__(self):
        # Used in dicts of PkgReferences as keys like the cached nodes in the graph binaries
        return hash((self.ref, self.package_id, self.revision))

    @staticmethod
    def loads(pkg_ref):  # TODO: change this default to validate only on end points
        try:
            tokens = pkg_ref.split(":", 1)
            assert len(tokens) == 2
            ref, pkg_id = tokens

            ref = RecipeReference.loads(ref)

            # timestamp
            tokens = pkg_id.rsplit("%", 1)
            text = tokens[0]
            timestamp = float(tokens[1]) if len(tokens) == 2 else None

            # revision
            tokens = text.split("#", 1)
            package_id = tokens[0]
            revision = tokens[1] if len(tokens) == 2 else None

            return PkgReference(ref, package_id, revision, timestamp)
        except Exception:
            raise ConanException(
                f"{pkg_ref} is not a valid package reference, provide a reference"
                f" in the form name/version[@user/channel:package_id]")
