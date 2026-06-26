import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import is_apple_os
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "libsamplerate"
    version = "0.2.2"
    license = "BSD-2-Clause"

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if is_apple_os(self) and self.options.shared:
            # see https://github.com/libsndfile/libsamplerate/blob/0.2.2/src/CMakeLists.txt#L110-L119
            self.requires_tool("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "libsndfile/libsamplerate")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz",
            sha256="3258da280511d24b49d6b08615bbe824d0cacc9842b0e4caf11c52cf2b043893",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(self, os.path.join(self.folders.source, "CMakeLists.txt"), "cmake_policy(SET CMP0091 OLD)", "")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["LIBSAMPLERATE_EXAMPLES"] = False
        tc.cache_variables["LIBSAMPLERATE_INSTALL"] = True
        tc.cache_variables["BUILD_TESTING"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "SampleRate")
        self.info.set_property("cmake_target_name", "SampleRate::samplerate")
        self.info.set_property("pkg_config_name", "samplerate")
        self.info.components["samplerate"].libs = ["samplerate"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.components["samplerate"].system_libs.append("m")
        self.info.components["samplerate"].set_property("cmake_target_name", "SampleRate::samplerate")
