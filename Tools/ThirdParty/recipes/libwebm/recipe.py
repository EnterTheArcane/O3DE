# Ported from conan-center-index/libwebm by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get
import os


class Recipe(RecipeBase):
    name = "libwebm"
    version = "1.0.0.31"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_pes_ts": [True, False],
        "with_new_parser_api": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        "with_pes_ts": True,
        "with_new_parser_api": False,
    }

    def source(self):
        get(
            url="https://github.com/webmproject/libwebm/archive/refs/tags/libwebm-1.0.0.31.tar.gz",
            dest=self.source_folder,
            sha256="616cfdca1c869222dc60d5a49d112c1464040390e3876afca4d385347c6ce55e",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ENABLE_WEBMTS"] = self.options.with_pes_ts
        tc.variables["ENABLE_WEBM_PARSER"] = self.options.with_new_parser_api
        tc.variables["ENABLE_WEBMINFO"] = False
        tc.variables["ENABLE_SAMPLE_PROGRAMS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            "LICENSE.TXT",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
