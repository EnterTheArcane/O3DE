# wayland-protocols — Linux-only Meson build
import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain


class Recipe(RecipeBase):
    name = "wayland-protocols"
    version = "1.45"
    license = "MIT"

    def requirements(self) -> list[str]:
        if not self.is_linux:
            return []
        return ["wayland"]

    def source(self):
        if not self.is_linux:
            return
        get(
            url="https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.45/downloads/wayland-protocols-1.45.tar.xz",
            dest=self.source_folder,
            sha256="4d2b2a9e3e099d017dc8107bf1c334d27bb87d9e4aff19a0c8d856d17cd41ef0",
        )

    def generate(self):
        if not self.is_linux:
            return
        tc = MesonToolchain(self)
        tc.project_options["datadir"] = "res"
        tc.project_options["tests"] = "false"
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
            "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses")
        )
        meson = Meson(self)
        meson.install()
        rmdir(os.path.join(self.package_folder, "res", "pkgconfig"))
