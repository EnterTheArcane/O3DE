#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import get


class Recipe(ConanFile):
    name = "android-sdk"
    license = "Android SDK License Agreement"
    homepage = "https://developer.android.com/tools/releases/build-tools"
    package_type = "application"
    settings = "os", "arch"

    def validate(self):
        archives = self.conan_data["sources"][self.version]["build_tools"]
        if str(self.settings.os) not in archives or (
            self.settings.arch != "x86_64"
            and (self.settings.os != "Macos" or self.settings.arch != "armv8")
        ):
            raise ConanInvalidConfiguration(
                f"No Android SDK Build Tools archive for {self.settings.os} {self.settings.arch}"
            )

    def package(self):
        root = Path(self.package_folder)
        data = self.conan_data["sources"][self.version]
        platform = data["platform"]
        get(
            self,
            url=platform["url"],
            sha256=platform["sha256"],
            destination=str(root / "platforms" / f"android-{platform['api_level']}"),
            strip_root=True,
        )
        get(
            self,
            **data["build_tools"][str(self.settings.os)],
            destination=str(root / "build-tools" / str(self.version)),
            strip_root=True,
            keep_permissions=True,
        )

    def package_id(self):
        self.info.conf.clear()

    def package_info(self):
        self.cpp_info.bindirs = [f"build-tools/{self.version}"]
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.buildenv_info.define_path("ANDROID_HOME", self.package_folder)
        self.buildenv_info.define_path("ANDROID_SDK_ROOT", self.package_folder)
