# Ported from conan-center-index/re2c by port_recipe.py
# REVIEW: verify all transforms are correct before building

import os

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, copy, get, rmdir, replace_in_file
from thirdparty.tools.cmake import CMakeToolchain, CMake
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "re2c"
    version = "4.3"
    license = "LicenseRef-re2c"

    def source(self):
        get(
            url="https://github.com/skvadrik/re2c/releases/download/4.3/re2c-4.3.tar.xz",
            dest=self.source_folder,
            sha256="51e88d6d6b6ab03eb7970276aca7e0db4f8e29c958b84b561d2fdcb8351c7150",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["RE2C_REBUILD_DOCS"] = False
        tc.cache_variables["RE2C_BUILD_BENCHMARKS"] = False
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
            keep_path=False,
        )
        copy(
            "NO_WARRANTY",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
            keep_path=False,
        )
        copy(
            "*.re",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
            keep_path=False,
        )

        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "share"))
