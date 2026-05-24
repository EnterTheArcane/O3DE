import os

from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import copy, get, replace_in_file, rmdir, save
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "libsamplerate"
    version = "0.2.2"
    license = "BSD-2-Clause"

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

    def build_requirements(self):
        if is_apple_os(self) and self.options.shared:
            # see https://github.com/libsndfile/libsamplerate/blob/0.2.2/src/CMakeLists.txt#L110-L119
            self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "libsndfile/libsamplerate")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz",
            sha256="3258da280511d24b49d6b08615bbe824d0cacc9842b0e4caf11c52cf2b043893",
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "cmake_policy(SET CMP0091 OLD)", "")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["LIBSAMPLERATE_EXAMPLES"] = False
        tc.cache_variables["LIBSAMPLERATE_INSTALL"] = True
        tc.cache_variables["BUILD_TESTING"] = False
        tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "SampleRate")
        self.cpp_info.set_property("cmake_target_name", "SampleRate::samplerate")
        self.cpp_info.set_property("pkg_config_name", "samplerate")
        self.cpp_info.components["samplerate"].libs = ["samplerate"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["samplerate"].system_libs.append("m")
        self.cpp_info.components["samplerate"].set_property("cmake_target_name", "SampleRate::samplerate")
