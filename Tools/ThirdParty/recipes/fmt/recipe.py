import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "fmt"
    version = "12.1.0"
    license = "MIT"

    options = {
        "header_only": [True, False],
        "shared": [True, False],
        "fPIC": [True, False],
        "with_fmt_alias": [True, False],
        "with_os_api": [True, False],
    }
    default_options = {
        "header_only": False,
        "shared": False,
        "fPIC": True,
        "with_fmt_alias": False,
        "with_os_api": True,
    }

    def config_options(self):
        if str(self.settings.os) == "baremetal":
            self.options.with_os_api = False

    def configure(self):
        if self.options.header_only:
            self.options.rm_safe("fPIC")
            self.options.rm_safe("shared")
            self.options.rm_safe("with_os_api")
        elif self.options.shared:
            self.options.rm_safe("fPIC")

    def latest_version(self):
        repo = GithubRepository(self, "fmtlib/fmt")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/fmtlib/fmt/releases/download/12.1.0/fmt-12.1.0.zip",
            sha256="695fd197fa5aff8fc67b5f2bbc110490a875cdf7a41686ac8512fb480fa8ada7",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        if not self.options.header_only:
            tc = CMakeToolchain(self)
            tc.cache_variables["FMT_DOC"] = False
            tc.cache_variables["FMT_TEST"] = False
            tc.cache_variables["FMT_INSTALL"] = True
            tc.cache_variables["FMT_LIB_DIR"] = "lib"
            tc.cache_variables["FMT_OS"] = bool(self.options.with_os_api)
            tc.cache_variables["FMT_UNICODE"] = True
            tc.generate()

    def build(self):
        if not self.options.header_only:
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        copy(self, pattern="LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        if self.options.header_only:
            copy(self, pattern="*.h", src=os.path.join(self.folders.source, "include"), dst=os.path.join(self.folders.package, "include"))
        else:
            cmake = CMake(self)
            cmake.install()
            rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
            rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
            rmdir(self, os.path.join(self.folders.package, "res"))
            rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        target = "fmt-header-only" if self.options.header_only else "fmt"
        self.cpp_info.set_property("cmake_file_name", "fmt")
        self.cpp_info.set_property("cmake_target_name", f"fmt::{target}")

        # Mirror upstream find package version policy:
        # https://github.com/fmtlib/fmt/blob/11.1.1/CMakeLists.txt#L403-L407
        self.cpp_info.set_property("cmake_config_version_compat", "AnyNewerVersion")
        self.cpp_info.set_property("pkg_config_name", "fmt")

        if is_msvc(self):
            self.cpp_info.components["_fmt"].cxxflags.append("/utf-8")

        if self.options.with_fmt_alias:
            self.cpp_info.components["_fmt"].defines.append("FMT_STRING_ALIAS=1")

        if self.options.header_only:
            self.cpp_info.components["_fmt"].defines.append("FMT_HEADER_ONLY=1")
            self.cpp_info.components["_fmt"].libdirs = []
            self.cpp_info.components["_fmt"].bindirs = []
        else:
            postfix = "d" if self.settings.build_type == "Debug" else ""
            libname = "fmt" + postfix
            self.cpp_info.components["_fmt"].libs = [libname]
            if self.settings.os == "Linux":
                self.cpp_info.components["_fmt"].system_libs.extend(["m"])
            if self.options.shared:
                self.cpp_info.components["_fmt"].defines.append("FMT_SHARED")

        self.cpp_info.components["_fmt"].set_property("cmake_target_name", f"fmt::{target}")
