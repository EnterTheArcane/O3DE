import os

from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "dirent"
    version = "1.26"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "tronkko/dirent")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/tronkko/dirent/archive/1.26.tar.gz",
            sha256="a91662ee5243d2dae5aee7ed8527f95097afda517cc5cc7ca2699648a74a419c",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        copy(
            self,
            pattern="*.h",
            dst=os.path.join(self.package_folder, "include"),
            src=os.path.join(self.source_folder, "include"),
        )

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
