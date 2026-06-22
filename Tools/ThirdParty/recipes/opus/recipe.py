import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "opus"
    version = "1.6.1"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def latest_version(self):
        repo = GithubRepository(self, "xiph/opus")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/xiph/opus/archive/refs/tags/v1.6.1.tar.gz",
            sha256="bf0b97ec7a65890b8db90ef94c4d6c18de12584c3085031953a10986f5917745",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["OPUS_BUILD_SHARED_LIBRARY"] = self.options.shared
        tc.cache_variables["OPUS_FIXED_POINT"] = False
        tc.cache_variables["OPUS_STACK_PROTECTOR"] = True
        if is_msvc(self):
            tc.cache_variables["OPUS_STATIC_RUNTIME"] = is_msvc_static_runtime(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Opus")
        self.cpp_info.set_property("cmake_target_name", "Opus::opus")
        self.cpp_info.set_property("pkg_config_name", "opus")
        self.cpp_info.components["libopus"].libs = ["opus"]
        self.cpp_info.components["libopus"].includedirs.append(os.path.join("include", "opus"))
        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.cpp_info.components["libopus"].system_libs.append("m")
        if self.settings.os == "Windows" and self.settings.compiler == "gcc":
            self.cpp_info.components["libopus"].system_libs.append("ssp")

        self.cpp_info.components["libopus"].set_property("cmake_target_name", "Opus::opus")
        self.cpp_info.components["libopus"].set_property("pkg_config_name", "opus")
