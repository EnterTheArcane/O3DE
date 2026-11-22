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
from conan.tools.layout import basic_layout


class Recipe(ConanFile):
    name = "mini-chromium"
    license = "BSD-3-Clause"
    package_type = "build-scripts"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def layout(self):
        basic_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def package(self):
        package = Path(self.package_folder)
        copy(self, "*", self.source_folder, package / "src")
        copy(self, "LICENSE", self.source_folder, package / "licenses", excludes="*/*")
