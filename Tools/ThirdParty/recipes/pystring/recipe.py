import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "pystring"
    version = "1.1.5"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "imageworks/pystring")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/imageworks/pystring/archive/refs/tags/v1.1.5.tar.gz",
            sha256="63c30c251b8017c897bd923826f400aee1d6e4f1c22ffbbd2104f150522a2040",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PYSTRING_SRC_DIR"] = self.folders.source.as_posix()
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.folders.source, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.libs = ["pystring"]
