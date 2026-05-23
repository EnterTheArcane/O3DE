from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
import os

class Recipe(RecipeBase):
    name = "cmake"
    version = "4.3.3"
    license = "BSD-3-Clause"

    def build(self):
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        if os_name == "Windows":
            if arch == "armv8":
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-windows-arm64.zip"
                sha256 = "db6e902b5ba6a08d0abed136763c4bd95adda17e882d659c0f5d14fe158f7395"
            else:
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-windows-x86_64.zip"
                sha256 = "935ade9e5e8723583c07f44c5592cea2a1c8f65c56ca7e07b34c025c880e0bd6"
        elif os_name == "Linux":
            if arch == "armv8":
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-linux-aarch64.tar.gz"
                sha256 = "9ea38356dbd3e32e51029a3e09a0f2f8e117ef4fbcaad7a21ffb36409bbd5cb4"
            else:
                url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-linux-x86_64.tar.gz"
                sha256 = "927b2368a946c37269c3a66225ab00544e756459cdd0b5d0da438694fb9ff802"
        else:  # macOS
            url = "https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-macos-universal.tar.gz"
            sha256 = "5221a13450c7a0219a2a0d1b6c9085eb06489721fafd8488ccebc1584175d2fb"
        get(
            self,
            url=url,
            sha256=sha256,
            destination=self.build_folder,
            strip_root=True)

    def package(self):
        for subdir in ("bin", "share", "lib"):
            src = os.path.join(self.build_folder, subdir)
            if os.path.isdir(src):
                copy(self, "*", src=src, dst=os.path.join(self.package_folder, subdir))

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        bin_dir = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
