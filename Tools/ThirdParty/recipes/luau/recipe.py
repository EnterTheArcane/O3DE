# Ported from conan-center-index/luau by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
from thirdparty.tools.cmake import CMake, CMakeToolchain

import os

class Recipe(RecipeBase):
    name = "luau"
    license = "MIT"
    options = {
        "with_cli": [True, False],
        "with_web": [True, False],
        "native_code_gen": [True, False],
    }
    default_options = {
        "with_cli": False,
        "with_web": False,
        "native_code_gen": False,
    }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LUAU_BUILD_CLI"] = self.options.with_cli
        tc.variables["LUAU_BUILD_TESTS"] = False
        tc.variables["LUAU_BUILD_WEB"] = self.options.with_web
        tc.variables["LUAU_WERROR"] = False
        tc.variables["LUAU_STATIC_CRT"] = False
        tc.variables["LUAU_NATIVE"] = self.options.native_code_gen
        tc.variables["LUAU_SRC_DIR"] = self.source_folder.replace("\\", "/")
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
        copy("lua_LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

