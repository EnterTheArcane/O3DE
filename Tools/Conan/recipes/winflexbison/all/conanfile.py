#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get


class Recipe(ConanFile):
    name = "winflexbison"
    license = "GPL-3.0-or-later AND BSD-2-Clause"
    settings = "os", "arch"
    package_type = "application"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def validate(self):
        if self.settings.os != "Windows" or self.settings.arch != "x86_64":
            raise ConanInvalidConfiguration(
                "winflexbison requires a Windows x64 build host"
            )

    def build(self):
        sources = self.conan_data["sources"][self.version]
        get(self, **sources["tools"], destination="tools")
        get(self, **sources["source"], destination="upstream", strip_root=True)

    def package(self):
        build = Path(self.build_folder)
        package = Path(self.package_folder)
        tools = build / "tools"
        source = build / "upstream"
        for program in ("win_bison", "win_flex"):
            copy(self, f"{program}.exe", tools, package / "bin")
        copy(self, "*", tools / "data", package / "bin/data")
        copy(self, "COPYING", source, package / "licenses", excludes=("*/COPYING",))
        for program in ("bison", "flex"):
            copy(
                self,
                "COPYING",
                source / program / "src",
                package / "licenses" / program,
            )
