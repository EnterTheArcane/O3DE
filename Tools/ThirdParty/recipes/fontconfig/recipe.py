import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.pkgconfig import PkgConfigDeps
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "fontconfig"
    version = "2.18.0"
    license = "MIT"

    def latest_version(self):
        repo = GitlabRepository(self, "fontconfig/fontconfig", host="gitlab.freedesktop.org")
        return Version(repo.latest_formal_release.removeprefix("v"))

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        self.requires("freetype")
        self.requires("expat")
        self.requires_tool("gperf")
        self.requires_tool("meson")
        if not self.conf.get("tools.gnu:pkg_config", default=False, check_type=str):
            self.requires_tool("pkgconf")

    def source(self):
        get(
            self,
            url="https://gitlab.freedesktop.org/api/v4/projects/890/packages/generic/fontconfig/2.18.0/fontconfig-2.18.0.tar.xz",
            sha256="e7064a4725431ddba06ff8b971ec5a4b422e23b0169ce215747beedcb30e9073",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        VirtualBuildEnv(self).generate()

        deps = PkgConfigDeps(self)
        deps.generate()

        tc = MesonToolchain(self)
        tc.project_options.update(
            {
                "doc": "disabled",
                "nls": "disabled",
                "tests": "disabled",
                "tools": "disabled",
                "sysconfdir": os.path.join("res", "etc"),
                "datadir": os.path.join("res", "share"),
            })
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", self.folders.source, self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rm(self, "*.pdb", self.folders.package, recursive=True)
        rm(self, "*.conf", self.folders.package / "res" / "etc" / "fonts" / "conf.d")
        rm(self, "*.def", self.folders.package / "lib")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.info.set_property("cmake_file_name", "Fontconfig")
        self.info.set_property("cmake_target_name", "Fontconfig::Fontconfig")
        self.info.set_property("pkg_config_name", "fontconfig")
        self.info.libs = ["fontconfig"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.info.system_libs.extend(["m", "pthread"])

        fontconfig_path = self.folders.package / "res" / "etc" / "fonts"
        self.runenv_info.append_path("FONTCONFIG_PATH", fontconfig_path)
