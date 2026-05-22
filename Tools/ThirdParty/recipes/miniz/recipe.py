# Ported from conan-center-index/miniz by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "miniz"
    version = "3.1.1"
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
        get(
            url="https://github.com/richgel999/miniz/archive/refs/tags/3.1.1.tar.gz",
            sha256="8bb29c7bd6f22356e5583e794bed4a0b3e6dfcbcadb49974fc9270ccca1e5557",
            dest=self.source_folder,
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_FUZZERS"] = False
        tc.variables["AMALGAMATE_SOURCES"] = False
        tc.variables["BUILD_HEADER_ONLY"] = False
        tc.variables["INSTALL_PROJECT"] = True
        tc.cache_variables["BUILD_TESTS"] = False
        # Honor BUILD_SHARED_LIBS from conan_toolchain (see https://github.com/conan-io/conan/issues/11840)
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        if Version(self.version) <= "3.0.2":
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
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
