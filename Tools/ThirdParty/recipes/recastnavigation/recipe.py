# Ported from conan-center-index/recastnavigation by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir, rm
import os

class Recipe(RecipeBase):
    name = "recastnavigation"
    license = "Zlib"
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
        tc.cache_variables["RECASTNAVIGATION_DEMO"] = False
        tc.cache_variables["RECASTNAVIGATION_TESTS"] = False
        tc.cache_variables["RECASTNAVIGATION_EXAMPLES"] = False
        tc.cache_variables["RECASTNAVIGATION_STATIC"] = not self.options.shared
        tc.cache_variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = self.options.shared
        tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("License.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm("*.pdb", self.package_folder, recursive=True)
