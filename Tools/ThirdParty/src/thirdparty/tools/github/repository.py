from __future__ import annotations

import os
from functools import cached_property

from github import Auth, Github
from github.GithubException import GithubException
from github.Repository import Repository

from thirdparty._conan.internal.model.version import Version


class GithubRepository:
    """Typed wrapper around a single GitHub repository, used by recipes.

    Usage:
        repo = GithubRepository(self, "abseil/abseil-cpp")
        repo.latest_release   # raw tag_name string, e.g. "20240722.0" or "v1.2.3"

    Authentication uses GH_TOKEN or GITHUB_TOKEN if set; otherwise anonymous
    (subject to GitHub's 60 req/hour unauthenticated rate limit).
    """

    def __init__(self, conanfile, slug: str) -> None:
        self._conanfile = conanfile
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

        Falls back to the highest version-like tag when the repo has no
        Releases (GitHub returns 404 from /releases/latest).
        """
        try:
            return self._repo.get_latest_release().tag_name
        except GithubException as exc:
            if exc.status != 404:
                raise
            return self._highest_tag()

    def _highest_tag(self) -> str:
        """Walk tags and return the one with the highest all-numeric Version."""
        best_tag: str | None = None
        best_version: Version | None = None
        for tag in self._repo.get_tags():
            try:
                v = Version(tag.name)
            except Exception:
                continue
            if not v.main or not all(isinstance(item.value, int) for item in v.main):
                continue
            if best_version is None or v > best_version:
                best_version = v
                best_tag = tag.name
        if best_tag is None:
            raise RuntimeError(f"no version-like tags found for {self._slug}")
        return best_tag
