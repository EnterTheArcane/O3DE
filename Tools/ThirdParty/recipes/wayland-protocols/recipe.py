import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class Recipe(RecipeBase):
    name = "wayland-protocols"
    version = "1.48"
    license = "MIT"

    def latest_version(self):
        repo = GitlabRepository(self, "wayland/wayland-protocols", host="gitlab.freedesktop.org")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.48/downloads/wayland-protocols-1.48.tar.xz",
            sha256="398036ac0eb6484982ddbde7ff86848d753231f9cdeeae983f06b52946625aa1",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(
            self,
            os.path.join(self.folders.source, "meson.build"),
            "dep_scanner = dependency('wayland-scanner',",
            "dep_scanner = dependency('wayland-scanner', required: false, disabler: true,")

    def generate(self):
        tc = MesonToolchain(self)
        # Using relative folder because of this upstream PR 15706
        tc.project_options["datadir"] = "res"
        tc.project_options["tests"] = "false"
        tc.generate()
        env = VirtualBuildEnv(self)
        env.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", self.folders.source, os.path.join(self.folders.package, "licenses"))
        meson = Meson(self)
        meson.install()
        rmdir(self, os.path.join(self.folders.package, "res", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        self.cpp_info.bindirs = []
