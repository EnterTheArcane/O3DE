#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path
from typing import ClassVar

from conan import ConanFile
from conan.errors import ConanException, ConanInvalidConfiguration
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
)


class Recipe(ConanFile):
    name = "directxshadercompiler"
    license = "NCSA"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "application"
    options: ClassVar[dict[str, list[bool]]] = {"tablegen_only": [True, False]}
    default_options: ClassVar[dict[str, bool]] = {"tablegen_only": False}

    def layout(self):
        cmake_layout(self, src_folder="src")

    def export_sources(self):
        export_conandata_patches(self)

    def build_requirements(self):
        if not self.options.tablegen_only and cross_building(self):
            self.tool_requires(
                f"{self.name}/{self.version}", options={"tablegen_only": True}
            )

    def validate(self):
        if self.options.tablegen_only and cross_building(self):
            raise ConanInvalidConfiguration(
                "DXC TableGen tools must run on the build machine"
            )

    def source(self):
        # The fork retains the SPIR-V and specialization-constant extensions used by Atom.
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        external = Path(self.source_folder) / "external"
        # Later DirectX-Headers WSL stubs redeclare types provided by DXC's WinAdapter.h.
        for name, archive in self.conan_data["extra_sources"][self.version].items():
            get(
                self,
                **archive,
                destination=(external / name).as_posix(),
                strip_root=True,
            )
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.blocks.remove("cppstd")
        tc.variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        tc.variables["ENABLE_SPIRV_CODEGEN"] = True
        tc.variables["HLSL_DISABLE_SOURCE_GENERATION"] = True
        tc.variables["HLSL_ENABLE_FIXED_VER"] = True
        tc.variables["HLSL_INCLUDE_TESTS"] = False
        tc.variables["LLVM_DEFAULT_TARGET_TRIPLE"] = "dxil-ms-dx"
        tc.variables["LLVM_ENABLE_EH"] = True
        tc.variables["LLVM_ENABLE_RTTI"] = True
        tc.variables["LLVM_ENABLE_TERMINFO"] = False
        tc.variables["LLVM_INCLUDE_TESTS"] = False
        tc.variables["LLVM_TARGETS_TO_BUILD"] = "None"
        tc.variables["CLANG_CL"] = False
        tc.variables["CLANG_ENABLE_ARCMT"] = False
        tc.variables["CLANG_ENABLE_STATIC_ANALYZER"] = False
        tc.variables["CLANG_INCLUDE_TESTS"] = False
        tc.variables["LIBCLANG_BUILD_STATIC"] = True
        tc.variables["SPIRV_SKIP_TESTS"] = True
        if self.settings.os == "Windows":
            tc.variables["SPIRV_WERROR"] = False
            if self.settings.compiler == "clang":
                # DXC's legacy -Wall enables all diagnostics with clang-cl.
                tc.variables["LLVM_ENABLE_WARNINGS"] = False
        if cross_building(self):
            tools = Path(self.dependencies.build[self.name].package_folder) / "bin"
            suffix = ".exe" if self.settings_build.os == "Windows" else ""
            tc.variables["CMAKE_SYSTEM_NAME"] = str(self.settings.os)
            tc.variables["LLVM_TABLEGEN"] = (tools / f"llvm-tblgen{suffix}").as_posix()
            tc.variables["CLANG_TABLEGEN"] = (
                tools / f"clang-tblgen{suffix}"
            ).as_posix()
        if self.settings.os == "Macos":
            tc.variables["CMAKE_BUILD_WITH_INSTALL_RPATH"] = True
            tc.variables["CMAKE_INSTALL_RPATH"] = "@loader_path/../lib"
        elif self.settings.os == "Linux":
            tc.variables["CMAKE_BUILD_WITH_INSTALL_RPATH"] = True
            tc.variables["CMAKE_INSTALL_RPATH"] = "$ORIGIN/../lib"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        if self.options.tablegen_only:
            cmake.build(target="llvm-tblgen")
            cmake.build(target="clang-tblgen")
            return
        cmake.build(target="dxc")
        cmake.build(target="dxsc")
        cmake.build(target="dxcompiler")
        if self.settings.os == "Windows":
            cmake.build(target="dxildll")

    def package(self):
        build = Path(self.build_folder)
        package = Path(self.package_folder)
        if self.options.tablegen_only:
            suffix = ".exe" if self.settings.os == "Windows" else ""
            for tool in ("llvm-tblgen", "clang-tblgen"):
                if not copy(self, f"{tool}{suffix}", build / "bin", package / "bin"):
                    raise ConanException(f"Missing DXC build tool {tool}{suffix}")
        elif self.settings.os == "Windows":
            for name in ("dxc.exe", "dxsc.exe", "dxcompiler.dll", "dxil.dll"):
                copy(self, name, build / "bin", package / "bin")
                copy(self, f"{Path(name).stem}.pdb", build / "bin", package / "bin")
        else:
            for name in ("dxc", "dxc-3.7", "dxsc", "dxsc-3.7"):
                copy(self, name, build / "bin", package / "bin")
            copy(self, "libdxcompiler.*", build / "lib", package / "lib")
        copy(
            self,
            "LICENSE*",
            self.source_folder,
            package / "licenses/dxc",
            excludes="*/*",
        )
        for dependency in ("DirectX-Headers", "SPIRV-Headers", "SPIRV-Tools"):
            if dependency == "DirectX-Headers" and self.settings.os == "Windows":
                continue
            copy(
                self,
                "LICENSE*",
                Path(self.source_folder) / "external" / dependency,
                package / "licenses" / dependency.lower(),
                excludes="*/*",
            )
