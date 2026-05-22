# Ported from conan-center-index/hidapi by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.apple import is_apple_os
import os


class Recipe(RecipeBase):
    name = "hidapi"
    version = "0.15.0"
    license = "GPL-3.0-or-later", "BSD-3-Clause"
    options = {
        "fPIC": [True, False],
        "shared": [True, False],
    }
    default_options = {
        "fPIC": True,
        "shared": False,
    }

    @property
    def _msbuild_configuration(self):
        return "Debug" if self.build_type == "Debug" else "Release"

    def requirements(self) -> list[str]:
        return []  # libusb/libudev are Linux-only; Windows uses Win32 HID API

    def source(self):
        get(
            url="https://github.com/libusb/hidapi/archive/hidapi-0.15.0.tar.gz",
            dest=self.source_folder,
            sha256="5d84dec684c27b97b921d2f3b73218cb773cf4ea915caee317ac8fc73cef8136",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = PkgConfigDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
