# Ported from conan-center-index/spirv-headers by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "spirv-headers"
    version = "1.4.313.0"
    license = "MIT-KhronosGroup"

    def source(self):
        get(
            url="https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/vulkan-sdk-1.4.313.0.tar.gz",
            dest=self.source_folder,
            sha256="f68be549d74afb61600a1e3a7d1da1e6b7437758c8e77d664909f88f302c5ac1",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPIRV_HEADERS_SKIP_EXAMPLES"] = True
        if Version(self.version) > "1.3.275.0":
            tc.variables["SPIRV_HEADERS_ENABLE_TESTS"] = False
        if Version(self.version) <= "1.3.243.0":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
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
        rmdir(os.path.join(self.package_folder, "lib"))
        rmdir(os.path.join(self.package_folder, "share"))
