# Ported from conan-center-index/abseil by port_recipe.py

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
import os

class Recipe(RecipeBase):
    name = "abseil"
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
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

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
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
