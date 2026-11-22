#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import os
from pathlib import Path
import shlex
from subprocess import list2cmdline
from tempfile import TemporaryDirectory

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, rename


class TestPython(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        if self.settings.os == "Android":
            tc = CMakeToolchain(self)
            tc.variables["Python_DIR"] = self.dependencies["python"].package_folder
            tc.generate()

    def build(self):
        if self.settings.os == "Android":
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def test(self):
        original = Path(self.dependencies["python"].package_folder) / "python"
        windows = self.settings.os == "Windows"
        apple = self.settings.os == "Macos"
        if self.settings.os == "Android":
            stdlib = original / "lib/python3.15"
            assert (stdlib / "encodings/__init__.py").is_file()
            for module in (
                "_ssl",
                "_bz2",
                "_ctypes",
                "_lzma",
                "_sqlite3",
                "_zstd",
                "zlib",
            ):
                assert list((stdlib / "lib-dynload").glob(f"{module}.*.so")), module
        if self.settings.os == "Emscripten":
            bits = 64 if self.settings.arch == "wasm64" else 32
            self.run(
                shlex.join(
                    [
                        os.environ.get("EMSDK_NODE", "node"),
                        str(Path(self.source_folder) / "test_wasm.mjs"),
                        str(original),
                        str(bits),
                    ]
                ),
                scope="run",
            )
            return
        if windows:
            for suffix in ("", "_d"):
                for path in (
                    original / f"python{suffix}.exe",
                    original / f"python315{suffix}.dll",
                    original / f"python315{suffix}.pdb",
                    original / f"libs/python315{suffix}.lib",
                ):
                    assert path.is_file(), path
        if not can_run(self):
            return
        command_line = list2cmdline if windows else shlex.join

        def run(*args, cwd=None):
            # The original package's runtime search paths would mask relocation errors.
            self.run(command_line(map(str, args)), cwd=cwd, env=None)

        with TemporaryDirectory(prefix="o3de relocated python ") as directory:
            root = Path(directory)
            sdk = root / "sdk"
            if not copy(self, "*", original, sdk):
                raise ConanException("Missing the CPython installation")
            interpreter = sdk / "bin/python"
            if windows:
                interpreter = sdk / "python.exe"
                run(
                    sdk / "python_d.exe",
                    "-IB",
                    "-c",
                    "import _ssl, sys; assert hasattr(sys, 'gettotalrefcount')",
                )
            elif apple:
                interpreter = sdk / "Python.framework/Versions/Current/bin/python3"
                run(
                    interpreter,
                    "-IB",
                    "-c",
                    "import tkinter; tkinter.Tcl().eval('info patchlevel')",
                )
                run(interpreter.with_name("pydoc3.15"), "-w", "sys", cwd=root)
            run(
                interpreter,
                "-IB",
                "-c",
                "import bz2, compression.zstd, ctypes, lzma, sqlite3, ssl, sys, zlib;"
                "from pathlib import Path;"
                "assert sys.version_info[:3] == (3, 15, 2);"
                "assert Path(sys.base_prefix).resolve().is_relative_to(Path(sys.argv[1]).resolve())",
                sdk,
            )
            venv = root / "venv"
            run(interpreter, "-IB", "-m", "venv", venv)
            entry = venv / ("Scripts/python.exe" if windows else "bin/python")
            run(entry, "-IB", "-m", "pip", "--version")
            if windows:
                (launcher,) = copy(
                    self,
                    "venvlauncher_d.exe",
                    sdk / "Lib/venv/scripts/nt",
                    entry.parent,
                )
                debug_entry = entry.with_name("python_d.exe")
                rename(self, launcher, debug_entry)
                run(
                    debug_entry,
                    "-IB",
                    "-c",
                    "import sys; assert hasattr(sys, 'gettotalrefcount')",
                )
