from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class Recipe(RecipeBase):
    name = "wayland-protocols"
    version = "1.49"
    license = "MIT"

    def latest_version(self):
        repo = GitlabRepository(self, "wayland/wayland-protocols", host="gitlab.freedesktop.org")
        return Version(repo.latest_release)

    def requirements(self):
        self.requires_tool("meson")

    def source(self):
        get(
            self,
            url="https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.49/downloads/wayland-protocols-1.49.tar.xz",
            sha256="04af1d372970b0ce9330d3bb9a821e0ebf50a669c3e5e1181b2459dc6becbfd0",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(
            self,
            self.folders.source / "meson.build",
            "dep_scanner = dependency('wayland-scanner',",
            "dep_scanner = dependency('wayland-scanner', required: false, disabler: true,")

    def generate(self):
        tc = MesonToolchain(self)
        # Using relative folder because of this upstream PR 15706
        tc.project_options["datadir"] = "res"
        tc.project_options["tests"] = "false"
        tc.generate()
        VirtualBuildEnv(self).generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", self.folders.source, self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rmdir(self, self.folders.package / "res" / "pkgconfig")

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []
        self.info.bindirs = []
