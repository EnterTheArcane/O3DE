from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get
import os

class Recipe(RecipeBase):
    name = "libwebm"
    version = "1.0.0.31"
    license = "BSD-3-Clause"

    settings = "os", "arch", "compiler", "build_type"

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

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def source(self):
        get(self, url="https://github.com/webmproject/libwebm/archive/refs/tags/libwebm-1.0.0.31.tar.gz", sha256="616cfdca1c869222dc60d5a49d112c1464040390e3876afca4d385347c6ce55e", destination=self.source_folder, strip_root=True)

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
        copy(self, "LICENSE.TXT", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "webm")
        self.cpp_info.set_property("cmake_target_name", "webm::webm")
        self.cpp_info.set_property("pkg_config_name", "webm")
        self.cpp_info.libs = ["webm"]
        self.cpp_info.includedirs.append("include/webm")

        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.cpp_info.system_libs.append("m")
