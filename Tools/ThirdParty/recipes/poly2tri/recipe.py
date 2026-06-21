import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get


class Recipe(RecipeBase):
    name = "poly2tri"
    version = "20130502"
    license = "BSD-3-Clause"

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
            self,
            url="https://github.com/greenm01/poly2tri/archive/88de49021b6d9bef6faa1bc94ceb3fbd85c3c204.zip",
            sha256="2bd25eb2b8f467382c5bc3384c8c62ab3e6c10de26be8019aa94d93f7b65806d",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["POLY2TRI_SRC_DIR"] = os.path.join(self.source_folder, "poly2tri").replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["poly2tri"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
