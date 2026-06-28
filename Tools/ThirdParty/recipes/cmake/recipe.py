import os

from thirdparty import RecipeBase
from thirdparty.errors import RecipeException
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "cmake"
    version = "4.3.4"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "Kitware/CMake")
        return Version(repo.latest_release.removeprefix("v"))

    def requirements(self):
        self.requires_tool("ninja")

    def build(self):
        if self.settings.os == "Windows":
            if self.settings.arch == "ARM":
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.4/cmake-4.3.4-windows-arm64.zip"
                sha256 = "91a59ca6ffbcec18e1fb05431fa22f538ab615b6e525d1bfd0b8dd67a4d45685"
            else:
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.4/cmake-4.3.4-windows-x86_64.zip"
                sha256 = "86e5fcafb38bdf58346a78b187c7b6b4f252ae5242cffe24c463a92bbd2e77d1"
        elif self.settings.os == "Linux":
            if self.settings.arch == "ARM":
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.4/cmake-4.3.4-linux-aarch64.tar.gz"
                sha256 = "56a8014a8f28b8ff9cbe2c6fa8beebc028ac5b1987195d122b847fb486dc5282"
            else:
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.4/cmake-4.3.4-linux-x86_64.tar.gz"
                sha256 = "ca6f08ccbd5e6b0a9068d33317d0d1aff7278d08cccaed4529b8fbead7942a68"
        elif self.settings.os == "Mac":
            url = "https://github.com/Kitware/CMake/releases/download/v4.3.4/cmake-4.3.4-macos-universal.tar.gz"
            sha256 = "bf6647c78ac295c54dbe0a094d4428f495be93c1f810fd8bde57374e8b548523"
        else:
            raise RecipeException(f"Unsupported OS: {self.settings.os}")
        get(
            self,
            url=url,
            sha256=sha256,
            destination=self.folders.build,
            strip_root=True)

    def package(self):
        for subdir in ("bin", "share", "lib"):
            src = self.folders.build / subdir
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=self.folders.package / subdir)

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []
        bin_dir = self.folders.package / "bin"
        self.info.buildenv.prepend_path("PATH", bin_dir)
