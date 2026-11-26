#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""Adapted from ConanCenter Index's OpenImageIO recipe (MIT)."""

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import (
    apply_conandata_patches,
    collect_libs,
    copy,
    export_conandata_patches,
    get,
    rename,
    replace_in_file,
)


class Recipe(ConanFile):
    name = "openimageio"
    license = "Apache-2.0 OR BSD-3-Clause"
    package_type = "shared-library"
    settings = "os", "arch", "compiler", "build_type"

    default_options = {
        "dcmtk/*:default_dict": "builtin",
        "dcmtk/*:with_openssl": False,
        "jasper/*:with_libjpeg": "libjpeg-turbo",
        "libheif/*:with_libaomav1": True,
        "libraw/*:build_thread_safe": True,
        "libraw/*:with_jpeg": "libjpeg-turbo",
        "libultrahdr/*:with_jpeg": "libjpeg-turbo",
        "onetbb/*:tbbbind": False,
        "onetbb/*:tbbmalloc": False,
        "onetbb/*:tbbproxy": False,
    }

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    @property
    def _build_tools(self):
        return self.settings.os in ("Linux", "Macos", "Windows") and (
            self.settings.build_type != "Debug"
        )

    @property
    def _build_dicom(self):
        # DCMTK runs target binaries during Android configuration.
        return self.settings.os not in ("Android", "Emscripten")

    def requirements(self):
        if self._build_dicom:
            self.requires("dcmtk/3.7.1")
        self.requires("fmt/[>=10.2.1 <13]", transitive_headers=True)
        self.requires("freetype/[>=2.10.0 <3]")
        self.requires("giflib/6.1.3")
        self.requires("imath/[>=3.2.0 <4]", transitive_headers=True)
        self.requires("libheif/1.23.4")
        self.requires("libjpeg-turbo/[>=3.0.2 <4]")
        self.requires("libjxl/0.13.0")
        self.requires("libpng/[>=1.6 <2]")
        self.requires("libraw/0.22.3")
        self.requires("libtiff/[>=4.6.0 <5]")
        self.requires("libultrahdr/2.0.2")
        self.requires("libwebp/1.6.0")
        self.requires("opencolorio/2.6.0")
        self.requires("openexr/[>=3.2.0 <4]")
        self.requires("openjpeg/2.5.4")
        self.requires("openjph/0.33.0")
        if self.settings.os != "Emscripten":
            self.requires("onetbb/2023.1.0")
            self.requires("openvdb/12.1.1")
        self.requires("ptex/2.4.2")
        if self._build_tools:
            self.requires("pybind11/[>=3.0.2 <4]", headers=True, libs=False)
            # Only the Python bindings use CPython.
            self.requires("python/3.15.2", visible=False)
        self.requires("tsl-robin-map/[>=1.3.0 <2]")
        self.requires("zlib/[>=1.2.13 <2]")

    def build_requirements(self):
        if self._build_tools and cross_building(self):
            self.tool_requires("python/3.15.2")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        # Conan's package configs report the case-sensitive find_package names.
        source = Path(self.source_folder) / "src"
        for plugin, legacy, config in (
            ("raw", "LIBRAW_FOUND", "LibRaw_FOUND"),
            ("jpeg2000", "OPENJPEG_FOUND", "OpenJPEG_FOUND"),
        ):
            replace_in_file(
                self, source / f"{plugin}.imageio/CMakeLists.txt", legacy, config
            )
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = True
        tc.variables["CMAKE_DEBUG_POSTFIX"] = "_d"
        tc.variables["CMAKE_FIND_PACKAGE_PREFER_CONFIG"] = True
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.variables["CMAKE_INSTALL_RPATH_USE_LINK_PATH"] = False
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        # OpenImageIO uses headers and the Emscripten scanner needs an initialized sysroot.
        tc.variables["CMAKE_CXX_SCAN_FOR_MODULES"] = False
        tc.variables["JXL_LIBRARIES"] = "libjxl::libjxl"
        tc.variables["LibRaw_r_LIBRARIES"] = (
            "libraw::libraw_" if self.settings.os == "Windows" else "libraw::libraw_r"
        )
        tc.variables["OPENVDB_LIBRARIES"] = "OpenVDB::openvdb"
        tc.variables["OIIO_BUILD_TOOLS"] = self._build_tools
        tc.variables["OIIO_BUILD_TESTS"] = False
        tc.variables["OpenImageIO_BUILD_MISSING_DEPS"] = "none"
        tc.variables["OIIO_INTERNALIZE_FMT"] = False
        tc.variables["OIIO_PYTHON_BINDINGS_BACKEND"] = "pybind11"
        tc.variables["OIIO_USE_COMPILED_FMT"] = False
        tc.variables["Python3_FIND_REGISTRY"] = "NEVER"
        tc.variables["USE_PYTHON"] = self._build_tools
        tc.variables["USE_QT"] = False
        tc.variables["USE_DCMTK"] = self._build_dicom
        if self.settings.os == "Emscripten":
            tc.variables["EMBEDPLUGINS"] = True
            tc.variables["USE_TBB"] = False
            tc.variables["USE_OpenVDB"] = False
        for name in ("FFmpeg", "hwy", "Nuke", "OpenCV", "OpenGL", "R3DSDK"):
            tc.variables[f"CMAKE_DISABLE_FIND_PACKAGE_{name}"] = True
        if self.settings.os == "Linux":
            tc.variables["CMAKE_INSTALL_RPATH"] = "$ORIGIN;$ORIGIN/..;$ORIGIN/../lib"
        elif self.settings.os == "Macos":
            tc.variables["CMAKE_INSTALL_RPATH"] = (
                "@loader_path;@loader_path/..;@loader_path/../lib"
            )
        elif self.settings.os == "Windows":
            tc.extra_cxxflags.append("/bigobj")
        tc.generate()
        deps = CMakeDeps(self)
        if self._build_dicom:
            deps.set_property("dcmtk", "cmake_target_name", "DCMTK::DCMTK")
        for name, cmake_name in (
            ("libheif", "Libheif"),
            ("libjxl", "JXL"),
            ("libraw", "LibRaw"),
            ("libultrahdr", "libuhdr"),
            ("ptex", "Ptex"),
        ):
            deps.set_property(name, "cmake_file_name", cmake_name)
        deps.set_property("libheif", "cmake_target_aliases", ["heif"])
        deps.set_property("libultrahdr", "cmake_target_name", "libuhdr::libuhdr")
        deps.set_property("openjph", "cmake_target_aliases", ["openjph"])
        deps.set_property(
            "libjpeg-turbo::jpeg", "cmake_target_aliases", ["libjpeg-turbo::jpeg"]
        )
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        package = Path(self.package_folder)
        if "onetbb" in self.dependencies.host:
            tbb = Path(self.dependencies["onetbb"].package_folder)
            runtime_dir, pattern = {
                "Android": ("lib", "libtbb*.so*"),
                "iOS": ("lib", "libtbb*.dylib"),
                "Windows": ("bin", "tbb*.dll"),
                "Macos": ("lib", "libtbb*.dylib"),
                "Linux": ("lib", "libtbb.so*"),
            }[str(self.settings.os)]
            if not copy(self, pattern, tbb / runtime_dir, package / runtime_dir):
                raise ConanException(f"Missing oneTBB runtime matching {pattern}")
        if self._build_tools and cross_building(self):
            # The build interpreter supplies the extension suffix to FindPython.
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
            self, "LICENSE*", self.source_folder, Path(self.package_folder) / "licenses"
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenImageIO")
        self.cpp_info.set_property("cmake_target_name", "OpenImageIO::OpenImageIO")
        if self.settings.os == "Emscripten":
            # libheif includes Embind bindings in its static WebAssembly library.
            self.cpp_info.exelinkflags.append("-lembind")
            self.cpp_info.sharedlinkflags.append("-lembind")
        libraries = collect_libs(self)
        self.cpp_info.libs = sorted(
            lib for lib in libraries if lib.startswith("OpenImageIO")
        )
