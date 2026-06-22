import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "tinyxml2"
    version = "11.0.0"
    license = "Zlib"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def latest_version(self):
        repo = GithubRepository(self, "leethomason/tinyxml2")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/leethomason/tinyxml2/archive/refs/tags/11.0.0.tar.gz",
            sha256="5556deb5081fb246ee92afae73efd943c889cef0cafea92b0b82422d6a18f289",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "tinyxml2")
        self.cpp_info.set_property("cmake_target_name", "tinyxml2::tinyxml2")
        self.cpp_info.set_property("pkg_config_name", "tinyxml2")
        postfix = ""
        self.cpp_info.libs = [f"tinyxml2{postfix}"]
        if self.settings.os == "Windows" and self.options.shared:
            self.cpp_info.defines.append("TINYXML2_IMPORT")
