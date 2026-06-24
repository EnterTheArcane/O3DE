import os

from thirdparty import RecipeBase
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "tsl-robin-map"
    version = "1.4.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "Tessil/robin-map")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/Tessil/robin-map/archive/v1.4.1.tar.gz",
            sha256="0e3f53a377fdcdc5f9fed7a4c0d4f99e82bbb64175233bd13427fef9a771f4a1",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        copy(self, "*.h", src=os.path.join(self.folders.source, "include"), dst=os.path.join(self.folders.package, "include"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "tsl-robin-map")
        self.info.set_property("cmake_target_name", "tsl::robin_map")
        self.info.bindirs = []
        self.info.libdirs = []

        self.info.components["robin_map"].set_property("cmake_target_name", "tsl::robin_map")
        self.info.components["robin_map"].bindirs = []
        self.info.components["robin_map"].libdirs = []
