import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import get, copy, rm, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "naive-tsearch"
    version = "0.1.1"
    license = "MIT"

    options = {
        "fPIC": [True, False],
        "header_only": [True, False],
    }
    default_options = {
        "fPIC": True,
        "header_only": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")
        if self.options.header_only:
            self.package_type = 'header-library'

    def latest_version(self):
        repo = GithubRepository(self, "kulp/naive-tsearch")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/kulp/naive-tsearch/releases/download/v0.1.1/naive-tsearch-0.1.1.tar.xz",
            sha256="cb779326a8748fb527ab2f4d199923c92dc7d120988b45400d4b31fd77288a1b",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["NAIVE_TSEARCH_INSTALL"] = True
        tc.variables["NAIVE_TSEARCH_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "share"))
        if self.options.header_only:
            rmdir(self, os.path.join(self.package_folder, "lib"))
            rm(self, "tsearch.h", os.path.join(self.package_folder, "include", "naive-tsearch"))
        else:
            rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
            rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
            rm(self, "tsearch_hdronly.h", os.path.join(self.package_folder, "include", "naive-tsearch"))
            rm(self, "tsearch.c.inc", os.path.join(self.package_folder, "include", "naive-tsearch"))

    def package_info(self):
        if self.options.header_only:
            self.cpp_info.components["header_only"].libs = []
            self.cpp_info.components["header_only"].libdirs = []
            self.cpp_info.components["header_only"].bindirs = []
            self.cpp_info.components["header_only"].includedirs.append(os.path.join("include", "naive-tsearch"))
            self.cpp_info.components["header_only"].defines = ["NAIVE_TSEARCH_HDRONLY"]
            self.cpp_info.components["header_only"].set_property("cmake_target_name", "naive-tsearch::naive-tsearch-hdronly")
            self.cpp_info.components["header_only"].set_property("pkg_config_name", "naive-tsearch")
        else:
            self.cpp_info.components["naive_tsearch"].libs = ["naive-tsearch"]
            self.cpp_info.components["naive_tsearch"].includedirs.append(os.path.join("include", "naive-tsearch"))
            self.cpp_info.components["naive_tsearch"].set_property("cmake_target_name", "naive-tsearch::naive-tsearch")
            self.cpp_info.components["naive_tsearch"].set_property("pkg_config_name", "naive-tsearch")
