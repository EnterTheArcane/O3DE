# Ported from conan-center-index/imath by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "imath"
    version = "3.2.2"
    license = "BSD-3-Clause"
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
            url="https://github.com/AcademySoftwareFoundation/Imath/releases/download/v3.2.2/Imath-3.2.2.tar.gz",
            dest=self.source_folder,
            sha256="0f5a783b424f374e6f27ec8b0c73130e89b08814ac8fa2e84fd7fe0b05862c53",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTING"] = False
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        if self.is_windows:
            # msvc needs __cplusplus macro to reflect the actual C++ standard
            tc.variables["CMAKE_CXX_FLAGS"] = "/Zc:__cplusplus"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.md",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
