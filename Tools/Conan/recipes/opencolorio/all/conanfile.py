#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""Adapted from ConanCenter Index's OpenColorIO recipe (MIT)."""

from pathlib import Path

from conan import ConanFile
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    collect_libs,
    copy,
    export_conandata_patches,
    get,
    rename,
)


class Recipe(ConanFile):
    name = "opencolorio"
    license = "BSD-3-Clause"
    package_type = "shared-library"
    settings = "os", "arch", "compiler", "build_type"

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def configure(self):
        if self.settings.os == "Emscripten":
            self.package_type = "static-library"

    @property
    def _build_tools(self):
        return self.settings.os in ("Linux", "Macos", "Windows") and (
            self.settings.build_type != "Debug"
        )

    def requirements(self):
        self.requires("expat/[>=2.6.0 <3]")
        self.requires("imath/[>=3.2.0 <4]")
        self.requires("lcms/[>=2.17 <3]")
        self.requires("minizip-ng/[>=4.0.5 <5]")
        self.requires("openexr/[>=3.2.0 <4]")
        self.requires("pystring/1.1.4")
        self.requires("yaml-cpp/[>=0.8.0 <1]")
        self.requires("zlib/[>=1.2.13 <2]")
        if self._build_tools:
            self.requires("pybind11/[>=3.0.2 <4]", headers=True, libs=False)
            self.requires("python/3.15.2")
        if self.settings.os == "Macos" and self.settings.arch == "armv8":
            self.requires("sse2neon/1.9.1", headers=True, libs=False)

    def build_requirements(self):
        if self._build_tools and cross_building(self):
            self.tool_requires("python/3.15.2")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        # OCIO sets CACHE properties on this entry even when the toolchain set a normal variable.
        tc.cache_variables["CMAKE_CXX_STANDARD"] = 20
        if self.settings.os in ("Macos", "iOS"):
            # OCIO sets a macOS default before project() loads Conan's toolchain.
            tc.cache_variables["CMAKE_OSX_DEPLOYMENT_TARGET"] = str(
                self.settings.os.version
            )
        tc.variables["BUILD_SHARED_LIBS"] = self.package_type == "shared-library"
        tc.variables["CMAKE_DEBUG_POSTFIX"] = "_d"
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        tc.variables["CMAKE_FIND_PACKAGE_PREFER_CONFIG"] = True
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.variables["minizip-ng_DIR"] = Path(self.generators_folder).as_posix()
        tc.variables["OCIO_INSTALL_EXT_PACKAGES"] = "NONE"
        tc.variables["OCIO_BUILD_APPS"] = self._build_tools
        tc.variables["OCIO_BUILD_PYTHON"] = self._build_tools
        tc.variables["OCIO_BUILD_TESTS"] = False
        tc.variables["OCIO_BUILD_GPU_TESTS"] = False
        tc.variables["OCIO_DIRECTX_ENABLED"] = False
        if self.settings.os in ("iOS", "Emscripten"):
            tc.extra_cxxflags.append("-DOCIO_HEADLESS_ENABLED")
        tc.variables["OCIO_PYTHON_VERSION"] = "3.15"
        tc.variables["Python_FIND_REGISTRY"] = "NEVER"
        if self._build_tools:
            python = self.dependencies.host["python"]
            cross = cross_building(self)
            interpreter = self.dependencies.build["python"] if cross else python
            executable = "python.exe" if self.settings.os == "Windows" else "python3"
            sdk = Path(python.package_folder) / "python"
            headers = sdk / "include/python3.15"
            library = sdk / "lib/libpython3.15.so"
            if self.settings.os == "Windows":
                headers = sdk / "include"
                library = sdk / "libs/python315.lib"
            elif self.settings.os == "Macos":
                framework = sdk / "Python.framework/Versions/Current"
                headers = framework / "Headers"
                library = framework / "Python"
            tc.variables["CMAKE_POLICY_DEFAULT_CMP0190"] = "OLD"
            tc.variables["Python_EXECUTABLE"] = (
                Path(interpreter.package_folder)
                / interpreter.cpp_info.bindirs[0]
                / executable
            ).as_posix()
            tc.variables["Python_INCLUDE_DIR"] = headers.as_posix()
            tc.variables["Python_LIBRARY"] = library.as_posix()
        if self.settings.os == "Linux":
            tc.variables["CMAKE_INSTALL_RPATH"] = "$ORIGIN;$ORIGIN/..;$ORIGIN/../lib"
        elif self.settings.os == "Macos":
            tc.variables["CMAKE_INSTALL_RPATH"] = (
                "@loader_path;@loader_path/..;@loader_path/../lib"
            )
        elif self.settings.os == "Windows":
            tc.extra_cxxflags.append("/bigobj")
            # minizip-ng's Windows crypto backend also needs bcrypt.
            tc.extra_sharedlinkflags.append("bcrypt.lib")
            tc.extra_exelinkflags.append("bcrypt.lib")
        tc.generate()
        deps = CMakeDeps(self)
        deps.set_property("lcms", "cmake_file_name", "lcms2")
        deps.set_property("lcms", "cmake_target_name", "lcms2::lcms2")
        deps.set_property("minizip-ng", "cmake_file_name", "minizip-ng")
        deps.set_property("minizip-ng", "cmake_target_name", "MINIZIP::minizip-ng")
        deps.set_property(
            "minizip-ng::minizip", "cmake_target_name", "MINIZIP::minizip-ng"
        )
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        if self._build_tools and cross_building(self):
            # The build interpreter supplies the extension suffix to FindPython.
            package = Path(self.package_folder)
            for extension in package.rglob("*-win_amd64.pyd"):
                rename(
                    self,
                    extension,
                    extension.with_name(
                        extension.name.replace("-win_amd64.pyd", "-win_arm64.pyd")
                    ),
                )
            for extension in package.rglob("*-x86_64-linux-gnu.so"):
                rename(
                    self,
                    extension,
                    extension.with_name(
                        extension.name.replace(
                            "-x86_64-linux-gnu.so", "-aarch64-linux-gnu.so"
                        )
                    ),
                )
        copy(
            self, "LICENSE", self.source_folder, Path(self.package_folder) / "licenses"
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenColorIO")
        self.cpp_info.set_property("cmake_target_name", "OpenColorIO::OpenColorIO")
        self.cpp_info.libs = collect_libs(self)
