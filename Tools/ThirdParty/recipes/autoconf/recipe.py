import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rmdir, replace_in_file, apply_patches
from thirdparty.autotools import Autotools, AutotoolsToolchain
from thirdparty.microsoft import unix_path, is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "autoconf"
    version = "2.72"
    license = "GPL-2.0-or-later", "GPL-3.0-or-later"

    def requirements(self):
        self.requires("m4")  # Needed at runtime by downstream clients as well
        self.tool_requires("m4")
        if self.settings.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

    def latest_version(self):
        repo = GithubRepository(self, "autotools-mirror/autoconf")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://ftpmirror.gnu.org/autoconf/autoconf-2.72.tar.xz",
            sha256="ba885c1319578d6c94d46e9b0dceb4014caafe2490e437a0dbca3f270a223f5a",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        tc.configure_args.extend(
            [
                "--datarootdir=${prefix}/res",
            ])

        if self.settings.os == "Windows":
            if is_msvc(self):
                build = "{}-{}-{}".format(
                    "x86_64" if self.settings.arch == "X64" else "i686",
                    "pc" if self.settings.arch == "x86" else "win64",
                    "mingw32")
                host = "{}-{}-{}".format(
                    "x86_64" if self.settings.arch == "X64" else "i686",
                    "pc" if self.settings.arch == "x86" else "win64",
                    "mingw32")
                tc.configure_args.append(f"--build={build}")
                tc.configure_args.append(f"--host={host}")

        env = tc.environment()
        env.define_path("INSTALL", unix_path(self, os.path.join(self.folders.source, "build-aux", "install-sh")))
        tc.generate(env)

    def build(self):
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        autotools = Autotools(self)
        autotools.install()

        copy(self, "COPYING*", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        rmdir(self, os.path.join(self.folders.package, "res", "info"))
        rmdir(self, os.path.join(self.folders.package, "res", "man"))

        autom4te_cfg = os.path.join(self.folders.package, "res", "autoconf", "autom4te.cfg")
        if self.settings.os == "Windows":
            actual_data_path = unix_path(self, os.path.join(self.folders.package, "res", "autoconf"))
            replace_in_file(self, autom4te_cfg, "'/res/autoconf'", f"'{actual_data_path}'")
        else:
            actual_data_path = os.path.join(self.folders.package, "res", "autoconf")
            replace_in_file(self, autom4te_cfg, "'//res/autoconf'", f"'{actual_data_path}'")

    def package_info(self):
        self.info.frameworkdirs = []
        self.info.libdirs = []
        self.info.includedirs = []
        self.info.resdirs = ["res"]

        bin_path = os.path.join(self.folders.package, "bin")
        self.buildenv_info.define_path("AUTOCONF", os.path.join(bin_path, "autoconf"))
        self.buildenv_info.define_path("AUTORECONF", os.path.join(bin_path, "autoreconf"))
        self.buildenv_info.define_path("AUTOHEADER", os.path.join(bin_path, "autoheader"))
        self.buildenv_info.define_path("AUTOM4TE", os.path.join(bin_path, "autom4te"))

        perllib_path = os.path.join(self.folders.package, "res", "autoconf")
        self.buildenv_info.define_path("autom4te_perllibdir", perllib_path)
        self.buildenv_info.define_path("AC_MACRODIR", perllib_path)
        self.buildenv_info.define_path("trailer_m4", os.path.join(perllib_path, "autoconf", "trailer.m4"))
