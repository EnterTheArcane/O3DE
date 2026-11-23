#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import os
from ctypes import CDLL
from pathlib import Path

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class TestNvCloth(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        nvcloth = self.dependencies.host["nvcloth"]
        tc.cache_variables["NVCLOTH_TEST_CUDA"] = bool(
            nvcloth.options.get_safe("with_cuda")
        )
        if self.settings.arch == "wasm64":
            tc.cache_variables["CMAKE_C_FLAGS"] = "-m64"
            tc.cache_variables["CMAKE_CXX_FLAGS"] = "-m64"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        binary = Path(self.build_folder) / self.cpp.build.bindir / "test_nvcloth"
        if self.settings.os == "Emscripten":
            node = os.environ.get("EMSDK_NODE", "node")
            self.run(f'"{node}" "{binary.with_suffix(".js")}"')
            return
        if not can_run(self):
            return
        nvcloth = self.dependencies.host["nvcloth"]
        if self.settings.os == "Linux" and nvcloth.options.get_safe("with_cuda"):
            # The static CUDA variant pulls in libcuda.so.1 even for this CPU test.
            try:
                CDLL("libcuda.so.1")
            except OSError:
                self.output.info(
                    "Skipping NvCloth runtime test on a host without libcuda.so.1"
                )
                return
        if self.settings.os == "Windows":
            binary = binary.with_suffix(".exe")
        self.run(f'"{binary}"')
