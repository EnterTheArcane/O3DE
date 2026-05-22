# Ported from conan-center-index/libwebp by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "libwebp"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_simd": [True, False],
        "near_lossless": [True, False],
        "swap_16bit_csp": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_simd": True,
        "near_lossless": True,
        "swap_16bit_csp": False,
    }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        # should be an option but it doesn't work yet
        tc.variables["WEBP_ENABLE_SIMD"] = self.options.with_simd
        tc.variables["WEBP_NEAR_LOSSLESS"] = self.options.near_lossless
        tc.variables["WEBP_ENABLE_SWAP_16BIT_CSP"] = self.options.swap_16bit_csp
        # avoid finding system libs
        tc.variables["CMAKE_DISABLE_FIND_PACKAGE_GIF"] = True
        tc.variables["CMAKE_DISABLE_FIND_PACKAGE_PNG"] = True
        tc.variables["CMAKE_DISABLE_FIND_PACKAGE_TIFF"] = True
        tc.variables["CMAKE_DISABLE_FIND_PACKAGE_JPEG"] = True
        tc.variables["WEBP_BUILD_ANIM_UTILS"] = False
        tc.variables["WEBP_BUILD_CWEBP"] = False
        tc.variables["WEBP_BUILD_DWEBP"] = False
        tc.variables["WEBP_BUILD_IMG2WEBP"] = False
        tc.variables["WEBP_BUILD_GIF2WEBP"] = False
        tc.variables["WEBP_BUILD_VWEBP"] = False
        tc.variables["WEBP_BUILD_EXTRAS"] = False
        tc.variables["WEBP_BUILD_WEBPINFO"] = False
        if Version(self.version) >= "1.2.1":
            tc.variables["WEBP_BUILD_LIBWEBPMUX"] = True
        tc.variables["WEBP_BUILD_WEBPMUX"] = False
        if self.options.shared and self.is_windows:
            # Building a dll (see fix-dll-export patch)
            tc.preprocessor_definitions["WEBP_DLL"] = 1
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy("COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))

