import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "sse2neon"
    version = "1.9.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "DLTcollab/sse2neon")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/DLTcollab/sse2neon/archive/refs/tags/v1.9.1.tar.gz",
            sha256="6b70e7cb8c5ce4641002b85deaafe97efdf9ade9b49884edeaf678b35f0e132f",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "sse2neon.h", src=self.source_folder, dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
