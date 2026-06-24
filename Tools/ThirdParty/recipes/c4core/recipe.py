import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "c4core"
    version = "0.3.0"
    license = "MIT",

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self):
        self.requires("fast-float")

    def latest_version(self):
        repo = GithubRepository(self, "biojppm/c4core")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/biojppm/c4core/releases/download/v0.3.0/c4core-0.3.0-src.tgz",
            sha256="47a5634c785f84a6bef07c04c3cc3c063ff61c5c7554b95c35298712e2f306fd",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["C4CORE_WITH_FASTFLOAT"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE*", dst=os.path.join(self.folders.package, "licenses"), src=self.folders.source)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rm(self, "*.natvis", os.path.join(self.folders.package, "include"), recursive=True)

    def package_info(self):
        self.info.libs = ["c4core"]
        self.info.set_property("cmake_file_name", "c4core")
        self.info.set_property("cmake_target_name", "c4core::c4core")

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")
