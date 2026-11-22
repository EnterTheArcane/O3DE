#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from io import StringIO
from pathlib import Path
import shlex
from subprocess import list2cmdline

from conan import ConanFile
from conan.tools.cmake import cmake_layout
from conan.tools.env import VirtualBuildEnv


class Recipe(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def build_requirements(self):
        self.tool_requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def test(self):
        env = VirtualBuildEnv(self, auto_generate=True).vars()
        command_line = shlex.join
        if self.settings_build.os == "Windows":
            command_line = list2cmdline

        ndk = Path(env.get("ANDROID_NDK_ROOT"))
        assert ndk == Path(env.get("ANDROID_NDK_HOME"))
        assert ndk == Path(self.conf.get("tools.android:ndk_path"))
        assert (ndk / "build/cmake/android.toolchain.cmake").is_file()
        for name in (
            "AR",
            "RANLIB",
            "STRIP",
            "NM",
            "OBJCOPY",
            "OBJDUMP",
            "READELF",
            "ADDR2LINE",
        ):
            tool = Path(env.get(name))
            assert tool.is_file() and tool.is_relative_to(ndk), (name, tool)

        readelf = env.get("READELF")
        if self.settings.os != "Android":
            self.run(command_line([readelf, "--version"]))
            return

        abi, machine = {
            "armv7": ("armeabi-v7a", "ARM"),
            "armv8": ("arm64-v8a", "AArch64"),
            "x86": ("x86", "Intel 80386"),
            "x86_64": ("x86_64", "Advanced Micro Devices X86-64"),
        }[str(self.settings.arch)]
        api = str(self.settings.os.api_level)
        assert env.get("ANDROID_ABI") == abi
        assert env.get("ANDROID_PLATFORM") == f"android-{api}"
        assert env.get("ANDROID_NATIVE_API_LEVEL") == api
        assert env.get("ANDROID_STL") == str(self.settings.compiler.libcxx)
        assert env.get("ANDROID_TOOLCHAIN") == "clang"
        sysroot = Path(env.get("SYSROOT"))
        assert sysroot.is_dir() and sysroot.is_relative_to(ndk)
        assert sysroot == Path(self.conf.get("tools.build:sysroot"))

        compilers = self.conf.get("tools.build:compiler_executables", check_type=dict)
        assert env.get("CC") == compilers["c"]
        assert env.get("CXX") == compilers["cpp"]
        source = Path(self.recipe_folder) / "probe.cpp"
        build = Path(self.build_folder)
        obj = (build / "probe.o").as_posix()
        archive = (build / "libprobe.a").as_posix()
        library = (build / "libprobe.so").as_posix()
        cxx = env.get("CXX")

        self.run(
            command_line(
                [
                    cxx,
                    f"-DEXPECTED_API={api}",
                    "-fPIC",
                    "-c",
                    source.as_posix(),
                    "-o",
                    obj,
                ]
            )
        )
        self.run(command_line([env.get("AR"), "rcs", archive, obj]))
        self.run(
            command_line(
                [
                    cxx,
                    "-shared",
                    "-Wl,--whole-archive",
                    archive,
                    "-Wl,--no-whole-archive",
                    "-o",
                    library,
                ]
            )
        )

        header = StringIO()
        self.run(command_line([readelf, "-h", library]), stdout=header, quiet=True)
        assert any(
            line.partition(":")[2].strip() == machine
            for line in header.getvalue().splitlines()
            if line.strip().startswith("Machine:")
        ), header.getvalue()
