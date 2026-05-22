# Ported from conan-center-index/libiconv by port_recipe.py
# On Windows: uses win-iconv (a Windows API-based iconv implementation).
# On other platforms: uses the standard GNU libiconv with Autotools.

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, save
import os

_WIN_ICONV_URL = (
    "https://github.com/win-iconv/win-iconv/archive/refs/tags/v0.0.8.tar.gz"
)
_WIN_ICONV_SHA256 = "23adea990a8303c6e69e32a64a30171efcb1b73824a1c2da1bbf576b0ae7c520"


class Recipe(RecipeBase):
    name = "libiconv"
    version = "1.18"
    license = "LGPL-2.1-or-later"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def source(self):
        if self.is_windows:
            get(
                url=_WIN_ICONV_URL,
                dest=self.source_folder,
                sha256=_WIN_ICONV_SHA256,
                strip_root=True,
            )
        else:
            get(
                url="https://ftpmirror.gnu.org/gnu/libiconv/libiconv-1.18.tar.gz",
                dest=self.source_folder,
                sha256="3b08f5f4f9b4eb82f151a7040bfd6fe6c6fb922efe4b1659c66ea933276965e8",
            )

    def generate(self):
        if self.is_windows:
            tc = CMakeToolchain(self)
            tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
            tc.variables["BUILD_EXECUTABLE"] = False
            tc.generate()

    def build(self):
        if self.is_windows:
            cmake = CMake(self)
            cmake.configure()
            cmake.build()
        else:
            from thirdparty.tools.gnu import Autotools, AutotoolsToolchain

            tc = AutotoolsToolchain(self)
            tc.generate()
            autotools = Autotools(self)
            autotools.configure()
            autotools.make()

    def package(self):
        if self.is_windows:
            copy(
                "COPYING.LIB",
                src=self.source_folder,
                dst=os.path.join(self.package_folder, "licenses"),
            )
            cmake = CMake(self)
            cmake.install()
        else:
            from thirdparty.tools.gnu import Autotools
            from thirdparty.tools.files import rmdir, rm
            from thirdparty.tools.apple import fix_apple_shared_install_name

            copy(
                "COPYING.LIB",
                self.source_folder,
                os.path.join(self.package_folder, "licenses"),
            )
            autotools = Autotools(self)
            autotools.install()
            rm("*.la", os.path.join(self.package_folder, "lib"))
            rmdir(os.path.join(self.package_folder, "share"))
            fix_apple_shared_install_name(self)

    @property
    def _is_clang_cl(self):
        return False  # simplified
