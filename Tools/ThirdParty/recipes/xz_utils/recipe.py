import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "xz_utils"
    version = "5.8.3"
    license = "Unlicense", "LGPL-2.1-or-later", "GPL-2.0-or-later", "GPL-3.0-or-later"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tools": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": False,
    }

    def source(self):
        get(
            url="https://tukaani.org/xz/xz-5.8.3.tar.xz",
            dest=self.source_folder,
            sha256="fff1ffcf2b0da84d308a14de513a1aa23d4e9aa3464d17e64b9714bfdd0bbfb6",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        if not self.options.with_tools:
            tc.cache_variables["XZ_TOOL_XZ"] = False
            tc.cache_variables["XZ_TOOL_XZDEC"] = False
            tc.cache_variables["XZ_TOOL_LZMADEC"] = False
            tc.cache_variables["XZ_TOOL_LZMAINFO"] = False
            tc.cache_variables["ENABLE_SCRIPTS"] = False
            tc.cache_variables["XZ_TOOL_SYMLINKS"] = False
            tc.cache_variables["XZ_TOOL_SYMLINKS_LZMA"] = False
            tc.cache_variables["XZ_DOC"] = False
            tc.cache_variables["XZ_SANDBOX"] = "no"
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
