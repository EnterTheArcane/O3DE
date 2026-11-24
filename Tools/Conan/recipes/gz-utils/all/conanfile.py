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
from conan.tools.files import copy, get


class Recipe(ConanFile):
    name = "gz-utils"
    license = "Apache-2.0 AND BSD-3-Clause"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {"shared": [True, False]}
    default_options = {"shared": True}

    def requirements(self):
        self.requires("gz-cmake/5.2.0", visible=True)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.variables["BUILD_TESTING"] = False
        tc.variables["CMAKE_DISABLE_FIND_PACKAGE_spdlog"] = True
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.variables["GZ_UTILS_VENDOR_CLI11"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        copy(
            self, "LICENSE", self.source_folder, Path(self.package_folder) / "licenses"
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [""]
        self.cpp_info.includedirs = ["include/gz/utils4"]
        self.cpp_info.libs = ["gz-utils"]
