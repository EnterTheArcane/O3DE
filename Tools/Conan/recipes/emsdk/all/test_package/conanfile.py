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

from conan import ConanFile
from conan.tools.cmake import cmake_layout
from conan.tools.env import VirtualBuildEnv
from conan.tools.files import save


class TestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def build_requirements(self):
        self.tool_requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def test(self):
        env = VirtualBuildEnv(self).vars()
        output = StringIO()
        self.run(shlex.join([env.get("CC"), "--version"]), stdout=output)
        version = self.dependencies.build["emsdk"].ref.version
        assert str(version) in output.getvalue().splitlines()[0]

        wasm64 = self.settings.arch == "wasm64"
        source = Path(self.build_folder) / "main.cpp"
        binary = source.with_suffix(".js")
        save(
            self,
            source,
            f"""#include <cstdio>
static_assert(sizeof(void*) == {8 if wasm64 else 4});
int main() {{ return std::puts("Emscripten SDK") < 0; }}
""",
        )
        flags = ["-m64"] if wasm64 else []
        self.run(
            shlex.join(
                [env.get("CXX"), *flags, "-std=c++20", str(source), "-o", str(binary)]
            )
        )
        self.run(shlex.join([env.get("EMSDK_NODE"), str(binary)]))
