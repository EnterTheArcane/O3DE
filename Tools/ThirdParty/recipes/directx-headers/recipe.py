from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "directx-headers"
    version = "1.618.2"
    license = "MIT"

    @property
    def _min_cppstd(self):
        return 11

    @property
    def _compilers_minimum_version(self):
        return {
            "apple-clang": "10",
            "clang": "5",
            "gcc": "6",
            "msvc": "191",
            "Visual Studio": "15",
        }

    def source(self):
        get(
            url="https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.618.2.tar.gz",
            dest=self.source_folder,
            sha256="62004f45e2ab00cbb5c7f03c47262632c22fbce0a237383fc458d9324c44cf36",
        )

    def generate(self):
        tc = MesonToolchain(self)
        tc.project_options["build-test"] = False
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(
            "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses")
        )
        meson = Meson(self)
        meson.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
