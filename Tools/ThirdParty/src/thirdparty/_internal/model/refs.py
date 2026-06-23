import fnmatch
from functools import total_ordering

from thirdparty._internal.model.version import Version
from thirdparty.errors import RecipeException


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
        if self.version is None:
            return self.name
        return "/".join([self.name, str(self.version)])

    def __lt__(self, ref):
        # Identity is by NAME only: this system has exactly one recipe per name, so the
        # version is never part of dependency lookup/dedup/ordering (it is only carried for
        # layout paths, generators like config-version.cmake, and publishing/out-of-date).
        return self.name < ref.name

    def __eq__(self, ref):
        if ref is None:
            return False
        return self.name == ref.name

    def __hash__(self):
        return hash(self.name)

    @staticmethod
    def loads(rref):
        try:
            # Tolerate (and discard) any legacy @user/channel, #revision or %timestamp suffix.
            text = rref.split("%", 1)[0].split("#", 1)[0].split("@", 1)[0]
            # A dep is identified by NAME; the version lives in its own recipe.  Accept a bare
            # name (``abseil``) as well as the explicit ``name/version`` form.
            if "/" in text:
                name, version = text.split("/", 1)
                assert name and version
                return RecipeReference(name, version)
            assert text
            return RecipeReference(text, None)
        except Exception:
            raise RecipeException(
                f"{rref} is not a valid recipe reference, provide a reference"
                f" in the form name or name/version"
            )

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


def ref_matches(ref, pattern, is_consumer):
    if not ref or not str(ref):
        assert is_consumer
        ref = RecipeReference.loads("*/*")  # FIXME: ugly
    return ref.matches(pattern, is_consumer=is_consumer)
