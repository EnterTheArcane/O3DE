import fnmatch
import re
from functools import total_ordering

from thirdparty.errors import RecipeException
from thirdparty._internal.model.version import Version


@total_ordering
class RecipeReference:
    """Recipe reference: ``name/version``.

    This system has no user/channel/revision/timestamp coordinates — a recipe is
    identified solely by its name, and each name maps to exactly one version.
    """

    def __init__(self, name=None, version=None):
        self.name: str = name
        if version is not None and not isinstance(version, Version):
            version = Version(version)
        self.version: Version = version

    def copy(self):
        return RecipeReference(self.name, self.version)

    def __repr__(self):
        return str(self)

    def repr_notime(self):
        return str(self)

    def repr_humantime(self):
        return str(self)

    def __str__(self):
        if self.name is None:
            return ""
        return "/".join([self.name, str(self.version)])

    def __lt__(self, ref):
        return (self.name, self.version) < (ref.name, ref.version)

    def __eq__(self, ref):
        if ref is None:
            return False
        return (self.name, self.version) == (ref.name, ref.version)

    def __hash__(self):
        return hash((self.name, self.version))

    @staticmethod
    def loads(rref):
        try:
            # Tolerate (and discard) any legacy @user/channel, #revision or %timestamp suffix.
            text = rref.split("%", 1)[0].split("#", 1)[0].split("@", 1)[0]
            name, version = text.split("/", 1)
            assert name and version
            return RecipeReference(name, version)
        except Exception:
            raise RecipeException(
                f"{rref} is not a valid recipe reference, provide a reference"
                f" in the form name/version"
            )

    def validate_ref(self, allow_uppercase=False):
        from thirdparty._internal.output import Output

        self_str = str(self)
        if self_str != self_str.lower():
            if not allow_uppercase:
                raise RecipeException(f"Recipe packages names '{self_str}' must be all lowercase")
            Output().warning(
                f"Package name '{self_str}' has uppercase, and has been "
                "allowed by temporary config. This will break in later 2.X"
            )
        if len(self_str) > 200:
            raise RecipeException(f"Package reference too long >200 {self_str}")
        if ":" in repr(self):
            raise RecipeException(f"Invalid recipe reference '{repr(self)}' is a package reference")
        if not allow_uppercase:
            validation_pattern = re.compile(r"^[a-z0-9_][a-z0-9_+.-]{1,100}\Z")
        else:
            validation_pattern = re.compile(r"^[a-zA-Z0-9_][a-zA-Z0-9_+.-]{1,100}\Z")
        if validation_pattern.match(self.name) is None:
            raise RecipeException(f"Invalid package name '{self.name}'")
        if validation_pattern.match(str(self.version)) is None:
            raise RecipeException(f"Invalid package version '{self.version}'")

        if re.compile(r"[.+]").search(self.name):
            Output().warning(f"Name containing special chars is discouraged '{self.name}'")

    def matches(self, pattern, is_consumer):
        negate = False
        if pattern.startswith("!") or pattern.startswith("~"):
            pattern = pattern[1:]
            negate = True

        # ``@`` / ``@#`` suffixes meant "no user/channel"; always true here, just strip them.
        if pattern.endswith("@"):
            pattern = pattern[:-1]
        elif "@#" in pattern:
            pattern = pattern.replace("@#", "#")

        condition = ((pattern == "&" and is_consumer) or
                     fnmatch.fnmatchcase(str(self), pattern))
        return not condition if negate else condition

    def partial_match(self, pattern):
        partial = ""
        for token in (self.name, "/", str(self.version)):
            partial += token
            if pattern.match(partial):
                return True


class PkgReference:
    def __init__(self, ref=None, package_id=None):
        self.ref = ref
        self.package_id = package_id

    def __repr__(self):
        return str(self)

    def repr_notime(self):
        return str(self)

    def repr_humantime(self):
        return str(self)

    def __str__(self):
        if self.ref is None:
            return ""
        result = str(self.ref)
        if self.package_id:
            result += f":{self.package_id}"
        return result

    def __lt__(self, ref):
        raise Exception("WHO IS COMPARING PACKAGE REFERENCES?")

    def __eq__(self, other):
        return self.ref == other.ref and self.package_id == other.package_id

    def __hash__(self):
        return hash((self.ref, self.package_id))

    @staticmethod
    def loads(pkg_ref):
        try:
            tokens = pkg_ref.split(":", 1)
            assert len(tokens) == 2
            ref, pkg_id = tokens
            ref = RecipeReference.loads(ref)
            # Tolerate (and discard) any legacy #revision or %timestamp suffix on the package id.
            package_id = pkg_id.split("%", 1)[0].split("#", 1)[0]
            return PkgReference(ref, package_id)
        except Exception:
            raise RecipeException(
                f"{pkg_ref} is not a valid package reference, provide a reference"
                f" in the form name/version:package_id"
            )
