# Wayland — Linux-only Meson build
import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain


class Recipe(RecipeBase):
    name = "wayland"
    version = "1.24.0"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_libraries": [True, False],
        "enable_dtd_validation": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "enable_libraries": True,
        "enable_dtd_validation": True,
    }

    def requirements(self) -> list[str]:
        if not self.is_linux:
            return []
        return ["libffi", "libxml2", "expat"]

    def source(self):
        if not self.is_linux:
            return
        get(
            url="https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.24.0/downloads/wayland-1.24.0.tar.xz",
            dest=self.source_folder,
            sha256="82892487a01ad67b334eca83b54317a7c86a03a89cfadacfef5211f11a5d0536",
        )

    def generate(self):
        if not self.is_linux:
            return
        tc = MesonToolchain(self)
        tc.project_options["libraries"] = (
            "true" if self.options.enable_libraries else "false"
        )
        tc.project_options["dtd_validation"] = (
            "true" if self.options.enable_dtd_validation else "false"
        )
        tc.project_options["documentation"] = "false"
        tc.project_options["scanner"] = "false"
        tc.generate()

    def build(self):
        if not self.is_linux:
            return
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        if not self.is_linux:
            return
        copy(
            "COPYING",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        meson = Meson(self)
        meson.install()
        rmdir(os.path.join(self.package_folder, "share"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
