import os

from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository

_BASE_URL = "https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.8"

_SOURCES = {
    "Windows": {
        "X64": {
            "url": f"{_BASE_URL}/clang+llvm-22.1.8-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "d96c2cc1736f4eb7fa43cb9bbdf56d93551a9ae0a9aadb9c99c3c3b2b712a234",
        },
        "ARM": {
            "url": f"{_BASE_URL}/clang+llvm-22.1.8-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "de718c58ebbc5f61d58c17b90457fcf42983bc2c4a4aba3e010d108713bfd7f1",
        },
    },
    "Linux": {
        "X64": {
            "url": f"{_BASE_URL}/LLVM-22.1.8-Linux-X64.tar.xz",
            "sha256": "df0e1ecf16caf3489a272a5eea4eec9b0d82878f6477fa309504f918a0006384",
        },
        "ARM": {
            "url": f"{_BASE_URL}/LLVM-22.1.8-Linux-ARM64.tar.xz",
            "sha256": "805efad2bb91cb4967fa569e0881d10c0f69c04461cf671cccbae19f547acc34",
        },
    },
    "Mac": {
        "ARM": {
            "url": f"{_BASE_URL}/LLVM-22.1.8-macOS-ARM64.tar.xz",
            "sha256": "f260f4f7c0d430828a81ae8a3826a1d63fc0963ec2459489308cc23b1f7eab4f",
        },
    },
}


class Recipe(RecipeBase):
    name = "llvm"
    version = "22.1.8"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "llvm/llvm-project")
        tag = repo.latest_release
        return Version(tag.removeprefix("llvmorg-"))

    def validate(self):
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        if os_name not in _SOURCES or arch not in _SOURCES[os_name]:
            raise RecipeInvalidConfiguration(f"{self.name} has no prebuilt binaries for {os_name}/{arch}")

    def build(self):
        entry = _SOURCES[str(self.settings.os)][str(self.settings.arch)]
        get(self, url=entry["url"], sha256=entry["sha256"], destination=self.folders.build, strip_root=True)

    def package(self):
        for subdir in ("bin", "include", "lib", "libexec", "share"):
            src = self.folders.build / subdir
            dst = self.folders.package / subdir
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=dst)

    def package_info(self):
        bin_dir = os.path.join(self.folders.package, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
        self.buildenv_info.define_path("LLVM_DIR", self.folders.package)
        self.buildenv_info.define_path("LIBCLANG_PATH", os.path.join(self.folders.package, "lib"))
        self.conf_info.define("tools.llvm:dir", self.folders.package)
