import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "animation-compression-library"
    version = "2.1.0"
    license = "MIT"

    def package_id(self):
        self.info.clear()

    def latest_version(self):
        repo = GithubRepository(self, "nfrechette/acl")
        return Version(repo.latest_release.lstrip("v"))

    def requirements(self):
        self.requires("rtm")

    def source(self):
        get(
            self,
            url="https://github.com/nfrechette/acl/archive/v2.1.0.tar.gz",
            sha256="0ac8473cd30eb768bae1ef58558e3088242d6fef81f727ce7b5ff5af9be74fce",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "includes"),
             dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "acl")
        self.cpp_info.set_property("cmake_target_name", "acl::acl")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
