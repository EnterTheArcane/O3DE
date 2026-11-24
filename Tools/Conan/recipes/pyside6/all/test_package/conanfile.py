#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import struct
from pathlib import Path

from conan import ConanFile
from conan.tools.env import VirtualBuildEnv, VirtualRunEnv
from conan.tools.layout import basic_layout


class TestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        basic_layout(self)

    def generate(self):
        # The test reads PE headers directly and has no subprocess environment.
        VirtualBuildEnv(self, auto_generate=False)
        VirtualRunEnv(self, auto_generate=False)

    def test(self):
        if self.settings.os != "Windows" or self.settings.arch != "armv8":
            return

        pyside = Path(self.dependencies["pyside6"].package_folder)
        qt = Path(self.dependencies["qt"].package_folder)
        modules = pyside / "lib/site-packages/PySide6"
        binaries = [
            qt / "bin/Qt6Core.dll",
            qt / "bin/Qt6Cored.dll",
            pyside / "bin/pyside6.abi3.dll",
            pyside / "bin/pyside6_d.dll",
            pyside / "bin/libclang.dll",
            modules / "QtCore.pyd",
            modules / "QtCore_d.pyd",
        ]
        for binary in binaries:
            with binary.open("rb") as stream:
                assert stream.read(2) == b"MZ", binary
                stream.seek(0x3C)
                (pe_header,) = struct.unpack("<I", stream.read(4))
                stream.seek(pe_header)
                assert stream.read(6) == b"PE\0\0\x64\xaa", binary
