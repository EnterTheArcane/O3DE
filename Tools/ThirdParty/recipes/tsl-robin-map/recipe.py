from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get
import os


class Recipe(RecipeBase):
    name = "tsl-robin-map"
    version = "1.3.0"
    license = "MIT"
    # Header-only library — no build options needed.

    def source(self):
        get(
            url="https://github.com/Tessil/robin-map/archive/v1.3.0.tar.gz",
            dest=self.source_folder,
            sha256="a8424ad3b0affd4c57ed26f0f3d8a29604f0e1f2ef2089f497f614b1c94c7236",
        )

    def generate(self):
        tc = CMakeToolchain(self)
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

    def package_info(self):
        # Header-only library — no compiled artifacts.
        self.cpp_info.set_property("cmake_file_name", "tsl-robin-map")
        self.cpp_info.set_property("cmake_target_name", "tsl::robin_map")
