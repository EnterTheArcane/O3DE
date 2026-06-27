import re

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, load, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "onetbb"
    version = "2023.0.0"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "uxlfoundation/oneTBB")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/uxlfoundation/oneTBB/archive/v2023.0.0.tar.gz",
            sha256="f8767b971ec6aea25dde58ae0f593e94e7aa75a739a86f67967012f69e2199b1",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = True
        tc.variables["TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH"] = True
        tc.variables["TBB_ENABLE_IPO"] = True
        tc.variables["TBB_STRICT"] = False
        tc.variables["TBB_TEST"] = False
        tc.variables["TBBMALLOC_BUILD"] = True
        tc.variables["TBBMALLOC_PROXY_BUILD"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "share")
        rm(self, "*.pdb", self.folders.package / "bin")

    def package_info(self):
        self.info.set_property("cmake_file_name", "TBB")

        def lib_name(name):
            if self.settings.build_type == "Debug":
                return name + "_debug"
            return name

        tbb = self.info.components["libtbb"]
        tbb.set_property("cmake_target_name", "TBB::tbb")
        tbb.libs = [lib_name("tbb")]
        if self.settings.os == "Windows":
            version_info = load(
                self,
                self.folders.package / "include" / "oneapi" / "tbb" / "version.h")
            binary_version = re.sub(
                r".*" + re.escape("#define __TBB_BINARY_VERSION ") + r"(\d+).*",
                r"\1",
                version_info,
                flags=re.MULTILINE | re.DOTALL,
            )
            tbb.libs.append(lib_name(f"tbb{binary_version}"))
        if self.settings.os in ["Linux", "FreeBSD"]:
            tbb.system_libs = ["m", "dl", "rt", "pthread"]

        tbbmalloc = self.info.components["tbbmalloc"]
        tbbmalloc.set_property("cmake_target_name", "TBB::tbbmalloc")
        tbbmalloc.libs = [lib_name("tbbmalloc")]
        if self.settings.os in ["Linux", "FreeBSD"]:
            tbbmalloc.system_libs = ["dl", "pthread"]

        tbbproxy = self.info.components["tbbmalloc_proxy"]
        tbbproxy.set_property("cmake_target_name", "TBB::tbbmalloc_proxy")
        tbbproxy.libs = [lib_name("tbbmalloc_proxy")]
        tbbproxy.requires = ["tbbmalloc"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            tbbproxy.system_libs = ["m", "dl", "pthread"]
