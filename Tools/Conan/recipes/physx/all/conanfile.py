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
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.cmake.utils import is_multi_configuration
from conan.tools.files import get

CONFIGURATIONS = ("debug", "checked", "profile", "release")


class Recipe(ConanFile):
    name = "physx"
    license = "Apache-2.0"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    options = {"with_cuda": [True, False]}
    default_options = {"with_cuda": True}

    def config_options(self):
        if self.settings.os not in ("Linux", "Windows") or self.settings.arch not in (
            "x86_64",
            "armv8",
        ):
            self.options.rm_safe("with_cuda")

    def build_requirements(self):
        if self.options.get_safe("with_cuda"):
            self.tool_requires("cuda-toolkit/13.5.0")

    @property
    def _multi_config(self):
        return is_multi_configuration(
            self.conf.get("tools.cmake.cmaketoolchain:generator", default="Ninja")
        )

    @property
    def _configuration(self):
        build_type = str(self.settings.build_type)
        return {"RelWithDebInfo": "profile", "MinSizeRel": "release"}.get(
            build_type, build_type.lower()
        )

    @property
    def _build_configurations(self):
        return CONFIGURATIONS if self._multi_config else (None,)

    def package_id(self):
        if self._multi_config:
            self.info.settings.rm_safe("build_type")
            self.info.settings.rm_safe("compiler.runtime_type")
            self.info.conf.define("user.physx:multi_config", True)

    def layout(self):
        self.folders.source = "src"
        self.folders.build = "build"

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        os_name = str(self.settings.os)
        tc = CMakeToolchain(self, generator="Ninja")
        tc.blocks.remove("cppstd")
        tc.cache_variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.cache_variables["CMAKE_INSTALL_DATADIR"] = "."
        if os_name == "Windows":
            tc.cache_variables["NV_USE_STATIC_WINCRT"] = (
                self.settings.get_safe("compiler.runtime") == "static"
            )
        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        with_cuda = bool(self.options.get_safe("with_cuda"))
        tc.cache_variables["PX_GENERATE_GPU_PROJECTS"] = with_cuda
        if with_cuda:
            tc.cache_variables["PX_GENERATE_GPU_STATIC_LIBRARIES"] = False
            # CUDA 13 targets Turing or newer. PTX lets the driver JIT for newer GPUs.
            tc.cache_variables["CMAKE_CUDA_ARCHITECTURES"] = "75-virtual"
        tc.cache_variables["PX_GENERATE_STATIC_LIBRARIES"] = True
        tc.cache_variables["PX_BUILDPVDRUNTIME"] = os_name in (
            "Linux",
            "Windows",
        )
        tc.cache_variables["TARGET_BUILD_PLATFORM"] = {
            "Android": "android",
            "Emscripten": "linux",
            "iOS": "ios",
            "Linux": "linux",
            "Macos": "mac",
            "Windows": "windows",
        }[os_name]
        if os_name == "Emscripten":
            wasm64 = self.settings.arch == "wasm64"
            tc.cache_variables["PHYSX_EMSCRIPTEN_ARCH"] = (
                "wasm64" if wasm64 else "wasm32"
            )
            if wasm64:
                # The SDK reads the C flags before CMake probes pointer size.
                for language in ("C", "CXX"):
                    flags = self.conf.get(
                        f"tools.build:{language.lower()}flags", default=[]
                    )
                    tc.cache_variables[f"CMAKE_{language}_FLAGS"] = " ".join(
                        ["-m64", *flags]
                    )
        tc.generate()

    def build(self):
        cmake = CMake(self)
        variables = {}
        if not self._multi_config:
            # Conan presets use Conan's build-type name.
            variables["CMAKE_BUILD_TYPE"] = self._configuration
        cmake.configure(
            variables=variables, build_script_folder=Path(self.source_folder) / "physx"
        )
        for config in self._build_configurations:
            cmake.build(build_type=config)

    def package(self):
        cmake = CMake(self)
        for config in self._build_configurations:
            cmake.install(build_type=config)
            if config and not (Path(self.package_folder) / "lib" / config).is_dir():
                raise ConanException(
                    "PhysX source must install multi-config libraries under lib/<config>"
                )

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = ["lib/cmake/PhysX"]
        if self._multi_config:
            self.cpp_info.libdirs = [f"lib/{self._configuration}"]
            self.cpp_info.bindirs = [f"bin/{self._configuration}"]
