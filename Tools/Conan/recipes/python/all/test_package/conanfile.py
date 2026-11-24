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
            CMakeToolchain(self).generate()

    def build(self):
        if self.settings.os == "Android":
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def test(self):
        python = self.dependencies["python"]
        original = Path(python.package_folder)
        windows = self.settings.os == "Windows"
        apple = self.settings.os == "Macos"
        if self.settings.os == "iOS":
            framework = original / "Python.framework"
            assert (framework / "Headers/Python.h").is_file()
            assert (framework / "Python").is_file()
            assert (original / "lib/python/encodings/__init__.py").is_file()
            assert not (original / "include").exists()
            assert not list((original / "lib").glob("libpython*.dylib"))
            return
        if self.settings.os in ("Linux", "Android"):
            assert (original / "lib/libpython.so").is_file()
            assert not (original / "lib/libpython.so").is_symlink()
            assert (original / "include/python/Python.h").is_file()
        if self.settings.os == "Android":
            stdlib = original / "lib/python"
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
            assert (original / "lib/libpython.a").is_file()
            bits = 64 if self.settings.arch == "wasm64" else 32
            self.run(
                shlex.join(
                    [
                        os.environ.get("EMSDK_NODE", "node"),
                        str(Path(self.source_folder) / "test_wasm.mjs"),
                        str(original),
                        str(bits),
                        str(python.ref.version),
                    ]
                ),
                scope="run",
            )
            return
        if windows:
            abi_version = str(python.ref.version).rsplit(".", 1)[0].replace(".", "")
            for suffix in ("", "_d"):
                for path in (
                    original / f"python{suffix}.exe",
                    original / f"python{abi_version}{suffix}.dll",
                    original / f"python{abi_version}{suffix}.pdb",
                    original / f"libs/python{abi_version}{suffix}.lib",
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
            interpreter = sdk / "bin/python3"
            if windows:
                interpreter = sdk / "python.exe"
                run(
                    sdk / "python_d.exe",
                    "-IB",
                    "-c",
                    "import _ssl, sys; assert hasattr(sys, 'gettotalrefcount')",
                )
            elif apple:
                framework = sdk / "Python.framework"
                assert not (framework / "Versions").exists()
                assert not (framework / "include").exists()
                assert not any(path.is_symlink() for path in framework.rglob("*"))
                assert not list((framework / "lib").glob("python3.*"))
                assert not list((framework / "lib").glob("libpython*.dylib"))
                interpreter = framework / "bin/python3"
                assert not interpreter.is_symlink()
                assert not list(interpreter.parent.glob("python3.[0-9]*"))
                run(
                    interpreter,
                    "-IB",
                    "-c",
                    "import _curses, _curses_panel, sys, sysconfig, tkinter;"
                    "from pathlib import Path;"
                    "root = Path(sys.base_prefix);"
                    "assert Path(sysconfig.get_path('stdlib')) == root / 'lib/python';"
                    "assert Path(sysconfig.get_path('include')) == root / 'Headers';"
                    "assert Path(sysconfig.get_makefile_filename()).is_file();"
                    "tkinter.Tcl().eval('info patchlevel')",
                )
                run(interpreter.with_name("pydoc3"), "-w", "sys", cwd=root)
            run(
                interpreter,
                "-IB",
                "-c",
                "import bz2, compression.zstd, ctypes, lzma, sqlite3, ssl, sys, zlib;"
                "from pathlib import Path;"
                "assert sys.version.split()[0] == sys.argv[2];"
                "assert Path(sys.base_prefix).resolve().is_relative_to(Path(sys.argv[1]).resolve())",
                sdk,
                python.ref.version,
            )
            venv = root / "venv"
            run(interpreter, "-IB", "-m", "venv", venv)
            entry = venv / ("Scripts/python.exe" if windows else "bin/python")
            run(entry, "-IB", "-m", "pip", "--version")
            if not windows:
                assert (venv / "lib/python/site-packages/pip").is_dir()
                assert not list((venv / "bin").glob("python3.*"))
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
