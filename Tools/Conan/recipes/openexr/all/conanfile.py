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
    replace_in_file,
    rmdir,
)


class Recipe(ConanFile):
    name = "openexr"
    license = "BSD-3-Clause"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self.requires("imath/3.2.3", transitive_headers=True)
        self.requires("libdeflate/1.26")
        self.requires("openjph/0.33.0")
        self.requires("zlib/1.4")
        self.requires("zstd/1.5.7")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        # OpenEXR builds the website snippets independently of BUILD_WEBSITE.
        replace_in_file(self, "CMakeLists.txt", "add_subdirectory(website/src)", "")
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_WEBSITE"] = False
        tc.variables["CMAKE_FIND_PACKAGE_PREFER_CONFIG"] = True
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        tc.variables["OPENEXR_BUILD_EXAMPLES"] = False
        if self.settings.os in ("Android", "iOS", "Emscripten"):
            tc.variables["OPENEXR_BUILD_TOOLS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.set_property("openjph", "cmake_target_name", "openjph")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        package = Path(self.package_folder)
        copy(self, "LICENSE.md", self.source_folder, package / "licenses")
        rmdir(self, package / "share")
        rmdir(self, package / "lib/cmake")
        rmdir(self, package / "lib/pkgconfig")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenEXR")
        for name in ("OpenEXRConfig", "IexConfig", "IlmThreadConfig"):
            config = self.cpp_info.components[f"openexr_{name.lower()}"]
            config.set_property("cmake_target_name", f"OpenEXR::{name}")
            config.includedirs = ["include", "include/OpenEXR"]

        suffix = "-3_5_d" if self.settings.build_type == "Debug" else "-3_5"
        for name, requires in {
            "Iex": ["openexr_iexconfig"],
            "IlmThread": ["openexr_ilmthreadconfig", "openexr_iex"],
            "OpenEXRCore": [
                "openexr_openexrconfig",
                "libdeflate::libdeflate",
                "openjph::openjph",
                "zlib::zlib",
                "zstd::zstdlib",
            ],
            "OpenEXR": ["openexr_openexrcore", "openexr_ilmthread", "imath::imath"],
            "OpenEXRUtil": ["openexr_openexr"],
        }.items():
            library = self.cpp_info.components[f"openexr_{name.lower()}"]
            library.set_property("cmake_target_name", f"OpenEXR::{name}")
            library.libs = [f"{name}{suffix}"]
            library.requires = requires
            if self.settings.os == "Linux":
                library.system_libs = ["m", "pthread"] if name == "IlmThread" else ["m"]
