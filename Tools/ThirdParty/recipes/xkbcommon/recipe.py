from thirdparty import RecipeBase, RecipeOptions
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rmdir
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.pkgconfig import PkgConfigDeps


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True
    with_x11: bool = True
    with_wayland: bool = False


class Recipe(RecipeBase[_Options]):
    name = "xkbcommon"
    version = "1.7.0"
    license = "MIT"

    def configure(self):
        self.settings.compiler_cxx_standard = None
        self.settings.compiler_libcxx = None

    def validate(self):
        from thirdparty.errors import RecipeInvalidConfiguration
        if self.settings.os not in ("Linux", "FreeBSD", "Android"):
            raise RecipeInvalidConfiguration(f"{self.name} is only supported on Linux-like platforms")

    def requirements(self):
        if self.options.with_x11:
            self.requires("libxcb")
        self.requires_tool("meson")
        self.requires_tool("bison")
        if not self.conf.tools.gnu.pkg_config:
            self.requires_tool("pkgconf")

    def source(self):
        get(
            self,
            url=f"https://xkbcommon.org/download/libxkbcommon-{self.version}.tar.xz",
            sha256="65782f0a10a4b455af9c6baab7040e2f537520caa2ec2092805cdfd36863b247",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        VirtualBuildEnv(self).generate()
        PkgConfigDeps(self).generate()
        tc = MesonToolchain(self)
        tc.project_options["libdir"] = "lib"
        tc.project_options["enable-x11"] = self.options.with_x11
        tc.project_options["enable-wayland"] = self.options.with_wayland
        tc.project_options["enable-docs"] = False
        tc.project_options["enable-xkbregistry"] = False
        tc.project_options["enable-tools"] = False
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        self.info.components["xkbcommon"].set_property("pkg_config_name", "xkbcommon")
        self.info.components["xkbcommon"].libs = ["xkbcommon"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.info.components["xkbcommon"].system_libs = ["m"]

        if self.options.with_x11:
            self.info.components["libxkbcommon-x11"].set_property("pkg_config_name", "xkbcommon-x11")
            self.info.components["libxkbcommon-x11"].libs = ["xkbcommon-x11"]
            self.info.components["libxkbcommon-x11"].requires = ["xkbcommon", "libxcb::xcb", "libxcb::xkb"]
