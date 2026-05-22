# Ported from conan-center-index/abseil by port_recipe.py

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
import os


class Recipe(RecipeBase):
    name = "abseil"
    version = "20260107.1"
    license = "Apache-2.0"
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
            url="https://github.com/abseil/abseil-cpp/archive/20260107.1.tar.gz",
            dest=self.source_folder,
            sha256="4314e2a7cbac89cac25a2f2322870f343d81579756ceff7f431803c2c9090195",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["ABSL_ENABLE_INSTALL"] = True
        tc.cache_variables["ABSL_PROPAGATE_CXX_STD"] = True
        tc.cache_variables["BUILD_TESTING"] = False
        tc.cache_variables["CMAKE_CXX_STANDARD"] = "17"
        tc.cache_variables["CMAKE_CXX_STANDARD_REQUIRED"] = True
        tc.cache_variables["CMAKE_CXX_FLAGS"] = "/std:c++17"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
