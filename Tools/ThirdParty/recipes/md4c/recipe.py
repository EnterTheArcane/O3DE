import os

from thirdparty import RecipeBase
from thirdparty.apple import is_apple_os
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "md4c"
    version = "0.5.3"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "md2html": [True, False],
        "encoding": ["utf-8", "utf-16", "ascii"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "encoding": "utf-8",
    }

    def config_options(self):
        # Set it to false for iOS, tvOS, watchOS, visionOS
        # to prevent cmake from creating a bundle for the md2html executable
        is_ios_variant = is_apple_os(self) and not self.settings.os == "Mac"
        self.options.md2html = not is_ios_variant

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def latest_version(self):
        repo = GithubRepository(self, "mity/md4c")
        return Version(repo.latest_release.removeprefix("release-"))

    def source(self):
        get(
            self,
            url="https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.tar.gz",
            sha256="353c346f376b87c954a13f3415ede2d51264cc61dc5abcd38ff1d2aa0d059b9e",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_MD2HTML_EXECUTABLE"] = self.options.get_safe("md2html", True)
        if self.options.encoding == "utf-8":
            tc.preprocessor_definitions["MD4C_USE_UTF8"] = "1"
        elif self.options.encoding == "utf-16":
            tc.preprocessor_definitions["MD4C_USE_UTF16"] = "1"
        elif self.options.encoding == "ascii":
            tc.preprocessor_definitions["MD4C_USE_ASCII"] = "1"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE.md", dst=os.path.join(self.folders.package, "licenses"), src=self.folders.source)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "md4c")

        self.info.components["_md4c"].set_property("cmake_target_name", "md4c::md4c")
        self.info.components["_md4c"].set_property("pkg_config_name", "md4c")
        self.info.components["_md4c"].libs = ["md4c"]
        if self.settings.os == "Windows" and self.options.encoding == "utf-16":
            self.info.components["_md4c"].defines.append("MD4C_USE_UTF16")

        self.info.components["md4c_html"].set_property("cmake_target_name", "md4c::md4c-html")
        self.info.components["md4c_html"].set_property("pkg_config_name", "md4c-html")
        self.info.components["md4c_html"].libs = ["md4c-html"]
        self.info.components["md4c_html"].requires = ["_md4c"]

        # workaround so that global target & pkgconfig file have all components while avoiding
        # to create unofficial target or pkgconfig file
        self.info.set_property("cmake_target_name", "md4c::md4c-html")
        self.info.set_property("pkg_config_name", "md4c-html")
