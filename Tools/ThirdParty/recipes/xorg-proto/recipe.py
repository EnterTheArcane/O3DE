from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rmdir
from thirdparty.meson import Meson, MesonToolchain

# Header-only protocol packages shipped by xorg-proto. Each installs an <name>.pc so downstream
# X.Org libraries (libX11, libxcb, libXrandr, ...) can find the matching headers via pkg-config.
_PROTOS = [
    "applewmproto",
    "bigreqsproto",
    "compositeproto",
    "damageproto",
    "dmxproto",
    "dpmsproto",
    "dri2proto",
    "dri3proto",
    "fixesproto",
    "fontsproto",
    "glproto",
    "inputproto",
    "kbproto",
    "presentproto",
    "randrproto",
    "recordproto",
    "renderproto",
    "resourceproto",
    "scrnsaverproto",
    "videoproto",
    "xcmiscproto",
    "xextproto",
    "xf86bigfontproto",
    "xf86dgaproto",
    "xf86driproto",
    "xf86vidmodeproto",
    "xineramaproto",
    "xproto",
]


class Recipe(RecipeBase):
    name = "xorg-proto"
    version = "2024.1"
    license = "MIT"

    def configure(self):
        self.settings.compiler_cxx_standard = None
        self.settings.compiler_libcxx = None

    def validate(self):
        from thirdparty.errors import RecipeInvalidConfiguration
        if self.settings.os not in ("Linux", "FreeBSD", "Android"):
            raise RecipeInvalidConfiguration(f"{self.name} is only supported on Linux-like platforms")

    def requirements(self):
        self.requires_tool("meson")
        if not self.conf.tools.gnu.pkg_config:
            self.requires_tool("pkgconf")

    def source(self):
        get(
            self,
            url=f"https://www.x.org/releases/individual/proto/xorgproto-{self.version}.tar.xz",
            sha256="372225fd40815b8423547f5d890c5debc72e88b91088fbfb13158c20495ccb59",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        VirtualBuildEnv(self).generate()
        tc = MesonToolchain(self)
        tc.project_options["libdir"] = "lib"
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", src=self.folders.source, dst=self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "share")

    def package_info(self):
        self.info.bindirs = []
        self.info.libdirs = []
        # One header-only component per protocol, each exposing its <name>.pc.
        for proto in _PROTOS:
            comp = self.info.components[proto]
            comp.set_property("pkg_config_name", proto)
            comp.set_property("component_version", self.version)
            comp.includedirs = ["include"]
            comp.libdirs = []
            comp.bindirs = []
