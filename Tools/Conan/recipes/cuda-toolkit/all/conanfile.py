#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from pathlib import Path
from tempfile import TemporaryDirectory

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get, save


class Recipe(ConanFile):
    name = "cuda-toolkit"
    license = "NVIDIA CUDA Toolkit EULA"
    homepage = "https://developer.nvidia.com/cuda-toolkit"
    package_type = "application"
    settings = "os", "arch"

    options = {
        "with_compiler": [True, False],
    }
    default_options = {
        "with_compiler": True,
    }

    def validate(self):
        sources = self.conan_data["sources"][self.version]
        if str(self.settings.arch) not in sources.get(str(self.settings.os), {}):
            raise ConanInvalidConfiguration(
                "CUDA requires Windows or Linux on x64 or ARM64"
            )

    def package_id(self):
        self.info.conf.clear()

    def package(self):
        package = Path(self.package_folder)
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        sources = self.conan_data["sources"][self.version][os_name]
        native = sources[arch]
        components = native["components"]
        if not self.options.with_compiler:
            components = {
                name: archive
                for name, archive in components.items()
                if name not in ("cuda_nvcc", "libnvvm")
            }
        licenses = package / "licenses" / os_name
        self._install_components(
            components, package / native.get("target_directory", ""), licenses / arch
        )
        # Consumers can locate a target toolkit without running its compiler.
        save(self, package / "version.txt", f"CUDA Version {self.version}\n")

        # The x64 compiler can target ARM64 with the corresponding headers and runtime.
        if self.options.with_compiler and arch == "x86_64":
            target = sources["armv8"]
            target_components = ("cuda_cudart",)
            if os_name == "Linux":
                target_components += ("cuda_crt", "cccl")
            self._install_components(
                {name: target["components"][name] for name in target_components},
                package / target.get("target_directory", ""),
                licenses / "armv8",
                cross=True,
            )
            if os_name == "Linux":
                # NVCC emits host code that includes this header for both targets.
                copy(
                    self,
                    "fatbinary_section.h",
                    package / native["target_directory"] / "include",
                    package / target["target_directory"] / "include",
                )

        if os_name == "Linux":
            target = Path(native["target_directory"])
            (package / "include").symlink_to(
                target / "include", target_is_directory=True
            )
            (package / "lib64").symlink_to(target / "lib", target_is_directory=True)

    def _install_components(self, components, target, licenses, *, cross=False):
        toolkit_url = "https://developer.download.nvidia.com/compute/cuda/redist"
        package = Path(self.package_folder)
        for name, archive in components.items():
            with TemporaryDirectory(dir=self.build_folder) as directory:
                source = Path(directory)
                get(
                    self,
                    f"{toolkit_url}/{archive['path']}",
                    sha256=archive["sha256"],
                    destination=directory,
                    strip_root=True,
                    keep_permissions=True,
                )
                copy(self, "LICENSE*", source, licenses / name)
                for folder in ("include", "lib", "res"):
                    if cross and self.settings.os == "Windows" and folder == "include":
                        continue
                    copy(self, "*", source / folder, target / folder)
                if not cross:
                    for folder in ("bin", "nvvm"):
                        copy(self, "*", source / folder, package / folder)

    def package_info(self):
        package = Path(self.package_folder)
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.buildenv_info.define_path("CUDAToolkit_ROOT", str(package))
        if self.options.with_compiler:
            nvcc = "nvcc.exe" if self.settings.os == "Windows" else "nvcc"
            self.buildenv_info.define_path("CUDACXX", str(package / "bin" / nvcc))
