import os

from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "directxshadercompiler"
    version = "1.9.2602"
    license = "NCSA"

    def validate(self):
        if self.settings.os not in ["Windows", "Linux"]:
            raise RecipeInvalidConfiguration(f"{self.name} has no prebuilt binaries for {self.settings.os}")

    def latest_version(self):
        repo = GithubRepository(self, "microsoft/DirectXShaderCompiler")
        return Version(repo.latest_release.removeprefix("v"))

    def build(self):
        if self.settings.os == "Windows":
            url = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip"
            sha256 = "a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e"
        elif self.settings.os == "Linux":
            url = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz"
            sha256 = "a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60"
        else:
            raise RecipeInvalidConfiguration(f"{self.name} has no prebuilt binaries for {self.settings.os}")
        get(self, url=url, sha256=sha256, destination=self.folders.build)

    def package(self):
        for subdir in ("bin", "include", "lib"):
            src = self.folders.build / subdir
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=self.folders.package / subdir)

    def package_info(self):
        bin_dir = self.folders.package / "bin"
        self.buildenv_info.prepend_path("PATH", bin_dir)

        if self.settings.os == "Windows":
            self.info.libs = ["dxcompiler"]
        else:
            self.info.libs = ["dxcompiler"]
            self.info.system_libs = ["dl", "pthread"]
