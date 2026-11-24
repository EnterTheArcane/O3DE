#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import get

CONFIGURATIONS = ("Debug", "Checked", "Profile", "Release")
CUDA_TOOLKIT = "cuda-toolkit/13.5.0"


class Recipe(ConanFile):
    name = "nvcloth"
    license = "Nvidia Source Code License (1-Way Commercial)"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    options = {"with_cuda": [True, False]}
    default_options = {"with_cuda": True}

    def config_options(self):
        if self.settings.os not in ("Linux", "Windows") or self.settings.arch not in (
            "x86_64",
            "armv8",
        ):
            self.options.rm_safe("with_cuda")

    def validate(self):
        if self.settings.os not in (
            "Android",
            "Emscripten",
            "Linux",
            "Macos",
            "Windows",
            "iOS",
        ):
            raise ConanInvalidConfiguration(
                f"NvCloth does not support {self.settings.os}"
            )

    def requirements(self):
        self.requires("physx/5.13.0", transitive_headers=True, transitive_libs=True)
        if self.options.get_safe("with_cuda"):
            self.requires(
                CUDA_TOOLKIT,
                headers=True,
                libs=True,
                transitive_headers=True,
                transitive_libs=True,
                options={"with_compiler": False},
            )

    def build_requirements(self):
        if self.options.get_safe("with_cuda"):
            self.tool_requires(CUDA_TOOLKIT)

    def package_id(self):
        self.info.settings.rm_safe("build_type")
        self.info.settings.rm_safe("compiler.runtime_type")

    def layout(self):
        self.folders.source = "src"
        self.folders.build = "build"

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja Multi-Config"
        tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = ";".join(CONFIGURATIONS)
        tc.cache_variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.cache_variables["BUILD_SHARED_LIBS"] = False
        physx = self.dependencies.host["physx"]
        tc.cache_variables["PhysX_DIR"] = Path(physx.package_folder).as_posix()
        if self.settings.arch == "wasm64":
            # Emscripten reads CMAKE_C_FLAGS before checking the PhysX package's pointer size.
            tc.cache_variables["CMAKE_C_FLAGS"] = "-m64"
            tc.cache_variables["CMAKE_CXX_FLAGS"] = "-m64"
        with_cuda = bool(self.options.get_safe("with_cuda"))
        tc.cache_variables["NV_CLOTH_ENABLE_CUDA"] = with_cuda
        if with_cuda:
            toolkit = self.dependencies.build["cuda-toolkit"]
            tc.cache_variables["CUDAToolkit_ROOT"] = Path(
                toolkit.package_folder
            ).as_posix()
        if self.settings.os == "Windows":
            tc.blocks.remove("vs_runtime")
            runtime = "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            if self.settings.compiler.runtime == "dynamic":
                runtime += "DLL"
            tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = runtime
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        for config in CONFIGURATIONS:
            cmake.build(build_type=config)

    def package(self):
        cmake = CMake(self)
        for config in CONFIGURATIONS:
            cmake.install(build_type=config)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "NvCloth")
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.set_property("cmake_target_name", "NvCloth::NvCloth")
        self.cpp_info.builddirs = ["lib/cmake/NvCloth"]
        config = {"Debug": "debug", "RelWithDebInfo": "profile"}.get(
            str(self.settings.build_type), "release"
        )
        self.cpp_info.libdirs = [f"lib/{config}"]
        self.cpp_info.libs = ["NvCloth"]
        self.cpp_info.defines = ["NV_CLOTH_STATIC"]
