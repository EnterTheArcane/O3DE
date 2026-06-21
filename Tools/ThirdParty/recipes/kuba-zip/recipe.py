import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "kuba-zip"
    version = "0.3.8"
    license = "Unlicense"

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
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def latest_version(self):
        repo = GithubRepository(self, "kuba--/zip")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/kuba--/zip/archive/v0.3.8.tar.gz",
            sha256="944656c33aa776dc2c882991d1a6a86c8408fec8b8a19bc5305bf7eabdd4d908",
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "-Werror", "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_DISABLE_TESTING"] = True
        tc.variables["ZIP_STATIC_PIC"] = self.options.get_safe("fPIC", True)
        tc.variables["ZIP_BUILD_DOCS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "UNLICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "zip")
        self.cpp_info.set_property("cmake_target_name", "zip::zip")

        self.cpp_info.libs = ["zip"]
        if self.options.shared:
            self.cpp_info.defines.append("ZIP_SHARED")
