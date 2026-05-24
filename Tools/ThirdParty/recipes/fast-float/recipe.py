import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "fast-float"
    version = "8.2.5"
    license = "Apache-2.0", "MIT", "BSL-1.0"

    def latest_version(self):
        repo = GithubRepository(self, "fastfloat/fast_float")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/fastfloat/fast_float/archive/refs/tags/v8.2.5.tar.gz",
            sha256="17c7fb14499fcf42c3f5d143df0fbe22172e92749ec5f75ef13224005421a654",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "FastFloat")
        self.cpp_info.set_property("cmake_target_name", "FastFloat::fast_float")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        self.cpp_info.components["fastfloat"].set_property("cmake_target_name", "FastFloat::fast_float")
        self.cpp_info.components["fastfloat"].bindirs = []
        self.cpp_info.components["fastfloat"].libdirs = []
