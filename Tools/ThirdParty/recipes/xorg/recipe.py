from thirdparty import RecipeBase
from thirdparty.tools.gnu import PkgConfig


_COMPONENTS = [
    "fontenc",
    "ice",
    "sm",
    "x11-xcb",
    "x11",
    "xau",
    "xaw7",
    "xcb-atom",
    "xcb-aux",
    "xcb-composite",
    "xcb-cursor",
    "xcb-dri2",
    "xcb-dri3",
    "xcb-dri3",
    "xcb-event",
    "xcb-ewmh",
    "xcb-glx",
    "xcb-icccm",
    "xcb-image",
    "xcb-keysyms",
    "xcb-present",
    "xcb-randr",
    "xcb-render",
    "xcb-renderutil",
    "xcb-res",
    "xcb-shape",
    "xcb-shm",
    "xcb-sync",
    "xcb-util",
    "xcb-xfixes",
    "xcb-xinerama",
    "xcb-xkb",
    "xcb",
    "xcomposite",
    "xcursor",
    "xdamage",
    "xdmcp",
    "xext",
    "xfixes",
    "xi",
    "xinerama",
    "xkbfile",
    "xmu",
    "xmuu",
    "xpm",
    "xrandr",
    "xrender",
    "xres",
    "xscrnsaver",
    "xt",
    "xtst",
    "xv",
    "xxf86vm",
]


class Recipe(RecipeBase):
    name = "xorg"
    version = "0.0.0"
    license = "MIT"

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        
        components = _COMPONENTS + ([] if self.settings.os == "FreeBSD" else ["uuid"])

        for name in components:
            pkg_config = PkgConfig(self, name)
            pkg_config.fill_cpp_info(self.cpp_info.components[name], is_system=self.settings.os != "FreeBSD")
            self.cpp_info.components[name].version = pkg_config.version
            self.cpp_info.components[name].set_property("pkg_config_name", name)
            self.cpp_info.components[name].set_property("component_version", pkg_config.version)
            self.cpp_info.components[name].bindirs = []
            self.cpp_info.components[name].includedirs = []
            self.cpp_info.components[name].libdirs = []
            self.cpp_info.components[name].set_property(
                "pkg_config_custom_content",
                "\n".join(f"{key}={value}" for key, value in pkg_config.variables.items() if key not in ["pcfiledir","prefix", "includedir"]))

        if self.settings.os == "Linux":
            self.cpp_info.components["sm"].requires.append("uuid")
