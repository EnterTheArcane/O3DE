import os

from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "rapidjson"
    version = "1.1.0"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "Tencent/rapidjson")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz",
            sha256="bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
            strip_root=True,
            destination=self.source_folder)

    def package(self):
        copy(self, pattern="license.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, pattern="*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "RapidJSON")
        self.cpp_info.set_property("cmake_target_name", "rapidjson")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

