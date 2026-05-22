from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rmdir
from thirdparty.tools.scm import Version
import os


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

    def source(self):
        get(
            url="https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz",
            dest=self.source_folder,
            sha256="3258da280511d24b49d6b08615bbe824d0cacc9842b0e4caf11c52cf2b043893",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["LIBSAMPLERATE_EXAMPLES"] = False
        tc.cache_variables["LIBSAMPLERATE_INSTALL"] = True
        tc.cache_variables["BUILD_TESTING"] = False
        tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"  # CMake 4 support
        tc.generate()

    def _patch_sources(self):
        # Disable upstream logic about msvc runtime policy, called before conan toolchain resolution
        replace_in_file(
            os.path.join(self.source_folder, "CMakeLists.txt"),
            "cmake_policy(SET CMP0091 OLD)",
            "",
        )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "COPYING",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
