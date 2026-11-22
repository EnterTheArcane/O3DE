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
from conan.tools.cmake import cmake_layout
from conan.tools.files import mkdir


class Recipe(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def test(self):
        if not can_run(self):
            return

        package = Path(self.dependencies["directxshadercompiler"].package_folder)
        dxc = package / "bin" / "dxc"
        if self.settings.os == "Windows":
            dxc = dxc.with_suffix(".exe")
        output = Path(self.build_folder)
        mkdir(self, output)
        self.run(
            f'"{dxc}" -spirv -fvk-disable-depth-hint -T ps_6_0 -E main '
            f'-Fo "{output / "test.spv"}" "{Path(self.source_folder) / "test.hlsl"}"',
            scope="run",
        )
