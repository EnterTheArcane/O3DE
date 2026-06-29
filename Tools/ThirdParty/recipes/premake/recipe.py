import os
import stat

from thirdparty import RecipeBase
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "premake"
    version = "5.0.0-beta8"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "premake/premake-core")
        return Version(repo.latest_release.lstrip("v"))

    def build(self):
        if self.settings.os == "Windows":
            url = "https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-windows.zip"
            sha256 = "e64ce2ed8778e0098f63674cca61fe33941b5f0c8d9a4afd651152bdea3758ab"
        elif self.settings.os == "Linux":
            url = "https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-linux.tar.gz"
            sha256 = "63edd3e7461eebdd45b500a3c7e8ad4e7a67d68f230010f9a97cbb71b4ec59c8"
        elif self.settings.os == "Mac":
            url = "https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-macosx.tar.gz"
            sha256 = "fa73a46f093fa6f17494a3d063421aa6cae3ea825a61c62dd59fc2f07a256d03"
        else:
            raise Exception(f"Unsupported OS: {self.settings.os}")
        get(
            self,
            url=url,
            sha256=sha256,
            destination=self.folders.build,
            strip_root=False)

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.build, dst=self.folders.package / "licenses")
        suffix = ".exe" if self.settings.os == "Windows" else ""
        copy(self, f"premake5{suffix}", src=self.folders.build, dst=self.folders.package / "bin")
        if self.settings.os != "Windows":
            premake5_path = self.folders.package / "bin" / "premake5"
            os.chmod(premake5_path, os.stat(premake5_path).st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    def package_info(self):
        self.info.includedirs = []
        self.info.libdirs = []
        bin_dir = self.folders.package / "bin"
        self.info.buildenv.prepend_path("PATH", bin_dir)
