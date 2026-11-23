#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""Adapted from ConanCenter Index's libde265 recipe (MIT)."""

from pathlib import Path
from typing import ClassVar

from conan import ConanFile
from conan.tools.build import check_min_cppstd, stdcpp_library
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
    replace_in_file,
    rmdir,
)
from conan.tools.microsoft import is_msvc


class Recipe(ConanFile):
    name = "libde265"
    description = "Open H.265 video codec implementation"
    homepage = "https://github.com/strukturag/libde265"
    license = "LGPL-3.0-or-later"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "simd": [True, False],
        "avx2": [True, False],
        "avx512": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "simd": True,
        "avx2": True,
        "avx512": True,
    }

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
        if self.settings.arch != "x86_64" or is_msvc(self):
            self.options.rm_safe("avx2")
            self.options.rm_safe("avx512")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if not self.options.simd:
            self.options.rm_safe("avx2")
        if not self.options.get_safe("avx2"):
            self.options.rm_safe("avx512")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def validate(self):
        check_min_cppstd(self, 11)

    def validate_build(self):
        check_min_cppstd(self, 17)

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.16]")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        cmakelists = Path(self.source_folder) / "CMakeLists.txt"
        replace_in_file(self, cmakelists, "set(CMAKE_POSITION_INDEPENDENT_CODE ON)", "")
        replace_in_file(self, cmakelists, "set(CMAKE_CXX_STANDARD 17)", "")
        replace_in_file(self, cmakelists, "set(CMAKE_CXX_EXTENSIONS OFF)", "")
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe(
            "fPIC", True
        )
        tc.variables["ENABLE_SDL"] = False
        tc.variables["ENABLE_SIMD"] = bool(self.options.simd)
        tc.variables["ENABLE_AVX2"] = bool(self.options.get_safe("avx2", False))
        tc.variables["ENABLE_AVX512"] = bool(self.options.get_safe("avx512", False))
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        package = Path(self.package_folder)
        copy(self, "COPYING", self.source_folder, package / "licenses")
        CMake(self).install()
        rmdir(self, package / "lib/cmake")
        rmdir(self, package / "lib/pkgconfig")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "libde265")
        self.cpp_info.set_property("cmake_target_name", "de265")
        self.cpp_info.set_property("cmake_target_aliases", ["libde265"])
        self.cpp_info.set_property("pkg_config_name", "libde265")
        prefix = (
            "lib" if self.settings.os == "Windows" and not self.options.shared else ""
        )
        self.cpp_info.libs = [f"{prefix}de265"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["m", "pthread"]
        if not self.options.shared:
            self.cpp_info.defines = ["LIBDE265_STATIC_BUILD"]
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.system_libs.append(libcxx)
