from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.files import copy, download, save
import os


class Recipe(ConanFile):
    name = "meson"
    version = "1.11.1"
    license = "Apache-2.0"
    package_type = "application"
    settings = "os", "arch"

    def source(self):
        pass

    def build(self):
        download(self,
                 url="https://github.com/mesonbuild/meson/releases/download/1.11.1/meson.pyz",
                 sha256="05f25d74eab0d1a9b26f1d2fe482677b9c2b9543f974016c8102a5f96b2b73f5",
                 filename=os.path.join(self.build_folder, "meson.pyz"))

    def package(self):
        dst = os.path.join(self.package_folder, "bin")
        if str(self.settings.os) == "Windows":
            # meson.pyz is a Python zipapp with a #!/usr/bin/env python3 shebang.
            # py.exe (Python Launcher, always at C:\Windows\py.exe) reads the shebang
            # and picks the correct Python — no hardcoded interpreter path needed.
            copy(self, "meson.pyz", src=self.build_folder, dst=dst)
            save(self, os.path.join(dst, "meson.cmd"),
                 '@py "%~dp0meson.pyz" %*\n')
        else:
            # On Unix the shebang is honoured directly. Copy as "meson" (no extension),
            # make executable, and it runs without any wrapper script.
            import shutil
            import stat
            os.makedirs(dst, exist_ok=True)
            meson_path = os.path.join(dst, "meson")
            shutil.copy2(os.path.join(self.build_folder, "meson.pyz"), meson_path)
            os.chmod(meson_path, os.stat(meson_path).st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        bin_dir = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
        self.env_info.PATH.append(bin_dir)
