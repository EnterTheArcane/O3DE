import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "openddl-parser"
    version = "0.5.2"
    license = "MIT"

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

    def latest_version(self):
        repo = GithubRepository(self, "kimkulling/openddl-parser")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/kimkulling/openddl-parser/archive/v0.5.2.tar.gz",
            sha256="8058caacdc989a010c2ad3ab62df99f9f3034b4981649c5fb832efa6fbf10c36",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["DDL_STATIC_LIBRARY"] = not self.options.shared
        tc.variables["DDL_BUILD_TESTS"] = False
        tc.variables["DDL_BUILD_PARSER_DEMO"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "openddlparser")
        self.cpp_info.set_property("cmake_target_name", "openddlparser::openddlparser")
        self.cpp_info.libs = ["openddlparser"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
        if not self.options.shared:
            self.cpp_info.defines.append("OPENDDL_STATIC_LIBARY")
