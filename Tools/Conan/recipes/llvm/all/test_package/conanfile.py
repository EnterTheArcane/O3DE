#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import shlex
from io import StringIO
from pathlib import Path
from subprocess import list2cmdline

from conan import ConanFile
from conan.tools.apple import XCRun
from conan.tools.build import can_run
from conan.tools.cmake import cmake_layout
from conan.tools.files import save
from conan.tools.microsoft import VCVars


class TestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def build_requirements(self):
        self.tool_requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        if self.settings.os == "Windows":
            VCVars(self).generate()

    def test(self):
        windows = self.settings.os == "Windows"
        arm = self.settings.arch == "armv8"
        architecture = "_M_ARM64" if arm else "_M_X64"
        if not windows:
            architecture = "__aarch64__" if arm else "__x86_64__"
        command_line = list2cmdline if windows else shlex.join
        compilers = self.conf.get("tools.build:compiler_executables", check_type=dict)
        version = self.dependencies.build["llvm"].ref.version
        output = StringIO()
        self.run(command_line([compilers["c"], "--version"]), stdout=output)
        assert f"clang version {version} " in output.getvalue()

        source = Path(self.build_folder) / "main.cpp"
        binary = source.with_name("llvm-test.exe" if windows else "llvm-test")
        save(
            self,
            source,
            f"""#include <string>
#if !defined(__clang__) || !defined({architecture})
#error Unexpected compiler or target
#endif
int main() {{ return std::string("LLVM").size() != 4; }}
""",
        )
        if windows:
            runtime = (
                "/MDd" if self.settings.compiler.runtime_type == "Debug" else "/MD"
            )
            flags = ["/std:c++20", "/EHsc", runtime, f"/Fe{binary.as_posix()}"]
        else:
            flags = ["-std=c++20", "-o", binary.as_posix()]
        if self.settings.os == "Macos":
            flags.extend(
                (
                    "-isysroot",
                    XCRun(self, sdk="macosx").sdk_path,
                    f"-mmacosx-version-min={self.settings.os.version}",
                )
            )
        self.run(command_line([compilers["cpp"], *flags, source.as_posix()]))
        if can_run(self):
            self.run(command_line([binary.as_posix()]))
