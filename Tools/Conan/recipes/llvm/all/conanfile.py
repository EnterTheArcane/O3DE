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
from conan.tools.files import copy, get, rename
from conan.tools.layout import basic_layout
from conan.tools.scm import Version


class Recipe(ConanFile):
    name = "llvm"
    license = "Apache-2.0 WITH LLVM-exception"
    homepage = "https://llvm.org/"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    def set_version(self):
        if self.version is None:
            self.version = max(self.conan_data["sources"], key=Version)

    def layout(self):
        basic_layout(self)

    def package_id(self):
        # Conan shares binary status between nodes with the same package ID.
        if self.info.invalid:
            return
        # The upstream archive depends only on the platform and architecture.
        self.info.settings.rm_safe("os.version")
        self.info.settings.rm_safe("compiler")
        self.info.settings.rm_safe("build_type")
        self.info.conf.clear()

    def validate(self):
        archives = self.conan_data["sources"][self.version]
        if str(self.settings.arch) not in archives.get(str(self.settings.os), {}):
            raise ConanInvalidConfiguration(
                f"No LLVM archive for {self.settings.os} {self.settings.arch}"
            )
        if self.context == "build":
            return
        if (
            self.settings.os == "Macos"
            and Version(self.settings.os.version or "0") < "14.0"
        ):
            raise ConanInvalidConfiguration(
                "The macOS LLVM SDK requires a deployment target of 14.0 or newer"
            )
        if self.settings.os == "Windows":
            compiler = self.settings.compiler
            if (
                compiler not in ("msvc", "clang")
                or compiler.get_safe("runtime") != "static"
                or compiler.get_safe("runtime_type") != "Release"
            ):
                raise ConanInvalidConfiguration(
                    "The Windows LLVM SDK requires the static Release MSVC CRT"
                )

    def package(self):
        root = Path(self.package_folder)
        data = self.conan_data["sources"][self.version][str(self.settings.os)]
        get(
            self,
            **data[str(self.settings.arch)],
            destination=str(root),
            strip_root=True,
            extract_filter="data",
        )
        windows = self.settings.os == "Windows"
        copy(
            self,
            "LICENSE.TXT",
            root / "include/llvm/Support",
            root / "licenses",
        )
        tools = root / "bin"
        # Clang derives its target from the executable name, including for clang-cl.
        cross = "aarch64" if self.settings.arch == "x86_64" else "x86_64"
        if windows:
            copy(self, "clang-cl.exe", tools, self.build_folder)
            rename(
                self,
                Path(self.build_folder) / "clang-cl.exe",
                tools / f"{cross}-pc-windows-msvc-clang-cl.exe",
            )
        elif self.settings.os == "Linux":
            for driver in ("clang", "clang++"):
                (tools / f"{cross}-linux-gnu-{driver}").symlink_to(driver)
            # CMake can prefer versioned system programs to unversioned tools.
            version = Version(self.version)
            for tool in ("clang-scan-deps", "llvm-ar", "llvm-ranlib"):
                alias = f"{tool}-{version.major}.{version.minor}"
                (tools / alias).symlink_to(tool)
                (tools / f"{cross}-linux-gnu-{alias}").symlink_to(tool)

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = ["."]
        if self.context == "build":
            # Keep SDK shared libraries out of the loader path for system tools.
            self.cpp_info.libdirs = []
        tools = Path(self.package_folder) / "bin"
        target = self.settings_target or self.settings
        cross = target.arch != self.settings.arch
        arch = "aarch64" if target.arch == "armv8" else "x86_64"
        if self.settings.os == "Windows":
            prefix = f"{arch}-pc-windows-msvc-" if cross else ""
            clang_cl = (tools / f"{prefix}clang-cl.exe").as_posix()
            compilers = {"c": clang_cl, "cpp": clang_cl}
            if cross:
                # Meson detects clang-cl by its executable name.
                command = [
                    (tools / "clang-cl.exe").as_posix(),
                    f"--target={arch}-pc-windows-msvc",
                ]
                self.conf_info.define(
                    "tools.meson.mesontoolchain:extra_variables",
                    {"binaries": {"c": command, "cpp": command}},
                )
        else:
            linux = self.settings.os == "Linux"
            prefix = f"{arch}-linux-gnu-" if linux and cross else ""
            compilers = {
                "c": (tools / f"{prefix}clang").as_posix(),
                "cpp": (tools / f"{prefix}clang++").as_posix(),
            }
            if linux:
                for tool in ("ar", "ranlib", "nm", "strip", "readelf", "objcopy"):
                    self.buildenv_info.define_path(
                        tool.upper(), (tools / f"llvm-{tool}").as_posix()
                    )
        self.conf_info.define("tools.build:compiler_executables", compilers)
        self.buildenv_info.define_path("CC", compilers["c"])
        self.buildenv_info.define_path("CXX", compilers["cpp"])
