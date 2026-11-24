#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import sys
from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
)
from conan.tools.scm import Version


class Recipe(ConanFile):
    name = "ispc"
    license = "BSD-3-Clause"
    homepage = "https://ispc.github.io/"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "application"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        export_conandata_patches(self)

    def configure(self):
        if self.settings.os == "Windows":
            # The LLVM SDK's libraries use the static Release CRT.
            self.settings.compiler.runtime = "static"
            self.settings.compiler.runtime_type = "Release"
        elif self.settings.os == "Macos":
            # The prebuilt LLVM libraries require macOS 14 or newer.
            deployment = self.settings.get_safe("os.version") or "14.0"
            self.settings.os.version = str(max(Version(deployment), Version("14.0")))

    def requirements(self):
        llvm = "23.1.5" if self.settings.os == "Macos" else "23.1.6"
        self.requires(f"llvm/{llvm}", visible=False)
        # Windows import libraries avoid mixing the SDK's static CRT with /MD objects.
        shared = self.settings.os == "Windows"
        self.requires("zlib/1.4", visible=False, options={"shared": shared})
        self.requires("zstd/1.5.7", visible=False, options={"shared": shared})

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("winflexbison/2.5.25")
        else:
            self.tool_requires("bison/3.8.2")
            self.tool_requires("flex/2.6.4")
        self.tool_requires("m4/1.4.19")
        self.tool_requires("emsdk/7.0.1")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        windows = self.settings.os == "Windows"
        linux = self.settings.os == "Linux"
        mac = self.settings.os == "Macos"
        llvm = Path(self.dependencies.host["llvm"].package_folder)
        sdk = Path(self.dependencies.build["emsdk"].package_folder)
        llvm_config = llvm / "bin/llvm-config"
        emcc = sdk / "upstream/emscripten/emcc"
        if windows:
            llvm_config = llvm_config.with_suffix(".exe")
            parser_bin = (
                Path(self.dependencies.build["winflexbison"].package_folder) / "bin"
            )
            tc.variables["BISON_EXECUTABLE"] = (parser_bin / "win_bison.exe").as_posix()
            tc.variables["FLEX_EXECUTABLE"] = (parser_bin / "win_flex.exe").as_posix()
            python = Path(sys.executable).as_posix()
            tc.variables["EMCC_EXECUTABLE"] = (
                f"{python};{emcc.with_suffix('.py').as_posix()}"
            )
            # ISPC runs its native compiler helper from this directory during the build.
            runtime_dir = Path(self.build_folder) / "bin" / self.cpp.build.bindir
            for name in ("zlib", "zstd"):
                dependency = self.dependencies.host[name]
                for bindir in dependency.cpp_info.bindirs:
                    directory = Path(dependency.package_folder) / bindir
                    copy(self, "*.dll", directory, runtime_dir)
        else:
            tc.variables["EMCC_EXECUTABLE"] = emcc.as_posix()
        if mac:
            tc.cache_variables["CMAKE_C_COMPILER"] = "/usr/bin/clang"
            tc.cache_variables["CMAKE_CXX_COMPILER"] = "/usr/bin/clang++"
            tc.cache_variables["CMAKE_OSX_DEPLOYMENT_TARGET"] = str(
                self.settings.os.version
            )
            # Xcode's LTO reader cannot read the LLVM SDK's archive members.
            lto = llvm / "lib/libLTO.dylib"
            tc.extra_exelinkflags.append(f'-Wl,-lto_library,"{lto.as_posix()}"')
        tc.variables["LLVM_CONFIG_EXECUTABLE"] = llvm_config.as_posix()
        tc.variables["ARM_ENABLED"] = True
        tc.variables["WASM_ENABLED"] = True
        tc.variables["X86_ENABLED"] = True
        tc.variables["ISPC_ANDROID_TARGET"] = linux
        tc.variables["ISPC_CROSS"] = True
        tc.variables["ISPC_FREEBSD_TARGET"] = False
        tc.variables["ISPC_INCLUDE_EXAMPLES"] = False
        tc.variables["ISPC_INCLUDE_RT"] = False
        tc.variables["ISPC_INCLUDE_TESTS"] = False
        tc.variables["ISPC_INCLUDE_UTILS"] = False
        tc.variables["ISPC_IOS_TARGET"] = mac
        tc.variables["ISPC_LIBRARY"] = False
        tc.variables["ISPC_LINUX_TARGET"] = linux
        tc.variables["ISPC_MACOS_TARGET"] = mac
        tc.variables["ISPC_PS_TARGET"] = False
        tc.variables["ISPC_WINDOWS_TARGET"] = windows
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        package = Path(self.package_folder)
        copy(self, "LICENSE*", self.source_folder, package / "licenses")
        if self.settings.os == "Windows":
            runtime_dir = Path(self.build_folder) / "bin" / self.cpp.build.bindir
            copy(self, "*.dll", runtime_dir, package / "bin")
            for name in ("zlib", "zstd"):
                root = Path(self.dependencies.host[name].package_folder)
                copy(self, "*", root / "licenses", package / "licenses" / name)
