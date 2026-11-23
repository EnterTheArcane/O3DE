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
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import Environment
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
    rm,
)


class Recipe(ConanFile):
    name = "qt"
    license = "LGPL-3.0-only"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "shared-library"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        for pattern in ("QT-NOTICE.TXT", "LICENSE"):
            copy(self, pattern, self.recipe_folder, self.export_sources_folder)
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def configure(self):
        if self.settings.os in ("iOS", "Emscripten"):
            self.package_type = "static-library"

    def package_id(self):
        # The Windows SDK contains both Release and Debug builds.
        if self.info.settings.os == "Windows":
            self.info.settings.rm_safe("build_type")
            self.info.settings.rm_safe("compiler.runtime_type")

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("openssl/4.1.0")

    def build_requirements(self):
        if self.settings.os == "Android":
            self.tool_requires("android-sdk/36.0.0")
        if cross_building(self):
            self.tool_requires(f"{self.name}/{self.version}")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        windows = self.settings.os == "Windows"
        linux = self.settings.os == "Linux"
        macos = self.settings.os == "Macos"
        desktop = windows or linux or macos
        tc = CMakeToolchain(self)
        if windows:
            tc.generator = "Ninja Multi-Config"
        tc.absolute_paths = True
        tc.blocks.remove("cppstd")
        tc.variables["BUILD_SHARED_LIBS"] = self.package_type == "shared-library"
        tc.variables["BUILD_qtactiveqt"] = False
        tc.variables["BUILD_qtdeclarative"] = False
        tc.variables["BUILD_qtlanguageserver"] = False
        tc.variables["BUILD_qtshadertools"] = False
        tc.variables["CMAKE_CXX_STANDARD"] = 20
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.variables["QT_BUILD_EXAMPLES"] = False
        tc.variables["QT_BUILD_TESTS"] = False
        modules = ["qtbase", "qtimageformats", "qtsvg"]
        if desktop:
            modules.extend(("qttools", "qttranslations"))
        if linux:
            modules.append("qtwayland")
        tc.variables["QT_BUILD_SUBMODULES"] = ";".join(modules)
        tc.variables["FEATURE_icu"] = False
        tc.variables["FEATURE_printsupport"] = False
        tc.variables["FEATURE_sql"] = False
        tc.variables["FEATURE_wayland_server"] = False
        tc.variables["FEATURE_dbus"] = linux
        tc.variables["FEATURE_designer"] = False
        tc.variables["FEATURE_linguist"] = desktop
        tc.variables["FEATURE_assistant"] = False
        tc.variables["FEATURE_distancefieldgenerator"] = False
        tc.variables["FEATURE_kmap2qmap"] = False
        tc.variables["FEATURE_pixeltool"] = False
        tc.variables["FEATURE_qdbus"] = False
        tc.variables["FEATURE_qdoc"] = False
        tc.variables["FEATURE_qtattributionsscanner"] = False
        tc.variables["FEATURE_qtdiag"] = False
        tc.variables["FEATURE_qtplugininfo"] = False
        tc.variables["FEATURE_system_zlib"] = False
        tc.variables["INPUT_tiff"] = "qt"
        tc.variables["INPUT_webp"] = "qt"
        tc.variables["INPUT_libjpeg"] = "qt"
        tc.variables["INPUT_openssl"] = "linked" if linux else "no"
        if cross_building(self):
            host_qt = Path(self.dependencies.build["qt"].package_folder) / "qt"
            tc.variables["QT_HOST_PATH"] = host_qt.as_posix()
            tc.variables["QT_BUILD_TOOLS_WHEN_CROSSCOMPILING"] = desktop
            if windows:
                tc.variables["CMAKE_SYSTEM_NAME"] = "Windows"
                tc.variables["CMAKE_SYSTEM_PROCESSOR"] = "ARM64"
        if windows:
            tc.blocks.remove("vs_runtime")
            tc.variables["CMAKE_CONFIGURATION_TYPES"] = "Release;Debug"
            tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = (
                "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
            tc.variables["INPUT_opengl"] = "dynamic"
        elif linux:
            tc.variables["FEATURE_xcb"] = True
            openssl = Path(self.dependencies["openssl"].package_folder)
            CMakeDeps(self).generate()
            tc.variables["OpenSSL_DIR"] = Path(self.generators_folder).as_posix()
            # Qt's OpenSSL wrappers also check the legacy FindOpenSSL variables.
            tc.variables["OPENSSL_INCLUDE_DIR"] = (openssl / "include").as_posix()
            tc.variables["OPENSSL_CRYPTO_LIBRARY"] = "OpenSSL::Crypto"
            tc.variables["CMAKE_TRY_COMPILE_CONFIGURATION"] = str(
                self.settings.build_type
            )
        elif macos:
            tc.variables["FEATURE_framework"] = True
        elif self.settings.os == "Emscripten":
            tc.cache_variables["QT_QMAKE_TARGET_MKSPEC"] = "wasm-emscripten"
            tc.variables["FEATURE_thread"] = False
        elif self.settings.os == "iOS":
            # Qt initializes its deployment target before loading the toolchain.
            tc.cache_variables["CMAKE_OSX_DEPLOYMENT_TARGET"] = str(
                self.settings.os.version
            )
        elif self.settings.os == "Android":
            sdk = self.dependencies.build["android-sdk"]
            tc.variables["ANDROID_SDK_ROOT"] = Path(sdk.package_folder).as_posix()
            tc.variables["QT_ALLOW_DOWNLOAD"] = True
            tc.variables["QT_ANDROID_SDK_BUILD_TOOLS_REVISION"] = str(sdk.ref.version)
            tc.variables["QT_ANDROID_MIN_SDK_VERSION"] = int(
                str(self.settings.os.api_level)
            )
            tc.variables["QT_ANDROID_COMPILE_SDK_VERSION"] = "android-36"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        if self.settings.os != "Windows":
            cmake.build()
            return

        cmake.build(build_type="Release")
        # Debug host tools run from bin/Debug and load Qt DLLs from bin.
        env = Environment()
        env.prepend_path("PATH", str(Path(self.build_folder) / "qtbase/bin"))
        with env.vars(self).apply():
            cmake.build(build_type="Debug")

    def package(self):
        package = Path(self.package_folder)
        sdk = package / "qt"
        cmake = CMake(self)
        install_args = ["--prefix", sdk.as_posix()]
        if self.settings.os == "Windows":
            cmake.install(build_type="Release", cli_args=install_args)
            cmake.install(build_type="Debug", cli_args=install_args)
        else:
            cmake.install(cli_args=install_args)
        assets = Path(self.source_folder).parent
        if not copy(self, "LICENSE", assets, sdk):
            raise ConanException("Missing the Qt license")
        for folder in Path(self.source_folder).glob("qt*"):
            if (folder / "LICENSES").is_dir():
                copy(
                    self,
                    "*.txt",
                    folder / "LICENSES",
                    package / "licenses" / folder.name,
                )
        if self.settings.os == "Linux" and self.conf.get("user.package:prune_build"):
            rm(self, "cmake_pch*.gch", self.build_folder, recursive=True)

    def package_info(self):
        self.cpp_info.bindirs = ["qt/bin"]
        self.cpp_info.libdirs = ["qt/lib"]
        self.cpp_info.includedirs = ["qt/include"]
