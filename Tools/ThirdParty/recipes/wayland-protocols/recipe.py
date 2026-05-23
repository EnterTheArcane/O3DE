from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, replace_in_file, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "wayland-protocols"
    version = "1.45"
    license = "MIT"
    settings = "os", "arch", "compiler", "build_type"

    def build_requirements(self):
        self.tool_requires("meson/[>=1.3.1 <2]")

    def source(self):
        get(self, url="https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.45/downloads/wayland-protocols-1.45.tar.xz", sha256="4d2b2a9e3e099d017dc8107bf1c334d27bb87d9e4aff19a0c8d856d17cd41ef0", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = MesonToolchain(self)
        # Using relative folder because of this https://github.com/conan-io/conan/pull/15706
        tc.project_options["datadir"] = "res"
        tc.project_options["tests"] = "false"
        tc.generate()

    def _patch_sources(self):
        if Version(self.version) >= "1.42":
            replace_in_file(self, os.path.join(self.source_folder, "meson.build"),
                            "dep_scanner = dependency('wayland-scanner',",
                            "dep_scanner = dependency('wayland-scanner', required: false, disabler: true,")

    def build(self):
        self._patch_sources()
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rmdir(self, os.path.join(self.package_folder, "res", "pkgconfig"))

    def package_info(self):
        pkgconfig_variables = {
            'datarootdir': '${prefix}/res',
            'pkgdatadir': '${datarootdir}/wayland-protocols',
        }
        # TODO: Remove when Conan 1.x not supported
        pkgconfig_variables = pkgconfig_variables if conan_version.major >= 2 \
            else "\n".join(f"{key}={value}" for key, value in pkgconfig_variables.items())
        self.cpp_info.set_property("pkg_config_custom_content", pkgconfig_variables)
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        self.cpp_info.bindirs = []
