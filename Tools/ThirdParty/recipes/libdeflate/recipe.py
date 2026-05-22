# Ported from conan-center-index/libdeflate by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import collect_libs, copy, get, rmdir
import os

class Recipe(RecipeBase):
    name = "libdeflate"
    license = "MIT"
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
        tc.variables["LIBDEFLATE_BUILD_STATIC_LIB"] = not self.options.shared
        tc.variables["LIBDEFLATE_BUILD_SHARED_LIB"] = self.options.shared
        tc.variables["LIBDEFLATE_BUILD_GZIP"] = False
        tc.variables["LIBDEFLATE_BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("COPYING", self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
