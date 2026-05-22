from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
import os


class Recipe(RecipeBase):
    name = "minizip-ng"
    version = "4.2.1"
    license = "Zlib"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "mz_compatibility": [True, False],
        "with_zlib": [True, False],
        "with_bzip2": [True, False],
        "with_lzma": [True, False],
        "with_zstd": [True, False],
        "with_openssl": [True, False],
        "with_iconv": [True, False],
        "with_libbsd": [True, False],
        "with_libcomp": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "mz_compatibility": False,
        "with_zlib": True,
        "with_bzip2": True,
        "with_lzma": True,
        "with_zstd": True,
        "with_openssl": True,
        "with_iconv": True,
        "with_libbsd": True,
        "with_libcomp": True,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "bzip2", "xz_utils", "zstd", "openssl", "libiconv"]

    def source(self):
        get(
            url="https://github.com/zlib-ng/minizip-ng/archive/4.2.1.tar.gz",
            dest=self.source_folder,
            sha256="3cc35c2cb925dbe67cc801e3234b31b0f30197812a99377352fa1b551ab3d011",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MZ_FETCH_LIBS"] = False
        tc.cache_variables["MZ_COMPAT"] = self.options.mz_compatibility
        tc.cache_variables["MZ_ZLIB"] = self.options.get("with_zlib", False)
        tc.cache_variables["MZ_ZLIB_FLAVOR"] = "zlib"
        tc.cache_variables["MZ_BZIP2"] = self.options.with_bzip2
        tc.cache_variables["MZ_PPMD"] = False
        tc.cache_variables["MZ_LZMA"] = self.options.with_lzma
        tc.cache_variables["MZ_ZSTD"] = self.options.with_zstd
        tc.cache_variables["MZ_OPENSSL"] = self.options.with_openssl
        tc.cache_variables["MZ_LIBCOMP"] = self.options.get("with_libcomp", False)
        if self.settings.os != "Windows":
            tc.cache_variables["MZ_ICONV"] = self.options.with_iconv
            tc.cache_variables["MZ_LIBBSD"] = self.options.with_libbsd
        tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True

        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_PkgConfig"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
