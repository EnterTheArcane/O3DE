#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import shlex
from pathlib import Path

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import cmake_layout


class TestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def test(self):
        if self.settings.os not in ("Linux", "Macos") or not can_run(self):
            return
        root = Path(self.dependencies["crashpad"].package_folder)
        self.run(shlex.join([str(root / "bin/crashpad_handler"), "--help"]))
