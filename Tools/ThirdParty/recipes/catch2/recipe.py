# Ported from conan-center-index/catch2 by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "catch2"
    license = "BSL-1.0"
    options = {
        "fPIC": [True, False],
        "with_main": [True, False],
        "with_benchmark": [True, False],
        "with_prefix": [True, False],
        "default_reporter": [None, "ANY"],
    }
    default_options = {
        "fPIC": True,
        "with_main": False,
        "with_benchmark": False,
        "with_prefix": False,
        "default_reporter": None,
    }

    @property
    def _default_reporter_str(self):
        return str(self.options.default_reporter).strip('"')

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        tc.cache_variables["CATCH_INSTALL_DOCS"] = False    # these are cmake options, so use cache_variables
        tc.cache_variables["CATCH_INSTALL_HELPERS"] = "ON"  # these are cmake options, so use cache_variables
        tc.cache_variables["CATCH_BUILD_STATIC_LIBRARY"] = str(self.options.with_main)   # these are cmake options, so use cache_variables (str() is required for conan 1.52)
        if self.options.with_prefix:
            tc.preprocessor_definitions["CATCH_CONFIG_PREFIX_ALL"] = 1
        if self.options.get("with_benchmark", False):
            tc.preprocessor_definitions["CATCH_CONFIG_ENABLE_BENCHMARKING"] = 1
        if self.options.default_reporter:
            tc.variables["CATCH_CONFIG_DEFAULT_REPORTER"] = self._default_reporter_str
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        if self.options.with_main:
            cmake.build()

    def package(self):
        copy(pattern="LICENSE.txt", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "share"))
        for cmake_file in ["ParseAndAddCatchTests.cmake", "Catch.cmake", "CatchAddTests.cmake"]:
            copy(cmake_file,
                src=os.path.join(self.source_folder, "contrib"),
                dst=os.path.join(self.package_folder, "lib", "cmake", "Catch2"),
            )
