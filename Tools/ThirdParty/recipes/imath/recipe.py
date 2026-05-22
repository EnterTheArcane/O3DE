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
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

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
        copy("LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
