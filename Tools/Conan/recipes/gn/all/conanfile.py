#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from base64 import b64decode
from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, download, load, save, unzip
from conan.tools.layout import basic_layout


class Recipe(ConanFile):
    name = "gn"
    license = "BSD-3-Clause"
    package_type = "application"
    settings = "os", "arch"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def layout(self):
        basic_layout(self)

    def validate(self):
        sources = self.conan_data["sources"][self.version]
        if str(self.settings.arch) not in sources.get(str(self.settings.os), {}):
            raise ConanInvalidConfiguration(
                f"No GN binary for {self.settings.os} {self.settings.arch}"
            )

    def package_id(self):
        self.info.settings.rm_safe("os.version")

    def build(self):
        build = Path(self.build_folder)
        archive = build / "gn.zip"
        sources = self.conan_data["sources"][self.version]
        download(
            self,
            **sources[str(self.settings.os)][str(self.settings.arch)],
            filename=archive,
        )
        unzip(self, str(archive), destination=str(build), keep_permissions=True)
        encoded = build / "LICENSE.b64"
        download(self, **self.conan_data["licenses"][self.version], filename=encoded)
        save(self, build / "LICENSE", b64decode(load(self, encoded)).decode())

    def package(self):
        package = Path(self.package_folder)
        binary = "gn.exe" if self.settings.os == "Windows" else "gn"
        copy(self, binary, self.build_folder, package / "bin")
        copy(self, "LICENSE", self.build_folder, package / "licenses")

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
