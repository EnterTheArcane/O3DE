import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "manifold"
    version = "3.5.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self):
        self.requires("clipper2")
        self.requires("onetbb")

    def latest_version(self):
        repo = GithubRepository(self, "elalish/manifold")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/elalish/manifold/archive/refs/tags/v3.5.0.tar.gz",
            sha256="7002091f992c80bec49b69e49c85769d862bb97169781e23b9909a4b72b6a618",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MANIFOLD_DOWNLOADS"] = False
        tc.cache_variables["MANIFOLD_TEST"] = False
        tc.cache_variables["MANIFOLD_CBIND"] = False
        tc.cache_variables["MANIFOLD_PYBIND"] = False
        tc.cache_variables["MANIFOLD_STRICT"] = False  # no -Werror
        tc.cache_variables["MANIFOLD_PAR"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("clipper2", "cmake_file_name", "Clipper2")
        deps.set_property("clipper2::clipper2", "cmake_target_name", "Clipper2")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.folders.source, os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()

        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))

    def package_info(self):
        self.info.libs = ["manifold"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")
