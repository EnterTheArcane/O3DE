#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.tools.build import can_run, cross_building
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv, VirtualRunEnv


class Recipe(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def configure(self):
        if self.settings.os == "Android":
            self.settings.compiler.libcxx = "c++_shared"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build_requirements(self):
        if cross_building(self):
            self.tool_requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        VirtualRunEnv(self).generate()
        qt = self.dependencies.host["qt"]
        sdk = Path(qt.package_folder)
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.variables["Qt6_ROOT"] = sdk.as_posix()
        if self.settings.os == "Emscripten":
            tc.variables["QT_TEST_POINTER_SIZE"] = (
                8 if self.settings.arch == "wasm64" else 4
            )
            tc.cache_variables["QT_CHAINLOAD_TOOLCHAIN_FILE"] = (
                Path(self.generators_folder) / "conan_toolchain.cmake"
            ).as_posix()
        if cross_building(self):
            tc.variables["QT_HOST_PATH"] = (
                Path(self.dependencies.build["qt"].package_folder)
            ).as_posix()
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cli_args = []
        if self.settings.os == "Emscripten":
            sdk = Path(self.dependencies.host["qt"].package_folder)
            toolchain = sdk / "lib/cmake/Qt6/qt.toolchain.cmake"
            cli_args.append(f'-DCMAKE_TOOLCHAIN_FILE="{toolchain.as_posix()}"')
        cmake.configure(cli_args=cli_args)
        cmake.build()

    def test(self):
        if self.settings.os == "Emscripten":
            executable = Path(self.build_folder) / "test_package.js"
            node = VirtualBuildEnv(self).vars().get("EMSDK_NODE")
            libraries = Path(self.dependencies.host["qt"].package_folder) / "lib"
            loader = Path(self.source_folder) / "run_wasm.cjs"
            self.run(f'"{node}" "{loader}" "{executable}" "{libraries}"')
            return
        if can_run(self):
            executable = (
                Path(self.build_folder) / self.cpp.build.bindir / "test_package"
            )
            self.run(f'"{executable}"', env="conanrun")
