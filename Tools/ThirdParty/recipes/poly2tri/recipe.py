# Ported from conan-center-index/poly2tri by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get
import os

class Recipe(RecipeBase):
    name = "poly2tri"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "CMakeLists.txt"

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["POLY2TRI_SRC_DIR"] = os.path.join(self.source_folder, "poly2tri").replace("\\", "/")
        tc.generate()

    def build(self):
        import shutil
        shutil.copy(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "CMakeLists.txt"),
            os.path.normpath(os.path.join(self.source_folder, os.pardir, "CMakeLists.txt"))
        )
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
