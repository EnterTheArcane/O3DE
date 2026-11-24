#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanException
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
        dependency = self.dependencies["directxshadercompiler"]
        bin_dir = Path(dependency.package_folder) / "bin"
        suffix = ".exe" if self.settings.os == "Windows" else ""
        dxc = bin_dir / f"dxc{suffix}"
        if dependency.options.tablegen_only:
            for tool in ("llvm-tblgen", "clang-tblgen"):
                if not (bin_dir / f"{tool}{suffix}").is_file():
                    raise ConanException(f"Missing DXC build tool {tool}{suffix}")
            if dxc.exists():
                raise ConanException("The TableGen package contains the DXC executable")
            return
        if any(bin_dir.glob("*-tblgen*")):
            raise ConanException("The DXC SDK contains TableGen build tools")
        if not can_run(self):
            return

        output = Path(self.build_folder)
        mkdir(self, output)
        self.run(
            f'"{dxc}" -spirv -fvk-disable-depth-hint -T ps_6_0 -E main '
            f'-Fo "{output / "test.spv"}" "{Path(self.source_folder) / "test.hlsl"}"',
            scope="run",
        )
