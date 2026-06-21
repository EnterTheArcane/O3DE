import fnmatch
import re
from functools import total_ordering

from thirdparty.errors import RecipeException
from thirdparty._internal.model.version import Version
from thirdparty._internal.util.dates import timestamp_to_str


@total_ordering
class RecipeReference:
    """Concrete recipe reference: ``name/version[@user/channel][#revision][%timestamp]``."""

    def __init__(self, name=None, version=None, user=None, channel=None, revision=None,
                 timestamp=None):
        self.name: str = name
        if version is not None and not isinstance(version, Version):
            version = Version(version)
        self.version: Version = version
        self.user = user
        self.channel = channel
        self.revision = revision
        self.timestamp = timestamp

    def copy(self):
        return RecipeReference(self.name, self.version, self.user, self.channel, self.revision,
                               self.timestamp)

    def __repr__(self):
        result = self.repr_notime()
        if self.timestamp is not None:
            result += f"%{self.timestamp}"
        return result

    def repr_notime(self):
        result = str(self)
        if self.revision is not None:
            result += f"#{self.revision}"
        return result

    def repr_humantime(self):
        result = self.repr_notime()
        assert self.timestamp
        result += f" ({timestamp_to_str(self.timestamp)})"
        return result

    def __str__(self):
        if self.name is None:
            return ""
        result = "/".join([self.name, str(self.version)])
        if self.user:
            result += f"@{self.user}"
        if self.channel:
            assert self.user
            result += f"/{self.channel}"
        return result

    def __lt__(self, ref):
        return (self.name, self.version, self.user or "", self.channel or "", self.timestamp or 0,
                self.revision or "") < \
               (ref.name, ref.version, ref.user or "", ref.channel or "", ref.timestamp or 0,
                ref.revision or "")

    def __eq__(self, ref):
        if ref is None:
            return False
        if self.revision is not None and ref.revision is not None:
            return (self.name, self.version, self.user, self.channel, self.revision) == \
                   (ref.name, ref.version, ref.user, ref.channel, ref.revision)
        return (self.name, self.version, self.user, self.channel) == \
               (ref.name, ref.version, ref.user, ref.channel)

    def __hash__(self):
        return hash((self.name, self.version, self.user, self.channel))

    @staticmethod
    def loads(rref):
        try:
            tokens = rref.rsplit("%", 1)
            text = tokens[0]
            timestamp = float(tokens[1]) if len(tokens) == 2 else None

            tokens = text.split("#", 1)
            ref = tokens[0]
            revision = tokens[1] if len(tokens) == 2 else None

            tokens = ref.split("@", 1)
            name, version = tokens[0].split("/", 1)
            assert name and version
            if len(tokens) == 2 and tokens[1]:
                tokens = tokens[1].split("/", 1)
                user = tokens[0] if tokens[0] else None
                channel = tokens[1] if len(tokens) == 2 else None
            else:
                user = channel = None
            return RecipeReference(name, version, user, channel, revision, timestamp)
        except Exception:
            raise RecipeException(
                f"{rref} is not a valid recipe reference, provide a reference"
                f" in the form name/version[@user/channel]"
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
        if self.user and validation_pattern.match(self.user) is None:
            raise RecipeException(f"Invalid package user '{self.user}'")
        if self.channel and validation_pattern.match(self.channel) is None:
            raise RecipeException(f"Invalid package channel '{self.channel}'")

        pattern = re.compile(r"[.+]")
        if pattern.search(self.name):
            Output().warning(f"Name containing special chars is discouraged '{self.name}'")
        if self.user and pattern.search(self.user):
            Output().warning(f"User containing special chars is discouraged '{self.user}'")
        if self.channel and pattern.search(self.channel):
            Output().warning(
                f"Channel containing special chars is discouraged '{self.channel}'"
            )

    def matches(self, pattern, is_consumer):
        negate = False
        if pattern.startswith("!") or pattern.startswith("~"):
            pattern = pattern[1:]
            negate = True

        no_user_channel = False
        if pattern.endswith("@"):
            pattern = pattern[:-1]
            no_user_channel = True
        elif "@#" in pattern:
            pattern = pattern.replace("@#", "#")
            no_user_channel = True

        condition = ((pattern == "&" and is_consumer) or
                     fnmatch.fnmatchcase(str(self), pattern) or
                     fnmatch.fnmatchcase(self.repr_notime(), pattern))
        if no_user_channel:
            condition = condition and not self.user and not self.channel
        return not condition if negate else condition

    def partial_match(self, pattern):
        tokens = [self.name, "/", str(self.version)]
        if self.user:
            tokens += ["@", self.user]
        if self.channel:
            tokens += ["/", self.channel]
        if self.revision:
            tokens += ["#", self.revision]
        partial = ""
        for token in tokens:
            partial += token
            if pattern.match(partial):
                return True


class PkgReference:
    def __init__(self, ref=None, package_id=None, revision=None, timestamp=None):
        self.ref = ref
        self.package_id = package_id
        self.revision = revision
        self.timestamp = timestamp

    def __repr__(self):
        if self.ref is None:
            return ""
        result = repr(self.ref)
        if self.package_id:
            result += f":{self.package_id}"
        if self.revision is not None:
            result += f"#{self.revision}"
        if self.timestamp is not None:
            result += f"%{self.timestamp}"
        return result

    def repr_notime(self):
        if self.ref is None:
            return ""
        result = self.ref.repr_notime()
        if self.package_id:
            result += f":{self.package_id}"
        if self.revision is not None:
            result += f"#{self.revision}"
        return result

    def repr_humantime(self):
        result = self.repr_notime()
        assert self.timestamp
        result += f" ({timestamp_to_str(self.timestamp)})"
        return result

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
        return self.ref == other.ref and self.package_id == other.package_id and \
               self.revision == other.revision

    def __hash__(self):
        return hash((self.ref, self.package_id, self.revision))

    @staticmethod
    def loads(pkg_ref):
        try:
            tokens = pkg_ref.split(":", 1)
            assert len(tokens) == 2
            ref, pkg_id = tokens

            ref = RecipeReference.loads(ref)

            tokens = pkg_id.rsplit("%", 1)
            text = tokens[0]
            timestamp = float(tokens[1]) if len(tokens) == 2 else None

            tokens = text.split("#", 1)
            package_id = tokens[0]
            revision = tokens[1] if len(tokens) == 2 else None

            return PkgReference(ref, package_id, revision, timestamp)
        except Exception:
            raise RecipeException(
                f"{pkg_ref} is not a valid package reference, provide a reference"
                f" in the form name/version[@user/channel:package_id]"
            )
