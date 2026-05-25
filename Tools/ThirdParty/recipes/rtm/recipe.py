import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "rtm"
    version = "2.3.0"
    license = "MIT"

    def package_id(self):
        self.info.clear()

    def latest_version(self):
        repo = GithubRepository(self, "nfrechette/rtm")
        return Version(repo.latest_release.lstrip("v"))

    def source(self):
        get(
            self,
            url="https://github.com/nfrechette/rtm/archive/v2.3.0.tar.gz",
            sha256="2b5f2c3761bb52ae89802a574e9dc9949aec3b183f7e100b9b66a65adcc6f5ab",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "includes"),
             dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "rtm")
        self.cpp_info.set_property("cmake_target_name", "rtm::rtm")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
