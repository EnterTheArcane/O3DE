import os

from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository

_BASE_URL = "https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.6"

_SOURCES = {
    "Windows": {
        "X64": {
            "url": f"{_BASE_URL}/clang+llvm-22.1.6-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "657343edf361ca463bd642e39c74b251c6338b96cdbd55ff277555298b027696",
        },
        "ARM": {
            "url": f"{_BASE_URL}/clang+llvm-22.1.6-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "841278d0f7e091653af22cea7643fbeba587f702edf19247713180036c5b4cdb",
        },
    },
    "Linux": {
        "X64": {
            "url": f"{_BASE_URL}/LLVM-22.1.6-Linux-X64.tar.xz",
            "sha256": "c5ac8ef89ca39d30cb32e9b83772f995dd891c685ebc188d593c943a64d5f8b5",
        },
        "ARM": {
            "url": f"{_BASE_URL}/LLVM-22.1.6-Linux-ARM64.tar.xz",
            "sha256": "b67817634e8e1c2632dfc056af14d61b94f8e6502f4e557560eea227aa22ce37",
        },
    },
    "Mac": {
        "ARM": {
            "url": f"{_BASE_URL}/LLVM-22.1.6-macOS-ARM64.tar.xz",
            "sha256": "8059d9d9eeb059c30d812b4a37291888f8dcba04d2b5ace61fd12d2904eaa0e9",
        },
    },
}


class Recipe(RecipeBase):
    name = "llvm"
    version = "22.1.6"
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
        get(self, url=entry["url"], sha256=entry["sha256"], destination=self.build_folder, strip_root=True)

    def package(self):
        for subdir in ("bin", "include", "lib", "libexec", "share"):
            src = os.path.join(self.build_folder, subdir)
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=os.path.join(self.package_folder, subdir))

    def package_info(self):
        bin_dir = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
        self.buildenv_info.define_path("LLVM_DIR", self.package_folder)
        self.buildenv_info.define_path("LIBCLANG_PATH", os.path.join(self.package_folder, "lib"))
        self.conf_info.define("tools.llvm:dir", self.package_folder)
