from __future__ import annotations

import os
from functools import cached_property

import gitlab
import gitlab.exceptions

from thirdparty._conan.internal.model.version import Version


class GitlabRepository:
    """Typed wrapper around a single GitLab project, used by recipes.

    Usage:
        repo = GitlabRepository(self, "wayland/wayland", host="gitlab.freedesktop.org")
        repo.latest_release   # raw tag_name string, e.g. "1.24.0" or "v2.2.1"

    Authentication uses GL_TOKEN or GITLAB_TOKEN if set; otherwise anonymous.
    Unauthenticated access is subject to GitLab's rate limits and may be
    blocked on some instances without a token.
    """

    def __init__(self, conanfile, slug: str, host: str = "gitlab.com") -> None:
        self._conanfile = conanfile
        self._slug = slug
        self._host = host

    @cached_property
    def _client(self) -> gitlab.Gitlab:
        token = os.environ.get("GL_TOKEN") or os.environ.get("GITLAB_TOKEN")
        return gitlab.Gitlab(url=f"https://{self._host}", private_token=token)

    @cached_property
    def _project(self):
        return self._client.projects.get(self._slug)

    @cached_property
    def latest_release(self) -> str:
        """Raw tag_name of the latest GitLab release.

        Falls back to the highest version-like tag when the project has no
        formal releases.
        """
        releases = self._project.releases.list(
            order_by="released_at", sort="desc", per_page=1
        )
        if releases:
            return releases[0].tag_name
        return self._highest_tag()

    def latest_commit_date(self, branch: str) -> str:
        commits = self._project.commits.list(ref_name=branch, per_page=1, get_all=False)
        if not commits:
            raise RuntimeError(f"no commits on {branch!r} for {self._slug} on {self._host}")
        return commits[0].created_at[:10].replace("-", "")

    def _highest_tag(self) -> str:
        """Walk tags and return the one with the highest all-numeric Version."""
        best_tag: str | None = None
        best_version: Version | None = None
        for tag in self._project.tags.list(get_all=True):
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
            raise RuntimeError(
                f"no version-like tags found for {self._slug} on {self._host}"
            )
        return best_tag
