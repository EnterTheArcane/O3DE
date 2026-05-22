# Simplified libcurl recipe for Windows (schannel + zlib only)
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir


class Recipe(RecipeBase):
    name = "libcurl"
    version = "8.20.0"
    license = "curl"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "zstd"]

    def source(self):
        get(
            url="https://curl.se/download/curl-8.20.0.tar.xz",
            dest=self.source_folder,
            sha256="63fe2dc148ba0ceae89922ef838f7e5c946272c2e78b7c59fab4b79d3ce2b896",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_CURL_EXE"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["CURL_STATICLIB"] = not self.options.shared
        tc.variables["CURL_USE_SCHANNEL"] = True
        tc.variables["CURL_USE_OPENSSL"] = False
        tc.variables["CURL_USE_WOLFSSL"] = False
        tc.variables["CURL_USE_MBEDTLS"] = False
        tc.variables["CURL_ZLIB"] = True
        tc.variables["CURL_BROTLI"] = False
        tc.variables["CURL_ZSTD"] = True
        tc.variables["ZSTD_USE_STATIC_LIBS"] = (
            True  # our zstd package only has zstd_static.lib
        )
        tc.variables["CURL_DISABLE_LDAP"] = True
        tc.variables["ENABLE_ARES"] = False
        tc.variables["USE_LIBIDN2"] = False
        tc.variables["CURL_USE_LIBPSL"] = False
        tc.variables["CURL_USE_LIBSSH2"] = False
        tc.variables["USE_NGHTTP2"] = False
        tc.variables["CMAKE_DEBUG_POSTFIX"] = ""
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "COPYING",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "res"))
        rmdir(os.path.join(self.package_folder, "share"))
