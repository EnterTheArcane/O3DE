# wayland-protocols — Linux-only Meson build
import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain


class Recipe(RecipeBase):
    name = "wayland-protocols"
    license = "MIT"

    def requirements(self) -> list[str]:
        if not self.is_linux:
            return []
        return ["wayland"]

    def source(self):
        if not self.is_linux:
            return
        get(url=self.thirdparty_data["versions"][self.version]["url"],
            dest=self.source_folder,
            sha256=self.thirdparty_data["versions"][self.version]["sha256"])

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
        copy("COPYING", self.source_folder, os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rmdir(os.path.join(self.package_folder, "res", "pkgconfig"))
