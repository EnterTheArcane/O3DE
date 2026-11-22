#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.tools.files import copy, get


class Recipe(ConanFile):
    name = "ispc"
    license = "BSD-3-Clause"
    settings = "os", "arch"
    package_type = "application"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def build(self):
        sources = self.conan_data["sources"][self.version]
        get(
            self,
            **sources[str(self.settings.os)][str(self.settings.arch)],
            strip_root=True
        )

    def package(self):
        build = Path(self.build_folder)
        package = Path(self.package_folder)
        copy(self, "*", build / "bin", package / "bin")
        copy(self, "LICENSE*", build, package / "licenses")
