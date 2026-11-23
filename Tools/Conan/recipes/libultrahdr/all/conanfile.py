#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""Adapted from ConanCenter Index's libultrahdr recipe (MIT)."""

from pathlib import Path
from typing import ClassVar

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    collect_libs,
    copy,
    export_conandata_patches,
    get,
    rm,
    rmdir,
)


class UltraHDR(ConanFile):
    name = "libultrahdr"
    description = "JPEG-based Ultra HDR image encoding and decoding"
    homepage = "https://github.com/google/libultrahdr"
    license = ("MIT", "Apache-2.0")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options: ClassVar[dict[str, list[bool] | list[str]]] = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_jpeg": ["libjpeg", "libjpeg-turbo", "mozjpeg"],
    }
    default_options: ClassVar[dict[str, bool | str]] = {
        "shared": False,
        "fPIC": True,
        "with_jpeg": "libjpeg",
    }

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        if self.options.with_jpeg == "libjpeg":
            self.requires("libjpeg/[>=9e]")
        elif self.options.with_jpeg == "libjpeg-turbo":
            self.requires("libjpeg-turbo/[>=3.0.0 <4]")
        else:
            self.requires("mozjpeg/[>=4.1.3 <5]")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.22 <5]")

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["UHDR_BUILD_DEPS"] = False
        tc.cache_variables["UHDR_BUILD_EXAMPLES"] = False
        tc.cache_variables["CMAKE_REQUIRE_FIND_PACKAGE_JPEG"] = True
        tc.cache_variables["UHDR_ENABLE_HEIF"] = False
        if self.settings.os == "Windows" and not self.options.shared:
            # Keep the CRT selected by the toolchain for static libraries.
            tc.cache_variables["BUILD_FOR_WINUI"] = True
        tc.generate()

        deps = CMakeDeps(self)
        jpeg = str(self.options.with_jpeg)
        deps.set_property(jpeg, "cmake_file_name", "JPEG")
        deps.set_property(jpeg, "cmake_target_name", "JPEG::JPEG")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        package = Path(self.package_folder)
        CMake(self).install()
        copy(self, "LICENSE*", self.source_folder, package / "licenses")
        rmdir(self, package / "lib/pkgconfig")
        if self.options.shared:
            rm(self, "libuhdr.a", package / "lib")
            rm(self, "uhdr-static.lib", package / "lib")

    def package_info(self):
        # MSVC and clang-cl can both produce the uhdr-static library basename.
        self.cpp_info.libs = collect_libs(self)
        if self.options.with_jpeg == "libjpeg-turbo":
            self.cpp_info.requires = ["libjpeg-turbo::jpeg"]
        else:
            self.cpp_info.requires = [f"{self.options.with_jpeg}::libjpeg"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread"]
