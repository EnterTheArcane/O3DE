import os

from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.files import copy, get, rm, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain

class Recipe(RecipeBase):
    name = "lcms"
    version = "2.17"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def build_requirements(self):
        self.tool_requires("meson")

    def source(self):
        get(self, url="https://github.com/mm2/Little-CMS/releases/download/lcms2.17/lcms2-2.17.tar.gz", sha256="d11af569e42a1baa1650d20ad61d12e41af4fead4aa7964a01f93b08b53ab074", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = MesonToolchain(self)
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.set_property("pkg_config_name", "lcms2")
        self.cpp_info.libs = ["lcms2"]
        if self.settings.os == "Windows" and self.options.shared:
            self.cpp_info.defines.append("CMS_DLL")
        if self.settings.os in ("FreeBSD", "Linux"):
            self.cpp_info.system_libs.extend(["m", "pthread"])
