import os

from thirdparty import RecipeBase
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "cmake"
    version = "4.3.3"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "Kitware/CMake")
        return Version(repo.latest_release.removeprefix("v"))

    def build(self):
        if self.settings.os == "Windows":
            if self.settings.arch == "ARM":
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-windows-arm64.zip"
                sha256 = "db6e902b5ba6a08d0abed136763c4bd95adda17e882d659c0f5d14fe158f7395"
            else:
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-windows-x86_64.zip"
                sha256 = "935ade9e5e8723583c07f44c5592cea2a1c8f65c56ca7e07b34c025c880e0bd6"
        elif self.settings.os == "Linux":
            if self.settings.arch == "ARM":
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-linux-aarch64.tar.gz"
                sha256 = "9ea38356dbd3e32e51029a3e09a0f2f8e117ef4fbcaad7a21ffb36409bbd5cb4"
            else:
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-linux-x86_64.tar.gz"
                sha256 = "927b2368a946c37269c3a66225ab00544e756459cdd0b5d0da438694fb9ff802"
        elif self.settings.os == "Mac":
            url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-macos-universal.tar.gz"
            sha256 = "5221a13450c7a0219a2a0d1b6c9085eb06489721fafd8488ccebc1584175d2fb"
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
            src = os.path.join(self.folders.build, subdir)
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=os.path.join(self.folders.package, subdir))

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []
        bin_dir = os.path.join(self.folders.package, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
