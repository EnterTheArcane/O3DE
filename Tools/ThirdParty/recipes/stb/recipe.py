import os

from thirdparty import RecipeBase
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version


class Recipe(RecipeBase):
    name = "stb"
    version = "20240531"
    license = "Unlicense", "MIT"

    @property
    def _version(self):
        return str(self.version)[4:]

    def source(self):
        get(
            self,
            url="https://github.com/nothings/stb/archive/013ac3beddff3dbffafd5177e7972067cd2b5083.zip",
            sha256="b7f476902bbef1b30f8ecc2d9d95c459c32302c8b559d09b589b5955463b7af8",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=self.source_folder, dst=os.path.join(self.package_folder, "include"))
        copy(self, "stb_vorbis.c", src=self.source_folder, dst=os.path.join(self.package_folder, "include"))
        rmdir(self, os.path.join(self.package_folder, "include", "tests"))
        rmdir(self, os.path.join(self.package_folder, "include", "deprecated"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "deprecated"), dst=os.path.join(self.package_folder, "include"))
        copy(self, "stb_image.c", src=os.path.join(self.source_folder, "deprecated"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.defines.append("STB_TEXTEDIT_KEYTYPE=unsigned")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
