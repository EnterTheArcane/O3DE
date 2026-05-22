# Ported from conan-center-index/md4c by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rmdir
from thirdparty.tools.scm import Version

import os


class Recipe(RecipeBase):
    name = "md4c"
    version = "0.5.2"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "md2html": [True, False],
        "encoding": ["utf-8", "utf-16", "ascii"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        # "md2html": True,  # conditional default value in config_options
        "encoding": "utf-8",
    }

    def source(self):
        get(
            url="https://github.com/mity/md4c/archive/refs/tags/release-0.5.2.tar.gz",
            dest=self.source_folder,
            sha256="55d0111d48fb11883aaee91465e642b8b640775a4d6993c2d0e7a8092758ef21",
        )
        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_MD2HTML_EXECUTABLE"] = self.options.get(
            "md2html", True
        )
        if self.options.encoding == "utf-8":
            tc.preprocessor_definitions["MD4C_USE_UTF8"] = "1"
        elif self.options.encoding == "utf-16":
            tc.preprocessor_definitions["MD4C_USE_UTF16"] = "1"
        elif self.options.encoding == "ascii":
            tc.preprocessor_definitions["MD4C_USE_ASCII"] = "1"
        if Version(self.version) < "0.5.0":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # Honor encoding option
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            'COMPILE_FLAGS "-DMD4C_USE_UTF8"',
            "",
        )

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            pattern="LICENSE.md",
            dst=os.path.join(self.package_folder, "licenses"),
            src=self.source_folder,
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
