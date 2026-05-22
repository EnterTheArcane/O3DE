# Ported from conan-center-index/spirv-reflect by port_recipe.py
# REVIEW: verify all transforms are correct before building

import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get


class Recipe(RecipeBase):
    name = "spirv-reflect"
    version = "1.4.313.0"
    license = "Apache-2.0"
    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["spirv-headers"]

    def source(self):
        get(
            url="https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.313.0.tar.gz",
            dest=self.source_folder,
            sha256="a72129e23ffdf98978b0e60fcf24a14039503383cb077462677a1c59ffd168f0",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPIRV_REFLECT_STATIC_LIB"] = True
        tc.variables["SPIRV_REFLECT_EXAMPLES"] = False
        tc.variables["SPIRV_REFLECT_BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
