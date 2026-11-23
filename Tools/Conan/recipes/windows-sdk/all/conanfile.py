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
from conan.tools.files import copy, get, save

ARCHITECTURES = {"x86_64": "x64", "armv8": "arm64"}
INCLUDE_DIRS = ("ucrt", "shared", "um", "winrt", "cppwinrt")
LIB_DIRS = ("ucrt", "um")


class Recipe(ConanFile):
    name = "windows-sdk"
    license = "Microsoft Windows SDK License"
    homepage = "https://developer.microsoft.com/windows/downloads/windows-sdk/"
    settings = "os", "arch"
    package_type = "application"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def validate(self):
        if (
            self.settings.os != "Windows"
            or str(self.settings.arch) not in ARCHITECTURES
        ):
            raise ConanInvalidConfiguration(
                "Windows SDK tools require Windows x64 or ARM64"
            )
        target = self.settings_target
        if target and (target.os != "Windows" or str(target.arch) not in ARCHITECTURES):
            raise ConanInvalidConfiguration(
                "Windows SDK supports Windows x64 and ARM64 targets"
            )

    def package_id(self):
        self.info.conf.clear()

    def package(self):
        data = self.conan_data["sources"][self.version]
        version = data["sdk_version"]
        root = Path(self.package_folder)
        staging = Path(self.build_folder) / "sdk"

        headers = staging / "headers"
        get(self, **data["headers"], destination=str(headers))
        for folder in ("Include", "Source", "UnionMetadata", "References"):
            copy(self, "*", headers / "c" / folder / version, root / folder / version)
        for folder in ("DesignTime", "Platforms"):
            copy(self, "*", headers / "c" / folder, root / folder)
        # The SDK's MSBuild props locate the headers relative to this manifest.
        copy(
            self,
            "SDKManifest.xml",
            headers / "c",
            root,
            excludes=("*/SDKManifest.xml",),
        )

        tools = staging / "tools"
        get(self, **data["tools"], destination=str(tools))
        host_arch = ARCHITECTURES[str(self.settings.arch)]
        bin_dir = Path("bin") / version / host_arch
        copy(self, "*", tools / bin_dir, root / bin_dir)

        # A single x64 tools package can build for both target architectures.
        for arch, archive in data["libraries"].items():
            libraries = staging / arch
            get(self, **archive, destination=str(libraries))
            for api in LIB_DIRS:
                copy(
                    self,
                    "*",
                    libraries / "c" / api / arch,
                    root / "Lib" / version / api / arch,
                )

        save(
            self,
            root / "licenses" / "NOTICE.txt",
            "Microsoft Windows SDK License: https://aka.ms/WinSDKLicenseURL\n",
        )

    def package_info(self):
        version = self.conan_data["sources"][self.version]["sdk_version"]
        root = Path(self.package_folder)
        target_arch = ARCHITECTURES[str((self.settings_target or self.settings).arch)]
        host_arch = ARCHITECTURES[str(self.settings.arch)]
        bin_dir = f"bin/{version}/{host_arch}"
        self.cpp_info.bindirs = [bin_dir]
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []

        env = self.buildenv_info
        # MSBuild resolves its SDK roots through the version-specific properties.
        for variable in (
            "WindowsSdkDir",
            "WindowsSdkDir_10",
            "UniversalCRTSdkDir",
            "UniversalCRTSdkDir_10",
            "UCRTContentRoot",
        ):
            env.define_path(variable, f"{root}\\")
        env.define_path("WindowsSdkBinPath", f"{root / 'bin'}\\")
        env.define_path("WindowsSdkVerBinPath", f"{root / 'bin' / version}\\")
        env.define("WindowsSDKVersion", f"{version}\\")
        env.define("WindowsSDKLibVersion", f"{version}\\")
        env.define("UCRTVersion", version)
        env.define("WindowsTargetPlatformVersion", version)
        env.define_path("RC", str(root / bin_dir / "rc.exe"))
        # Native tools expect semicolon-separated paths even when launched from MSYS2.
        for directory in reversed(INCLUDE_DIRS):
            include_path = root / "Include" / version / directory
            env.prepend("INCLUDE", include_path.as_posix(), separator=";")
        for directory in reversed(LIB_DIRS):
            library_path = root / "Lib" / version / directory / target_arch
            env.prepend("LIB", library_path.as_posix(), separator=";")
        for directory in ("References", "UnionMetadata"):
            metadata_path = root / directory / version
            env.prepend("LIBPATH", metadata_path.as_posix(), separator=";")
