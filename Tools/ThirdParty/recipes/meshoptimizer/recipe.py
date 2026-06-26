import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.build import stdcpp_library
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "meshoptimizer"
    version = "1.1.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "zeux/meshoptimizer")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.1.1.tar.gz",
            sha256="30cd4d28fe71bf58c614c23c87fed385bac223acbb2dfaf343d20ffc3584a083",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MESHOPT_BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rm(self, "*.pdb", os.path.join(self.folders.package, "bin"))
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "meshoptimizer")
        self.info.set_property("cmake_target_name", "meshoptimizer::meshoptimizer")
        self.info.libs = ["meshoptimizer"]
        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                self.info.system_libs.append(libcxx)
        if self.options.shared:
            self.info.defines = ["MESHOPTIMIZER_ALLOC_EXPORT"]
            if self.settings.os == "Windows":
                self.info.defines.append("MESHOPTIMIZER_API=__declspec(dllimport)")
