from __future__ import annotations

import fnmatch
from functools import total_ordering
from typing import Any

from thirdparty._internal.model.version import Version
from thirdparty.errors import RecipeException


@total_ordering
class RecipeReference:
    """Recipe reference: ``name/version``.

    This system has no user/channel/revision/timestamp coordinates — a recipe is
    identified solely by its name, and each name maps to exactly one version.
    """

    def __init__(self, name: str | None = None, version: Any = None):
        self.name: str | None = name
        if version is not None and not isinstance(version, Version):
            version = Version(version)
        self.version: Version | None = version

    def copy(self) -> RecipeReference:
        return RecipeReference(self.name, self.version)

    def __repr__(self) -> str:
        return str(self)

    def repr_notime(self) -> str:
        return str(self)

    def repr_humantime(self) -> str:
        return str(self)

    def __str__(self) -> str:
        if self.name is None:
            return ""
        if self.version is None:
            return self.name
        return "/".join([self.name, str(self.version)])

    def __lt__(self, ref: RecipeReference) -> bool:
        # Identity is by NAME only: this system has exactly one recipe per name, so the
        # version is never part of dependency lookup/dedup/ordering (it is only carried for
        # layout paths, generators like config-version.cmake, and publishing/out-of-date).
        return self.name < ref.name

    def __eq__(self, ref: Any) -> bool:
        if ref is None:
            return False
        return self.name == ref.name

    def __hash__(self) -> int:
        return hash(self.name)

    @staticmethod
    def loads(rref: str) -> RecipeReference:
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
                f" in the form name or name/version")

    def matches(self, pattern: str, is_consumer: bool) -> bool:
        negate = False
        if pattern.startswith("!") or pattern.startswith("~"):
            pattern = pattern[1:]
            negate = True

        # ``@`` / ``@#`` suffixes meant "no user/channel"; always true here, just strip them.
        if pattern.endswith("@"):
            pattern = pattern[:-1]
        elif "@#" in pattern:
            pattern = pattern.replace("@#", "#")

        condition = ((pattern == "&" and is_consumer) or fnmatch.fnmatchcase(str(self), pattern))
        return not condition if negate else condition


def ref_matches(ref: Any, pattern: str, is_consumer: bool) -> bool:
    if not ref or not str(ref):
        assert is_consumer
        ref = RecipeReference.loads("*/*")  # FIXME: ugly
    return ref.matches(pattern, is_consumer=is_consumer)
