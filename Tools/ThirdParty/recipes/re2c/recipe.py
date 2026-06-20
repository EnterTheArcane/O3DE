import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMakeToolchain, CMake
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "re2c"
    version = "4.5.1"
    license = "LicenseRef-re2c"

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def latest_version(self):
        repo = GithubRepository(self, "skvadrik/re2c")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/skvadrik/re2c/releases/download/4.5.1/re2c-4.5.1.tar.xz",
            sha256="ffea067c11aa668bcb42885be6e6cd000302000b7747d2bb213299ec66b7864e",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["RE2C_REBUILD_DOCS"] = False
        tc.cache_variables["RE2C_BUILD_BENCHMARKS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"),
             keep_path=False)
        copy(self, "NO_WARRANTY",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"),
             keep_path=False)
        copy(self, "*.re",
             src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"),
             keep_path=False)

        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.frameworkdirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.resdirs = []
        self.cpp_info.includedirs = []

        include_dir = os.path.join(self.package_folder, "include")
        self.buildenv_info.define("RE2C_STDLIB_DIR", include_dir)
