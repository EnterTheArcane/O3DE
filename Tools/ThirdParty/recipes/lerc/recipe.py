import os

from thirdparty import RecipeBase
from thirdparty.tools.build import stdcpp_library
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "lerc"
    version = "4.1.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "Esri/lerc")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/Esri/lerc/archive/refs/tags/v4.1.0.tar.gz",
            sha256="f05b24d2368becab9144873878655bb718910631550d4f786262378c16ab94a7",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libs = ["Lerc"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
        if not self.options.shared:
            self.cpp_info.defines = ["LERC_STATIC"]
            lib = stdcpp_library(self)
            if lib:
                self.cpp_info.system_libs.append(lib)
