import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "unordered_dense"
    version = "4.8.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "martinus/unordered_dense")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/martinus/unordered_dense/archive/v4.8.1.tar.gz",
            sha256="9f7202ec6d8353932ef865d33f5872e4b7a1356e9032da7cd09c3a0c5bb2b7de",
            destination=self.source_folder,
            strip_root=True)

    def package_id(self):
        self.info.clear()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "include", "ankerl"),
             dst=os.path.join(self.package_folder, "include", "ankerl"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "unordered_dense")
        self.cpp_info.set_property("cmake_target_name", "unordered_dense::unordered_dense")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
