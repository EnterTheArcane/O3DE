import os

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "robin-hood-hashing"
    version = "3.11.5"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "martinus/robin-hood-hashing")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/martinus/robin-hood-hashing/archive/refs/tags/3.11.5.tar.gz",
            sha256="3693e44dda569e9a8b87ce8263f7477b23af448a3c3600c8ab9004fe79c20ad0",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "robin_hood.h", src=os.path.join(self.source_folder, "src", "include"),
                                   dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "robin_hood")
        self.cpp_info.set_property("cmake_target_name", "robin_hood::robin_hood")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

