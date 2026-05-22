# Simplified libxml2 recipe using CMake build (2.14+)
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir


class Recipe(RecipeBase):
    name = "libxml2"
    version = "2.14.5"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "libiconv"]

    def source(self):
        get(
            url="https://download.gnome.org/sources/libxml2/2.14/libxml2-2.14.5.tar.xz",
            dest=self.source_folder,
            sha256="03d006f3537616833c16c53addcdc32a0eb20e55443cba4038307e3fa7d8d44b",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.cache_variables["LIBXML2_WITH_ICONV"] = True
        tc.cache_variables["LIBXML2_WITH_ICU"] = False
        tc.cache_variables["LIBXML2_WITH_LZMA"] = False
        tc.cache_variables["LIBXML2_WITH_PYTHON"] = False
        tc.cache_variables["LIBXML2_WITH_ZLIB"] = True
        tc.cache_variables["LIBXML2_WITH_LEGACY"] = False
        tc.cache_variables["LIBXML2_WITH_TESTS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "Copyright",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
