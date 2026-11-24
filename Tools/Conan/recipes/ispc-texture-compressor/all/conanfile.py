#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get


class Recipe(ConanFile):
    name = "ispc-texture-compressor"
    license = "MIT"
    settings = "os", "arch", "compiler", "build_type"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def configure(self):
        self.package_type = "shared-library"
        if self.settings.os == "Emscripten":
            self.package_type = "static-library"

    def layout(self):
        cmake_layout(self)
        self.cpp.package.includedirs = ["include/ISPC", "include"]

    def build_requirements(self):
        self.tool_requires("ispc/1.32.0")

    def source(self):
        get(
            self,
            **self.conan_data["sources"][self.version],
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        wasm = self.settings.os == "Emscripten"
        ispc = self.dependencies.build["ispc"]
        compiler = Path(ispc.package_folder) / "bin" / "ispc"
        if self.settings_build.os == "Windows":
            compiler = compiler.with_suffix(".exe")
        tc.variables["CMAKE_ISPC_COMPILER"] = compiler.as_posix()
        # The ISPC headers are generated during the build.
        tc.variables["CMAKE_CXX_SCAN_FOR_MODULES"] = False
        tc.variables["BUILD_SHARED_LIBS"] = not wasm
        if self.settings.arch == "wasm64":
            tc.cache_variables["CMAKE_C_FLAGS"] = "-m64"
            tc.cache_variables["CMAKE_CXX_FLAGS"] = "-m64"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        copy(
            self,
            "license.txt",
            self.source_folder,
            Path(self.package_folder) / "licenses",
        )

    def package_info(self):
        self.cpp_info.libs = ["ispc_texcomp"]
        if self.settings.os == "Emscripten":
            self.cpp_info.exelinkflags = ["-msimd128"]
            self.cpp_info.sharedlinkflags = ["-msimd128"]
        else:
            self.cpp_info.defines = ["ISPC_TEXCOMP_SHARED"]
