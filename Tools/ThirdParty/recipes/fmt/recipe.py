from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
import os


class Recipe(RecipeBase):
    name = "fmt"
    version = "11.1.4"
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
            url="https://github.com/fmtlib/fmt/releases/download/11.1.4/fmt-11.1.4.zip",
            dest=self.source_folder,
            sha256="49b039601196e1a765e81c5c9a05a61ed3d33f23b3961323d7322e4fe213d3e6",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["FMT_TEST"] = False
        tc.variables["FMT_INSTALL"] = True
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
        rmdir(os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["fmt"]
        self.cpp_info.set_property("cmake_file_name", "fmt")
        self.cpp_info.set_property("cmake_target_name", "fmt::fmt")
