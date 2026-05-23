import os

from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rmdir
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "md4c"
    version = "0.5.2"
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
        #"md2html": True,  # conditional default value in config_options
        "encoding": "utf-8",
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if Version(self.version) >= "0.5.0":
            # Set it to false for iOS, tvOS, watchOS, visionOS
            # to prevent cmake from creating a bundle for the md2html executable
            is_ios_variant = is_apple_os(self) and not self.settings.os == "Macos"
            self.options.md2html = not is_ios_variant
        else:
            # md2html was introduced in 0.5.0
            del self.options.md2html

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def latest_version(self):
        repo = GithubRepository(self, "mity/md4c")
        return Version(repo.latest_release.removeprefix("release-"))

    def source(self):
        get(
            self,
            url="https://github.com/mity/md4c/archive/refs/tags/release-0.5.2.tar.gz",
            sha256="55d0111d48fb11883aaee91465e642b8b640775a4d6993c2d0e7a8092758ef21",
            destination=self.source_folder,
            strip_root=True)
        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_MD2HTML_EXECUTABLE"] = self.options.get_safe("md2html", True)
        if self.options.encoding == "utf-8":
            tc.preprocessor_definitions["MD4C_USE_UTF8"] = "1"
        elif self.options.encoding == "utf-16":
            tc.preprocessor_definitions["MD4C_USE_UTF16"] = "1"
        elif self.options.encoding == "ascii":
            tc.preprocessor_definitions["MD4C_USE_ASCII"] = "1"
        if Version(self.version) < "0.5.0":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"  # CMake 4 support
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # Honor encoding option
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            "COMPILE_FLAGS \"-DMD4C_USE_UTF8\"",
            "",
        )

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE.md", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "md4c")

        self.cpp_info.components["_md4c"].set_property("cmake_target_name", "md4c::md4c")
        self.cpp_info.components["_md4c"].set_property("pkg_config_name", "md4c")
        self.cpp_info.components["_md4c"].libs = ["md4c"]
        if self.settings.os == "Windows" and self.options.encoding == "utf-16":
            self.cpp_info.components["_md4c"].defines.append("MD4C_USE_UTF16")

        self.cpp_info.components["md4c_html"].set_property("cmake_target_name", "md4c::md4c-html")
        self.cpp_info.components["md4c_html"].set_property("pkg_config_name", "md4c-html")
        self.cpp_info.components["md4c_html"].libs = ["md4c-html"]
        self.cpp_info.components["md4c_html"].requires = ["_md4c"]

        # workaround so that global target & pkgconfig file have all components while avoiding
        # to create unofficial target or pkgconfig file
        self.cpp_info.set_property("cmake_target_name", "md4c::md4c-html")
        self.cpp_info.set_property("pkg_config_name", "md4c-html")
