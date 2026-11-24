#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir


class Recipe(ConanFile):
    name = "imath"
    license = "BSD-3-Clause"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["IMATH_LIB_SUFFIX"] = ""
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        if self.settings.compiler == "msvc":
            tc.extra_cxxflags.append("/Zc:__cplusplus")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        package = Path(self.package_folder)
        copy(self, "LICENSE.md", self.source_folder, package / "licenses")
        rmdir(self, package / "lib/cmake")
        rmdir(self, package / "lib/pkgconfig")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Imath")
        self.cpp_info.set_property("cmake_target_name", "Imath::Imath")
        config = self.cpp_info.components["imath_config"]
        config.set_property("cmake_target_name", "Imath::ImathConfig")
        config.includedirs = ["include", "include/Imath"]
        imath = self.cpp_info.components["imath_lib"]
        imath.set_property("cmake_target_name", "Imath::Imath")
        imath.requires = ["imath_config"]
        imath.libs = ["Imath_d" if self.settings.build_type == "Debug" else "Imath"]
        if self.settings.os == "Linux":
            imath.system_libs = ["m"]
