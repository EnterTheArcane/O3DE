import os

from thirdparty import RecipeBase
from thirdparty._conan.errors import ConanInvalidConfiguration
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "directxshadercompiler"
    version = "1.9.2602"
    license = "NCSA"

    def validate(self):
        if self.settings.os not in ["Windows", "Linux"]:
            raise ConanInvalidConfiguration(f"{self.name} has no prebuilt binaries for {self.settings.os}")

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
            raise ConanInvalidConfiguration(f"{self.name} has no prebuilt binaries for {self.settings.os}")
        get(self, url=url, sha256=sha256, destination=self.build_folder)

    def package(self):
        for subdir in ("bin", "include", "lib"):
            src = os.path.join(self.build_folder, subdir)
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=os.path.join(self.package_folder, subdir))

    def package_info(self):
        bin_dir = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)

        if self.settings.os == "Windows":
            self.cpp_info.libs = ["dxcompiler"]
        else:
            self.cpp_info.libs = ["dxcompiler"]
            self.cpp_info.system_libs = ["dl", "pthread"]
