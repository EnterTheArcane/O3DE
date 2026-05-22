# Wayland — Linux-only Meson build
import os

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain


class Recipe(RecipeBase):
    name = "wayland"
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
        get(url=self.thirdparty_data["versions"][self.version]["url"],
            dest=self.source_folder,
            sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        if not self.is_linux:
            return
        tc = MesonToolchain(self)
        tc.project_options["libraries"] = "true" if self.options.enable_libraries else "false"
        tc.project_options["dtd_validation"] = "true" if self.options.enable_dtd_validation else "false"
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
        copy("COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rmdir(os.path.join(self.package_folder, "share"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
