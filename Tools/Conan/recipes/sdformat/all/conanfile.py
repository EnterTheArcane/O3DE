#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
    load,
    save,
)


class Recipe(ConanFile):
    name = "sdformat"
    license = "Apache-2.0 AND BSD-3-Clause"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False]}
    default_options = {"shared": True, "tinyxml2/*:shared": False}

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        export_conandata_patches(self)

    def requirements(self):
        # Upstream CMake exports call the Gazebo package macros at configure time.
        self.requires("gz-cmake/5.2.0", visible=True)
        self.requires("gz-math/9.3.0", transitive_headers=True, transitive_libs=True)
        self.requires("gz-utils/4.0.0", transitive_headers=True, transitive_libs=True)
        self.requires("tinyxml2/11.0.0")
        if self.settings.os == "Windows":
            self.requires("dlfcn-win32/1.4.2")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.variables["BUILD_TESTING"] = False
        tc.variables["CMAKE_FIND_PACKAGE_PREFER_CONFIG"] = True
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.variables["GZ_ENABLE_RELOCATABLE_INSTALL"] = bool(self.options.shared)
        tc.variables["SKIP_PYBIND11"] = True
        tc.variables["USE_INTERNAL_URDF"] = True
        if self.settings.os == "Linux":
            tc.variables["CMAKE_INSTALL_RPATH"] = "$ORIGIN"
        elif self.settings.os == "Macos":
            tc.variables["CMAKE_INSTALL_RPATH"] = "@loader_path"
        tc.generate()

        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        licenses = Path(self.package_folder) / "licenses"
        copy(self, "LICENSE", self.source_folder, licenses)
        for file in ("urdf_parser/model.cpp", "urdf_model/utils.h"):
            notice = Path(self.source_folder) / "src/urdf" / file
            license_text = load(self, notice).split("*/", 1)[0]
            save(self, licenses / f"urdf-{notice.stem}.txt", f"{license_text}*/\n")
        for name in ("tinyxml2", "dlfcn-win32"):
            if name in self.dependencies.host:
                dependency = Path(self.dependencies.host[name].package_folder)
                copy(self, "*", dependency / "licenses", licenses / name)

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [""]
        self.cpp_info.includedirs = ["include/gz/sdformat16"]
        self.cpp_info.libs = ["sdformat"]
