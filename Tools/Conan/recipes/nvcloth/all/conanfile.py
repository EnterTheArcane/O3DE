#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.env import Environment
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
    load,
    replace_in_file,
    save,
)


class Recipe(ConanFile):
    name = "nvcloth"
    license = "Nvidia Source Code License (1-Way Commercial)"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    def export_sources(self):
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)
        export_conandata_patches(self)

    def validate(self):
        if self.settings.os not in ("Android", "Linux", "Macos", "Windows", "iOS"):
            raise ConanInvalidConfiguration("NvCloth requires a native platform")

    def package_id(self):
        self.info.settings.rm_safe("build_type")
        self.info.settings.rm_safe("compiler.runtime_type")

    def layout(self):
        self.folders.source = "src"
        self.folders.build = "build"

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)
        replace_in_file(
            self,
            "NvCloth/src/ps/android/cpu-features.c",
            "#include <errno.h>",
            "#include <errno.h>\n#include <string.h>\n#include <unistd.h>",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja Multi-Config"
        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        if self.settings.os == "Android":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
            tc.cache_variables["PX_STATIC_LIBRARIES"] = True
            tc.cache_variables["APPEND_CONFIG_NAME"] = False
        else:
            tc.blocks.remove("cppstd")
            tc.cache_variables["NVCLOTH_SOURCE_DIR"] = Path(
                self.source_folder
            ).as_posix()
            tc.cache_variables["NVCLOTH_TARGET_ARCH"] = str(self.settings.arch)
            tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = (
                "Debug;RelWithDebInfo;Release"
            )
            if self.settings.os == "Windows":
                tc.blocks.remove("vs_runtime")
                runtime = "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                if self.settings.compiler.runtime == "dynamic":
                    runtime += "DLL"
                tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = runtime
        tc.generate()

    def build(self):
        cmake = CMake(self)
        if self.settings.os != "Android":
            cmake.configure(build_script_folder=Path(self.source_folder).parent)
            for config in ("Debug", "RelWithDebInfo", "Release"):
                cmake.build(build_type=config)
            return

        environment = Environment()
        environment.define_path("GW_DEPS_ROOT", self.source_folder)
        with environment.vars(self).apply():
            cmake.configure(
                build_script_folder=Path(self.source_folder)
                / "NvCloth/compiler/cmake/android"
            )
            for config in ("debug", "profile", "release"):
                cmake.build(build_type=config)

    def package(self):
        source = Path(self.source_folder)
        package = Path(self.package_folder)
        copy(self, "*.h", source / "NvCloth/include", package / "include")
        copy(self, "*.h", source / "NvCloth/extensions/include", package / "include")
        copy(self, "*.h", source / "PxShared/include", package / "include")
        for component in ("NvCloth", "PxShared"):
            copy(
                self,
                "license.txt",
                source / component,
                package / "licenses" / component,
            )
        android = self.settings.os == "Android"
        if android:
            cpu_features = source / "NvCloth/src/ps/android/cpu-features.c"
            notice = load(self, cpu_features).split("*/", 1)[0].removeprefix("/*")
            save(
                self,
                package / "licenses/android/cpu-features.txt",
                notice.strip() + "\n",
            )
        library = "NvCloth.lib" if self.settings.os == "Windows" else "libNvCloth.a"
        for config in ("debug", "profile", "release"):
            build_config = config
            if not android:
                build_config = (
                    "RelWithDebInfo" if config == "profile" else config.title()
                )
            libraries = copy(
                self,
                library,
                Path(self.build_folder) / build_config,
                package / "lib" / config,
            )
            if not libraries:
                raise ConanInvalidConfiguration(
                    f"NvCloth built no library for {config}"
                )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "NvCloth")
        self.cpp_info.set_property("cmake_target_name", "NvCloth::NvCloth")
        config = {"Debug": "debug", "RelWithDebInfo": "profile"}.get(
            str(self.settings.build_type), "release"
        )
        self.cpp_info.libdirs = [f"lib/{config}"]
        self.cpp_info.libs = ["NvCloth"]
        self.cpp_info.defines = ["NV_CLOTH_IMPORT=", "PX_CALL_CONV="]
