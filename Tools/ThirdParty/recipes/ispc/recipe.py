import os

from thirdparty import RecipeBase
from thirdparty.errors import InvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "ispc"
    version = "1.30.0"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "ispc/ispc")
        return Version(repo.latest_release.removeprefix("v"))

    def build(self):
        if self.settings.os == "Windows":
            url = "https://github.com/ispc/ispc/releases/download/v1.30.0/ispc-v1.30.0-windows.zip"
            sha256 = "e126a78fd15f12475bac6204f2536b3986e07973127e4f8e7336c3d304c4a69f"
        elif self.settings.os == "Linux":
            if self.settings.arch == "armv8":
                url = "https://github.com/ispc/ispc/releases/download/v1.30.0/ispc-v1.30.0-linux.aarch64.tar.gz"
                sha256 = "509399c399ec162d746889458a10cc13797a1aed1c0164b2bd3faddf7d023f13"
            else:
                url = "https://github.com/ispc/ispc/releases/download/v1.30.0/ispc-v1.30.0-linux.tar.gz"
                sha256 = "63e7d61037849fa1ed644f0398d21740ee9f880b9bf81f017c65eebe1d42c02b"
        elif self.settings.os == "Macos":
            url = "https://github.com/ispc/ispc/releases/download/v1.30.0/ispc-v1.30.0-macOS.universal.tar.gz"
            sha256 = "4a45b95d9cd590acbdcd158d287f9398d4a1961461e9456bda09181e7b34912e"
        else:
            raise InvalidConfiguration("Unsupported platform")
        get(self, url=url, sha256=sha256, destination=self.build_folder, strip_root=True)

    def package(self):
        copy(
            self,
            "*",
            src=os.path.join(self.build_folder, "bin"),
            dst=os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        self.buildenv_info.prepend_path("PATH", os.path.join(self.package_folder, "bin"))
