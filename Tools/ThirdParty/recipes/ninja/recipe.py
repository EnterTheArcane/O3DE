from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.files import copy, get
import os

class Recipe(ConanFile):
    name = "ninja"
    version = "1.13.2"
    license = "Apache-2.0"

    def source(self):
        pass

    def build(self):
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        if os_name == "Windows":
            if arch == "armv8":
                url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-winarm64.zip"
                sha256 = "e52f0bdef9dfb1003229dbd6508a508c4073fd017247002adc66e5e806cb0391"
            else:
                url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip"
                sha256 = "07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65"
        elif os_name == "Linux":
            if arch == "armv8":
                url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-linux-aarch64.zip"
                sha256 = "fd2cacc8050a7f12a16a2e48f9e06fca5c14fc4c2bee2babb67b58be17a607fc"
            else:
                url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-linux.zip"
                sha256 = "5749cbc4e668273514150a80e387a957f933c6ed3f5f11e03fb30955e2bbead6"
        else:  # Macos
            url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-mac.zip"
            sha256 = "c99048673aa765960a99cf10c6ddb9f1fad506099ff0a0e137ad8960a88f321b"
        get(self, url=url, sha256=sha256, destination=self.build_folder, strip_root=False)

    def package(self):
        dst = os.path.join(self.package_folder, "bin")
        if str(self.settings.os) == "Windows":
            copy(self, "ninja.exe", src=self.build_folder, dst=dst)
        else:
            copy(self, "ninja", src=self.build_folder, dst=dst)
            import stat
            ninja_path = os.path.join(dst, "ninja")
            os.chmod(ninja_path, os.stat(ninja_path).st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        bin_dir = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
        self.env_info.PATH.append(bin_dir)
