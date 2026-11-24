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
from shutil import which
from subprocess import list2cmdline
from tempfile import TemporaryDirectory

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import cross_building
from conan.tools.cmake import CMakeToolchain, cmake_layout
from conan.tools.env import Environment
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    download,
    export_conandata_patches,
    get,
    load,
    rename,
    rm,
    rmdir,
    save,
)
from conan.tools.microsoft import VCVars

MODULES = "Core,Gui,Widgets,Network,Concurrent,OpenGL,OpenGLWidgets,Svg,SvgWidgets,Test,UiTools,Xml"


class Recipe(ConanFile):
    name = "pyside6"
    license = "LGPL-3.0-only"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "shared-library"

    def export_sources(self):
        for pattern in ("Findpyside6.cmake", "__init__.py", "setup.py"):
            copy(self, pattern, self.recipe_folder, self.export_sources_folder)
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def package_id(self):
        # The Windows distribution contains both Python ABIs.
        if self.info.settings.os == "Windows":
            self.info.settings.rm_safe("build_type")
            self.info.settings.rm_safe("compiler.runtime_type")

    def requirements(self):
        self.requires("qt/6.12.0")
        self.requires("python/3.15.2")

    def build_requirements(self):
        if cross_building(self):
            self.tool_requires(f"{self.name}/{self.version}")
            self.tool_requires("qt/6.12.0")
            self.tool_requires("python/3.15.2")

    def generate(self):
        if cross_building(self):
            tc = CMakeToolchain(self)
            # setup.py owns the staging prefix for its separate CMake projects.
            tc.blocks.remove("output_dirs")
            host_qt = Path(self.dependencies.build["qt"].package_folder)
            host_pyside = Path(self.dependencies.build[self.name].package_folder)
            windows = self.settings.os == "Windows"
            runtime_dir = "bin" if windows else "lib"
            if windows:
                python = self.dependencies.host["python"]
                tc.blocks.remove("vs_runtime")
                tc.variables["CMAKE_SYSTEM_NAME"] = "Windows"
                tc.variables["CMAKE_SYSTEM_PROCESSOR"] = "ARM64"
                tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = (
                    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
                )
                tc.variables["Python_FIND_REGISTRY"] = "NEVER"
                tc.variables["PYTHON_LIMITED_LIBRARIES"] = (
                    Path(python.package_folder)
                    / python.cpp_info.libdirs[0]
                    / "python3.lib"
                ).as_posix()
            tc.variables["QT_HOST_PATH"] = host_qt.as_posix()
            tc.variables["SHIBOKEN_WRAPPER_HOST_QT_LIB_PATH"] = (
                host_qt / runtime_dir
            ).as_posix()
            tc.variables["SHIBOKEN_WRAPPER_HOST_CLANG_LIB_PATH"] = (
                host_pyside / runtime_dir
            ).as_posix()
            tc.generate()
        elif self.settings.os == "Windows":
            VCVars(self).generate()

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def build(self):
        build = Path(self.build_folder)
        source = Path(self.source_folder)
        windows = self.settings.os == "Windows"
        command_line = list2cmdline if windows else shlex.join
        cross = cross_building(self)
        python_sdk = self.dependencies.host["python"]
        build_python = self.dependencies.build["python"] if cross else python_sdk
        python = (
            Path(build_python.package_folder)
            / build_python.cpp_info.bindirs[0]
            / ("python.exe" if windows else "python3")
        )
        clang = build / "libclang"
        archive = build / "clang.7z"
        if not clang.is_dir():
            sources = self.conan_data["libclang"][self.version]
            asset = sources[str(self.settings.os)][str(self.settings.arch)]
            download(self, **asset, filename=str(archive))
            seven_zip = which("7z") or which("7zz")
            if not seven_zip and windows:
                seven_zip = "C:/Program Files/7-Zip/7z.exe"
            if not seven_zip:
                raise ConanException("PySide requires 7-Zip to extract libclang")
            aliases = ()
            if self.settings.os == "Linux":
                # 7-Zip rejects these links because clang itself links to clang-20.
                aliases = ("clang++", "clang-cl", "clang-cpp")
            self.run(
                command_line(
                    [
                        seven_zip,
                        "x",
                        "-y",
                        f"-o{build}",
                        str(archive),
                        *(f"-x!libclang/bin/{alias}" for alias in aliases),
                    ]
                )
            )
            for alias in aliases:
                (clang / "bin" / alias).symlink_to("clang")
            rm(self, archive.name, archive.parent)

        qt = Path(self.dependencies["qt"].package_folder)
        host_qt = qt
        if cross:
            host_qt = Path(self.dependencies.build["qt"].package_folder)
        qtpaths = host_qt / "bin" / ("qtpaths6.exe" if windows else "qtpaths6")
        venv = build / "pyside"
        venv_python = venv / ("Scripts/python.exe" if windows else "bin/python")
        self.run(command_line([str(python), "-m", "venv", str(venv)]))
        self.run(
            command_line(
                [
                    str(venv_python),
                    "-m",
                    "pip",
                    "install",
                    "-r",
                    str(source / "requirements.txt"),
                ]
            )
        )
        env = Environment()
        env.define_path("LLVM_INSTALL_DIR", str(clang))
        compiler_bin = clang / "bin"
        if self.settings.os == "Macos":
            # Shiboken probes the compiler's include paths while parsing with libclang.
            compiler = StringIO()
            self.run(
                "/usr/bin/xcrun --sdk macosx --find clang", stdout=compiler, quiet=True
            )
            compiler_bin = Path(compiler.getvalue().strip()).parent
            sdk = StringIO()
            self.run(
                "/usr/bin/xcrun --sdk macosx --show-sdk-path", stdout=sdk, quiet=True
            )
            env.define_path("SDKROOT", sdk.getvalue().strip())
            env.define("MACOSX_DEPLOYMENT_TARGET", str(self.settings.os.version))
        env.prepend_path("PATH", str(host_qt / "bin"))
        if not cross:
            env.prepend_path("PATH", str(compiler_bin))
        args = [
            "setup.py",
            "build",
            f"--qtpaths={qtpaths}",
            "--ignore-git",
            "--build-type=all",
            "--no-strip",
            "--skip-docs",
            "--skip-packaging",
            "--skip-mypy-test",
            f"--module-subset={MODULES}",
            "--parallel=8",
        ]
        if cross:
            python_target = (
                Path(python_sdk.package_folder) / python_sdk.cpp_info.bindirs[0]
            )
            if windows:
                env.prepend_path(
                    "LIB",
                    str(
                        Path(python_sdk.package_folder) / python_sdk.cpp_info.libdirs[0]
                    ),
                )
            else:
                python_target = python_target.parent
            shiboken = Path(self.dependencies.build[self.name].package_folder)
            args.extend(
                (
                    f"--cmake-toolchain-file={Path(self.generators_folder) / 'conan_toolchain.cmake'}",
                    f"--qt-host-path={host_qt}",
                    f"--qt-target-path={qt}",
                    f"--python-target-path={python_target}",
                    f"--shiboken-host-path={shiboken}",
                    "--plat-name=win_arm64" if windows else "--plat-name=linux_aarch64",
                )
            )
        env.vars(self, scope=None).save_script("pyside-build")
        self.run(
            command_line(
                [str(venv_python), *args, "--relwithdebinfo", "--limited-api=yes"]
            ),
            cwd=source,
            env=["conanbuild", "pyside-build"],
        )
        if windows:
            env.define_path("PYTHONPATH", str(venv / "Lib/site-packages"))
            env.vars(self, scope=None).save_script("pyside-build-debug")
            self.run(
                command_line(
                    [
                        str(python.with_name("python_d.exe")),
                        *args,
                        "--debug",
                        "--limited-api=no",
                        "--disable-pyi",
                    ]
                ),
                cwd=source,
                env=["conanbuild", "pyside-build-debug"],
            )

    def package(self):
        source = Path(self.source_folder)
        sdk = Path(self.package_folder)
        assets = source.parent
        clang = Path(self.build_folder) / "libclang"
        windows = self.settings.os == "Windows"
        site_packages = Path(
            "lib/site-packages" if windows else "lib/python3.15/site-packages"
        )
        (release,) = (
            folder
            for folder in (source / "build").glob("*/install")
            if (folder / site_packages / "PySide6/__init__.py").is_file()
            and not (folder / "bin/pyside6_d.dll").is_file()
        )
        copy(
            self, "*", release, sdk, excludes=("__pycache__", "*/__pycache__", "*.pyc")
        )
        if not copy(self, "README.md", source, sdk):
            raise ConanException("Missing the PySide README")
        if not copy(self, "Findpyside6.cmake", assets, sdk):
            raise ConanException("Missing Findpyside6.cmake")
        for name in ("__init__.py", "setup.py"):
            if not copy(self, name, assets, sdk / site_packages):
                raise ConanException(f"Missing {name}")
        copy(self, "*.txt", source / "LICENSES", sdk / "licenses")
        save(
            self,
            sdk / "licenses/libclang.txt",
            load(self, clang / "include/llvm/Support/LICENSE.TXT"),
        )

        if windows:
            (debug,) = (
                folder
                for folder in (source / "build").glob("*/install")
                if (folder / "bin/pyside6_d.dll").is_file()
            )
            if not copy(self, "libclang.dll", clang / "bin", sdk / "bin"):
                raise ConanException("Missing libclang.dll")
            for name, module in (("pyside6", "PySide6"), ("shiboken6", "shiboken6")):
                for suffix, installation in ((".abi3", release), ("_d", debug)):
                    stem = name + suffix
                    for extension in (".dll", ".pdb"):
                        filename = stem + extension
                        if not copy(self, filename, installation / "bin", sdk / "bin"):
                            raise ConanException(f"Missing {filename}")
                        copy(
                            self,
                            filename,
                            installation / "bin",
                            sdk / site_packages / module,
                        )
                    if not copy(self, stem + ".lib", installation / "lib", sdk / "lib"):
                        raise ConanException(f"Missing {stem}.lib")
                for pattern in ("*_d.pyd", "*.pdb"):
                    copy(
                        self,
                        pattern,
                        debug / site_packages / module,
                        sdk / site_packages / module,
                    )
            return
        linux = self.settings.os == "Linux"
        if linux:
            clang_library = clang / "lib/libclang.so.20.1.3"
            clang_name = "libclang.so.20.1"
        else:
            clang_library = (clang / "lib/libclang.dylib").resolve()
            clang_name = "libclang.dylib"
        with TemporaryDirectory(dir=self.build_folder) as staging:
            (library,) = copy(self, clang_library.name, clang_library.parent, staging)
            rm(self, clang_name, sdk / "lib")
            rename(self, library, sdk / "lib" / clang_name)

        if linux:
            patchelf = Path(self.build_folder) / "pyside/bin/patchelf"
            if (sdk / "bin/shiboken6").exists():
                self.run(
                    shlex.join(
                        [
                            str(patchelf),
                            "--set-rpath",
                            "$ORIGIN:$ORIGIN/../lib",
                            str(sdk / "bin/shiboken6"),
                        ]
                    )
                )
            rpath = [str(patchelf), "--set-rpath", "$ORIGIN:$ORIGIN/../../.."]
        else:
            for tool in (sdk / "bin/shiboken6", sdk / "shiboken6_generator/shiboken6"):
                self.run(
                    shlex.join(
                        [
                            "/usr/bin/install_name_tool",
                            "-add_rpath",
                            "@loader_path/../lib",
                            str(tool),
                        ]
                    )
                )
            rpath = [
                "/usr/bin/install_name_tool",
                "-add_rpath",
                "@loader_path/../../..",
            ]
        for module in ("PySide6", "shiboken6"):
            for library in (sdk / site_packages / module).glob("*.so"):
                self.run(shlex.join([*rpath, str(library)]))
        if linux and self.conf.get("user.package:prune_build"):
            rmdir(self, clang)
