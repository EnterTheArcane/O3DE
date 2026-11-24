#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv, VirtualRunEnv


class Recipe(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        VirtualRunEnv(self).generate()
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        if self.settings.arch == "wasm64":
            tc.cache_variables["CMAKE_C_FLAGS"] = "-m64"
            tc.cache_variables["CMAKE_CXX_FLAGS"] = "-m64"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        executable = Path(self.build_folder) / self.cpp.build.bindir / "test_package"
        if self.settings.os == "Emscripten":
            node = VirtualBuildEnv(self).vars().get("EMSDK_NODE")
            self.run(f'"{node}" "{executable.with_suffix(".js")}"')
        elif can_run(self):
            if self.settings.os == "Windows":
                executable = executable.with_suffix(".exe")
            self.run(f'"{executable}"', env="conanrun")
