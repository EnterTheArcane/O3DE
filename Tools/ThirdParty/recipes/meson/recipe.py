import os

from thirdparty import RecipeBase
from thirdparty.files import copy, download, save
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "meson"
    version = "1.11.1"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "mesonbuild/meson")
        return Version(repo.latest_release)

    def build(self):
        download(
            self,
            url="https://github.com/mesonbuild/meson/releases/download/1.11.1/meson.pyz",
            sha256="05f25d74eab0d1a9b26f1d2fe482677b9c2b9543f974016c8102a5f96b2b73f5",
            filename=self.folders.build / "meson.pyz")

    def package(self):
        dst = self.folders.package / "bin"
        if str(self.settings.os) == "Windows":
            # meson.pyz is a Python zipapp. We must NOT invoke it directly: meson's
            # set_meson_command() checks whether sys.argv[0] ends with '.py' to decide
            # whether to prepend `python.exe` to the regenerate rule in build.ninja.
            # If we pass meson.pyz directly, ninja's REGENERATE_BUILD rule becomes
            # just ["meson.pyz", ...] which CreateProcess cannot execute.
            #
            # Instead we create a thin meson.py launcher that adds meson.pyz to
            # sys.path and calls mesonmain.  sys.argv[0] then ends with '.py', so
            # meson stores [python.exe, meson.py] as the command — executable by ninja.
            copy(self, "meson.pyz", src=self.folders.build, dst=dst)
            save(
                self, dst / "meson.py",
                "import sys, os\n"
                'sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "meson.pyz"))\n'
                "from mesonbuild import mesonmain\n"
                "sys.exit(mesonmain.main())\n")
            save(
                self, dst / "meson.cmd",
                '@python "%~dp0meson.py" %*\n')
        else:
            # On Unix the shebang is honoured directly. Copy as "meson" (no extension),
            # make executable, and it runs without any wrapper script.
            import shutil
            import stat
            os.makedirs(dst, exist_ok=True)
            meson_path = dst / "meson"
            shutil.copy2(self.folders.build / "meson.pyz", meson_path)
            os.chmod(meson_path, os.stat(meson_path).st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []
        bin_dir = self.folders.package / "bin"
        self.buildenv_info.prepend_path("PATH", bin_dir)
