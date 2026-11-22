#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import copy, get


CONFIGURATIONS = ("debug", "checked", "profile", "release")


class Recipe(ConanFile):
    name = "physx"
    license = "Apache-2.0"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        copy(self, "physx-config.cmake", self.recipe_folder, self.export_sources_folder)

    def package_id(self):
        # Every package includes all four configurations, including the MSVC Debug CRT.
        self.info.settings.rm_safe("build_type")
        self.info.settings.rm_safe("compiler.runtime_type")

    def layout(self):
        self.folders.source = "src"
        self.folders.build = "build"

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        os_name = str(self.settings.os)
        self.conf.define("tools.cmake.cmaketoolchain:generator", "Ninja Multi-Config")
        tc = CMakeToolchain(self)
        tc.blocks.remove("cppstd")
        tc.cache_variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        tc.cache_variables["PX_GENERATE_GPU_PROJECTS"] = False
        tc.cache_variables["PX_GENERATE_STATIC_LIBRARIES"] = True
        tc.cache_variables["PX_BUILDPVDRUNTIME"] = os_name in (
            "Windows",
            "Linux",
        )
        tc.cache_variables["TARGET_BUILD_PLATFORM"] = {
            "Windows": "windows",
            "Linux": "linux",
            "Macos": "mac",
            "Android": "android",
            "iOS": "ios",
            "Emscripten": "linux",
        }[os_name]
        if os_name == "Emscripten":
            wasm64 = self.settings.arch == "wasm64"
            tc.cache_variables["PHYSX_EMSCRIPTEN_ARCH"] = (
                "wasm64" if wasm64 else "wasm32"
            )
            if wasm64:
                # The SDK reads the C flags before CMake probes pointer size.
                tc.cache_variables["CMAKE_C_FLAGS"] = "-m64"
                tc.cache_variables["CMAKE_CXX_FLAGS"] = "-m64"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=Path(self.source_folder) / "physx")
        for config in CONFIGURATIONS:
            cmake.build(build_type=config)

    def package(self):
        cmake = CMake(self)
        package = Path(self.package_folder)
        for config in CONFIGURATIONS:
            cmake.install(
                build_type=config,
                cli_args=["--prefix", f'"{(package / config).as_posix()}"'],
            )
        if not copy(self, "physx-config.cmake", self.export_sources_folder, package):
            raise ConanException("Missing physx-config.cmake")
        copy(
            self,
            "PhysXConfigVersion.cmake",
            package / "release/lib/cmake/PhysX",
            package,
        )
        copy(
            self, "LICENSE.md", Path(self.source_folder) / "physx", package / "licenses"
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = ["."]
        self.cpp_info.includedirs = ["release/include"]
