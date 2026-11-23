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
    package_type = "shared-library"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        self.tool_requires("ispc/1.32.0")

    def source(self):
        get(
            self,
            **self.conan_data["sources"][self.version],
            destination="upstream",
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ISPC_TEXCOMP_SOURCE"] = (
            Path(self.source_folder) / "upstream"
        ).as_posix()
        compiler = Path(self.dependencies.build["ispc"].package_folder) / "bin" / "ispc"
        if self.settings_build.os == "Windows":
            compiler = compiler.with_suffix(".exe")
        tc.variables["CMAKE_ISPC_COMPILER"] = compiler.as_posix()
        flags = []
        if self.settings.os == "Android":
            flags.append("--target-os=android")
        elif self.settings.os == "iOS":
            flags.append("--target-os=ios")
            if self.settings.os.sdk == "iphonesimulator":
                tc.variables["ISPC_TEXCOMP_IOS_SIMULATOR"] = True
        if self.settings.arch == "armv8":
            tc.variables["ISPC_TEXCOMP_TARGETS"] = "neon-i32x4"
            flags.append("--arch=aarch64")
        else:
            tc.variables["ISPC_TEXCOMP_TARGETS"] = "sse2-i32x4;avx1-i32x8"
        if flags:
            tc.variables["CMAKE_ISPC_FLAGS"] = " ".join(flags)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        copy(
            self,
            "LICENSE*",
            Path(self.source_folder) / "upstream",
            Path(self.package_folder) / "licenses",
        )

    def package_info(self):
        self.cpp_info.libs = ["ispc_texcomp"]
