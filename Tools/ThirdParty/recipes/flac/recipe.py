import os

from thirdparty import RecipeBase
from thirdparty.apple import is_apple_os
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "flac"
    version = "1.5.0"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self):
        self.requires("ogg")

    def latest_version(self):
        repo = GithubRepository(self, "xiph/flac")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/xiph/flac/releases/download/1.5.0/flac-1.5.0.tar.xz",
            sha256="f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_DOCS"] = False
        tc.variables["BUILD_PROGRAMS"] = not is_apple_os(self) or self.settings.os == "Mac"
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_CXXLIBS"] = True
        tc.generate()
        cd = CMakeDeps(self)
        cd.generate()

    def build(self):
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "share", "getopt", "CMakeLists.txt"),
            "find_package(Intl)",
            "")
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            self, "COPYING.*", src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"), keep_path=False)
        copy(
            self, "*.h", src=os.path.join(self.source_folder, "include", "share"),
            dst=os.path.join(self.package_folder, "include", "share"), keep_path=False)
        rmdir(self, os.path.join(self.package_folder, "share"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "flac")

        self.cpp_info.components["libflac"].set_property("cmake_target_name", "FLAC::FLAC")
        self.cpp_info.components["libflac"].libs = ["FLAC"]
        self.cpp_info.components["libflac"].requires = ["ogg::ogg"]

        self.cpp_info.components["libflac++"].set_property("cmake_target_name", "FLAC::FLAC++")
        self.cpp_info.components["libflac++"].libs = ["FLAC++"]
        self.cpp_info.components["libflac++"].requires = ["libflac"]

        if not self.options.shared:
            self.cpp_info.components["libflac"].defines = ["FLAC__NO_DLL"]
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["libflac"].system_libs += ["m"]
