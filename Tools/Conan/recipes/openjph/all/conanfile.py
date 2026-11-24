#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""Adapted from ConanCenter Index's OpenJPH recipe (MIT)."""

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
    rmdir,
)


class Recipe(ConanFile):
    name = "openjph"
    description = "High-throughput JPEG 2000 codec"
    homepage = "https://github.com/aous72/OpenJPH"
    license = "BSD-2-Clause"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    implements: ClassVar[list[str]] = ["auto_shared_fpic"]
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_executables": [True, False],
        "with_tiff": [True, False],
        "with_stream_expand_tool": [True, False],
        "disable_simd": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_executables": True,
        "with_tiff": True,
        "with_stream_expand_tool": False,
        "disable_simd": False,
    }

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        if self.options.with_executables and self.options.with_tiff:
            self.requires("libtiff/[>=4.6.0 <5]")

    def validate(self):
        check_min_cppstd(self, 11)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["OJPH_BUILD_EXECUTABLES"] = bool(
            self.options.with_executables
        )
        tc.cache_variables["OJPH_ENABLE_TIFF_SUPPORT"] = bool(self.options.with_tiff)
        tc.cache_variables["OJPH_BUILD_STREAM_EXPAND"] = bool(
            self.options.with_stream_expand_tool
        )
        tc.cache_variables["OJPH_DISABLE_SIMD"] = bool(self.options.disable_simd)
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        package = Path(self.package_folder)
        CMake(self).install()
        copy(self, "LICENSE", self.source_folder, package / "licenses")
        rmdir(self, package / "lib/cmake")
        rmdir(self, package / "lib/pkgconfig")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "openjph")
        self.cpp_info.set_property("cmake_target_name", "openjph::openjph")
        self.cpp_info.set_property("pkg_config_name", "openjph")
        # Upstream's debug postfix varies by compiler ABI.
        self.cpp_info.libs = collect_libs(self)
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.append("pthread")
