from __future__ import annotations

import os
import re
from functools import cached_property

from github import Auth, Github
from github.GithubException import GithubException
from github.Repository import Repository

from thirdparty._internal.model.version import Version

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase


def _tag_version(tag: str) -> Version | None:
    # Strip a leading identifier+separator prefix: "vulkan-sdk-", "nasm-", "m4-",
    # "bzip2-", "VER-", etc.  The pattern allows digits inside the word (like m4).
    stripped = re.sub(r'^(?:[A-Za-z][A-Za-z0-9]*[-_])+', '', tag)
    if not stripped or not stripped[0].isdigit():
        # Fall back to a bare single-letter prefix: v1.2, V3.4, n8.1
        if len(tag) >= 2 and tag[0].isalpha() and tag[1].isdigit():
            stripped = tag[1:]
        else:
            return None
    had_prefix = (stripped != tag)
    candidate = stripped.replace("_", ".").replace("-", ".")
    if not candidate or not candidate[0].isdigit():
        return None
    if not re.match(r'^\d+(?:\.\d+)*$', candidate):
        return None
    try:
        v = Version(candidate)
    except Exception:
        return None
    if not v.main or not all(isinstance(i.value, int) for i in v.main):
        return None
    # Require at least two components; single numbers (r42, v114, draft-15) are not versions.
    if len(v.main) < 2:
        return None
    # When a non-digit prefix was stripped, reject year-like first components
    # (e.g. "before-reformat-2005-01" -> 2005.01, "CVE-2021-3541" -> 2021.3541).
    if had_prefix and v.main[0].value >= 1000:
        return None
    # Reject digit-starting YYYY-MM-DD style date tags (e.g. "2021-01-15").
    if (not had_prefix and len(v.main) == 3 and v.main[0].value > 1970 and 1 <= v.main[1].value <= 12 and 1 <= v.main[2].value <= 31):
        return None
    return v


class GithubRepository:
    """Typed wrapper around a single GitHub repository, used by recipes.

    Usage:
        repo = GithubRepository(self, "abseil/abseil-cpp")
        repo.latest_release   # raw tag_name string, e.g. "20240722.0" or "v1.2.3"

    Authentication uses GH_TOKEN or GITHUB_TOKEN if set; otherwise anonymous
    (subject to GitHub's 60 req/hour unauthenticated rate limit).
    """

    def __init__(self, recipe: RecipeBase, slug: str) -> None:
        self._recipe = recipe
        self._slug = slug

    @cached_property
    def _client(self) -> Github:
        token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
        auth = Auth.Token(token) if token else None
        return Github(auth=auth, user_agent="thirdparty-outdated")

    @cached_property
    def _repo(self) -> Repository:
        return self._client.get_repo(self._slug)

    @cached_property
    def latest_release(self) -> str:
        """Raw tag_name of the latest GitHub release.

        Tries get_latest_release() first, but validates the result against
        _highest_tag(): some repos have maintenance-branch security patches
        marked as the GitHub "Latest Release" even though newer major versions
        exist (e.g. CPython 3.9.x published after 3.13.x), or the latest
        release uses an unexpected tag scheme.  When _highest_tag() returns a
        strictly higher version, that tag is preferred.
        """
        try:
            release_tag = self._repo.get_latest_release().tag_name
        except GithubException as exc:
            if exc.status != 404:
                raise
            return self._highest_tag()
        try:
            ht = self._highest_tag()
            r_v = _tag_version(release_tag)
            h_v = _tag_version(ht)
            if h_v is not None and (r_v is None or h_v >= r_v):
                return ht
        except Exception:
            pass
        return release_tag

    @cached_property
    def latest_formal_release(self) -> str:
        """Raw tag_name of the GitHub latest release; no fallback to tag scanning."""
        try:
            return self._repo.get_latest_release().tag_name
        except GithubException as exc:
            if exc.status != 404:
                raise
            return self._highest_tag()

    def latest_tag(self, prefix: str) -> str:
        """Return the raw tag name with the highest version among tags starting with `prefix`.

        The suffix after `prefix` is normalised (hyphens → dots) before version comparison.
        """
        best_tag: str | None = None
        best_version: Version | None = None
        for tag in self._repo.get_tags():
            if not tag.name.startswith(prefix):
                continue
            try:
                v = Version(tag.name[len(prefix):].replace("-", ".").replace("_", "."))
            except Exception:
                continue
            if not v.main or not all(isinstance(item.value, int) for item in v.main):
                continue
            if best_version is None or v > best_version:
                best_version = v
                best_tag = tag.name
        if best_tag is None:
            raise RuntimeError(f"no tag with prefix {prefix!r} found in {self._slug}")
        return best_tag

    def latest_release_matching(self, pattern: str) -> str:
        """Return the tag of the most recently published non-pre-release release
        whose tag_name matches the given regex pattern."""
        rx = re.compile(pattern)
        for release in self._repo.get_releases():
            if not release.prerelease and rx.match(release.tag_name):
                return release.tag_name
        raise RuntimeError(f"no release matching {pattern!r} found in {self._slug}")

    def _highest_tag(self) -> str:
        """Walk up to 500 tags (newest first) and return the one with the
        highest all-numeric version, tolerating any leading non-digit prefix
        and underscore/hyphen separators in the tag name."""
        best_tag: str | None = None
        best_version: Version | None = None
        for i, tag in enumerate(self._repo.get_tags()):
            if i >= 500:
                break
            v = _tag_version(tag.name)
            if v is None:
                continue
            if best_version is None or v > best_version:
                best_version = v
                best_tag = tag.name
        if best_tag is None:
            raise RuntimeError(f"no version-like tags found for {self._slug}")
        return best_tag
