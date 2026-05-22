# Ported from conan-center-index/expat by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import collect_libs, copy, get, rmdir
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "expat"
    version = "2.8.1"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "char_type": ["char", "wchar_t", "ushort"],
        "large_size": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "char_type": "char",
        "large_size": False,
    }

    def source(self):
        get(
            url="https://github.com/libexpat/libexpat/releases/download/R_2_8_1/expat-2.8.1.tar.xz",
            dest=self.source_folder,
            sha256="10b195ee78160a908388180a8fe3603d4e9a12f4755fbf5f3816b23a9d750da0",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["EXPAT_BUILD_DOCS"] = False
        tc.variables["EXPAT_BUILD_EXAMPLES"] = False
        tc.variables["EXPAT_SHARED_LIBS"] = self.options.shared
        tc.variables["EXPAT_BUILD_TESTS"] = False
        tc.variables["EXPAT_BUILD_TOOLS"] = False
        tc.variables["EXPAT_CHAR_TYPE"] = self.options.char_type
        if self.is_windows:
            tc.variables["EXPAT_MSVC_STATIC_CRT"] = False
        tc.variables["EXPAT_BUILD_PKGCONFIG"] = False
        tc.variables["EXPAT_LARGE_SIZE"] = self.options.large_size
        tc.generate()

    def build(self):
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
