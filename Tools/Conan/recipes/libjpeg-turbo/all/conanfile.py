#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""Adapted from ConanCenter Index's libjpeg-turbo recipe (MIT)."""

from pathlib import Path
from typing import ClassVar

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir
from conan.tools.microsoft import is_msvc


class Recipe(ConanFile):
    name = "libjpeg-turbo"
    description = "SIMD-accelerated libjpeg-compatible JPEG codec library"
    homepage = "https://libjpeg-turbo.org"
    license = ("IJG", "BSD-3-Clause", "Zlib")
    provides = "libjpeg"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "SIMD": [True, False],
        "arithmetic_encoder": [True, False],
        "arithmetic_decoder": [True, False],
        "libjpeg7_compatibility": [True, False],
        "libjpeg8_compatibility": [True, False],
        "turbojpeg": [True, False],
    }
    default_options = dict.fromkeys(options, True)
    default_options["shared"] = False

    @property
    def _msvc_abi(self):
        return is_msvc(self) or (
            self.settings.os == "Windows"
            and self.settings.compiler == "clang"
            and self.settings.get_safe("compiler.runtime") is not None
        )

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
        if self.settings.os == "Emscripten":
            self.options.rm_safe("SIMD")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility:
            self.options.rm_safe("arithmetic_encoder")
            self.options.rm_safe("arithmetic_decoder")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def build_requirements(self):
        if self.options.get_safe("SIMD") and self.settings.arch in ("x86", "x86_64"):
            self.tool_requires("nasm/2.15.05")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        jpeg_compatibility = (
            self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility
        )
        tc = CMakeToolchain(self)
        tc.blocks.remove("output_dirs")
        tc.cache_variables["CMAKE_INSTALL_DEFAULT_LIBDIR"] = "lib"
        tc.variables["ENABLE_STATIC"] = not self.options.shared
        tc.variables["ENABLE_SHARED"] = bool(self.options.shared)
        tc.variables["WITH_SIMD"] = bool(self.options.get_safe("SIMD", False))
        tc.variables["WITH_ARITH_ENC"] = bool(
            self.options.get_safe("arithmetic_encoder", False) or jpeg_compatibility
        )
        tc.variables["WITH_ARITH_DEC"] = bool(
            self.options.get_safe("arithmetic_decoder", False) or jpeg_compatibility
        )
        tc.variables["WITH_JPEG7"] = bool(self.options.libjpeg7_compatibility)
        tc.variables["WITH_JPEG8"] = bool(self.options.libjpeg8_compatibility)
        tc.variables["WITH_TURBOJPEG"] = bool(self.options.turbojpeg)
        tc.variables["WITH_TOOLS"] = False
        tc.variables["WITH_TESTS"] = False
        if self._msvc_abi:
            tc.variables["WITH_CRT_DLL"] = self.settings.compiler.runtime != "static"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        package = Path(self.package_folder)
        copy(self, "LICENSE.md", self.source_folder, package / "licenses")
        copy(self, "README.ijg", self.source_folder, package / "licenses")
        CMake(self).install()
        rmdir(self, package / "share/doc")
        rmdir(self, package / "doc")
        rmdir(self, package / "lib/pkgconfig")
        rmdir(self, package / "lib/cmake")

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_module_file_name", "JPEG")
        self.cpp_info.set_property("cmake_file_name", "libjpeg-turbo")

        target_suffix = "" if self.options.shared else "-static"
        # Upstream uses the -static filenames for MSVC and clang-cl.
        lib_suffix = target_suffix if self._msvc_abi else ""
        jpeg = self.cpp_info.components["jpeg"]
        jpeg.set_property("cmake_module_target_name", "JPEG::JPEG")
        jpeg.set_property("cmake_target_name", f"libjpeg-turbo::jpeg{target_suffix}")
        jpeg.set_property("pkg_config_name", "libjpeg")
        jpeg.libs = [f"jpeg{lib_suffix}"]
        if self.options.turbojpeg:
            turbojpeg = self.cpp_info.components["turbojpeg"]
            turbojpeg.set_property(
                "cmake_target_name", f"libjpeg-turbo::turbojpeg{target_suffix}"
            )
            turbojpeg.set_property("pkg_config_name", "libturbojpeg")
            turbojpeg.libs = [f"turbojpeg{lib_suffix}"]
