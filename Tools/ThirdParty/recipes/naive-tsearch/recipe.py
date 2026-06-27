import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import get, copy, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    fPIC: bool = True
    header_only: bool = True


class Recipe(RecipeBase[_Options]):
    name = "naive-tsearch"
    version = "0.1.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "kulp/naive-tsearch")
        return Version(repo.latest_release.removeprefix("v"))

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def source(self):
        get(
            self,
            url="https://github.com/kulp/naive-tsearch/releases/download/v0.1.1/naive-tsearch-0.1.1.tar.xz",
            sha256="cb779326a8748fb527ab2f4d199923c92dc7d120988b45400d4b31fd77288a1b",
            destination=self.folders.source,
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
        copy(self, pattern="LICENSE", dst=self.folders.package / "licenses", src=self.folders.source)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "share")
        if self.options.header_only:
            rmdir(self, self.folders.package / "lib")
            rm(self, "tsearch.h", self.folders.package / "include" / "naive-tsearch")
        else:
            rmdir(self, self.folders.package / "lib" / "cmake")
            rmdir(self, self.folders.package / "lib" / "pkgconfig")
            rm(self, "tsearch_hdronly.h", self.folders.package / "include" / "naive-tsearch")
            rm(self, "tsearch.c.inc", self.folders.package / "include" / "naive-tsearch")

    def package_info(self):
        if self.options.header_only:
            self.info.components["header_only"].libs = []
            self.info.components["header_only"].libdirs = []
            self.info.components["header_only"].bindirs = []
            self.info.components["header_only"].includedirs.append(os.path.join("include", "naive-tsearch"))
            self.info.components["header_only"].defines = ["NAIVE_TSEARCH_HDRONLY"]
            self.info.components["header_only"].set_property("cmake_target_name", "naive-tsearch::naive-tsearch-hdronly")
            self.info.components["header_only"].set_property("pkg_config_name", "naive-tsearch")
        else:
            self.info.components["naive_tsearch"].libs = ["naive-tsearch"]
            self.info.components["naive_tsearch"].includedirs.append(os.path.join("include", "naive-tsearch"))
            self.info.components["naive_tsearch"].set_property("cmake_target_name", "naive-tsearch::naive-tsearch")
            self.info.components["naive_tsearch"].set_property("pkg_config_name", "naive-tsearch")
