import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "tsl-robin-map"
    version = "1.4.0"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "Tessil/robin-map")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/Tessil/robin-map/archive/v1.4.0.tar.gz",
            sha256="7930dbf9634acfc02686d87f615c0f4f33135948130b8922331c16d90a03250c",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "tsl-robin-map")
        self.cpp_info.set_property("cmake_target_name", "tsl::robin_map")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        self.cpp_info.components["robin_map"].set_property("cmake_target_name", "tsl::robin_map")
        self.cpp_info.components["robin_map"].bindirs = []
        self.cpp_info.components["robin_map"].libdirs = []
