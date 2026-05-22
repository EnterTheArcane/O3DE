# Ported from conan-center-index/gtest by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.microsoft import is_msvc_static_runtime, msvc_runtime_flag
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "gtest"
    version = "1.17.0"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_gmock": [True, False],
        "no_main": [True, False],
        "hide_symbols": [True, False],
        "disable_pthreads": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_gmock": True,
        "no_main": False,
        "hide_symbols": False,
        "disable_pthreads": False,
    }
    implements = ["auto_shared_fpic"]

    def source(self):
        get(
            url="https://github.com/google/googletest/archive/v1.17.0.tar.gz",
            dest=self.source_folder,
            sha256="65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c",
        )

        internal_utils = os.path.join(
            self.source_folder, "googletest", "cmake", "internal_utils.cmake"
        )
        replace_in_file(internal_utils, "-WX", "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_GMOCK"] = bool(self.options.build_gmock)
        tc.variables["gtest_hide_internal_symbols"] = bool(self.options.hide_symbols)

        if self.is_windows:
            tc.variables["gtest_force_shared_crt"] = "MD" in msvc_runtime_flag(self)
        tc.variables["gtest_disable_pthreads"] = self.options.disable_pthreads
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses")
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm("*.pdb", os.path.join(self.package_folder, "lib"))
