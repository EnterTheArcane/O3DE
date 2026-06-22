import os

from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get


class Recipe(RecipeBase):
    name = "jom"
    version = "1.1.4"
    license = "GPL-3.0-only"

    def validate(self):
        if self.settings.os != "Windows":
            raise RecipeInvalidConfiguration("jom is only available on Windows")

    def build(self):
        get(
            self,
            url="https://download.qt.io/official_releases/jom/jom_1_1_4.zip",
            sha256="d533c1ef49214229681e90196ed2094691e8c4a0a0bef0b2c901debcb562682b",
            destination=self.folders.build)

    def package(self):
        copy(self, "jom.exe", src=self.folders.build, dst=os.path.join(self.folders.package, "bin"))

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        self.buildenv_info.prepend_path("PATH", os.path.join(self.folders.package, "bin"))
