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

    def source(self):
        get(
            url="https://github.com/mm2/Little-CMS/releases/download/lcms2.17/lcms2-2.17.tar.gz",
            dest=self.source_folder,
            sha256="d11af569e42a1baa1650d20ad61d12e41af4fead4aa7964a01f93b08b53ab074",
        )

    def generate(self):
        tc = MesonToolchain(self)
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        meson = Meson(self)
        meson.install()
        rm("*.pdb", os.path.join(self.package_folder, "bin"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        fix_apple_shared_install_name(self)
