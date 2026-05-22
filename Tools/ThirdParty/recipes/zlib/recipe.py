# Ported from conan-center-index/zlib by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, get, rmdir, copy, rm
import os

class Recipe(RecipeBase):
    name = "zlib"
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
        get(**self.thirdparty_data["versions"][self.version],
            dest=self.source_folder, strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["ZLIB_BUILD_TESTING"] = False
        tc.cache_variables["ZLIB_BUILD_SHARED"] = self.options.shared
        tc.cache_variables["ZLIB_BUILD_STATIC"] = not self.options.shared
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "share"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm("*.pdb", os.path.join(self.package_folder, "bin"))
