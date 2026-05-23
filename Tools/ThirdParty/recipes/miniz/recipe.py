import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "miniz"
    version = "3.1.1"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def latest_version(self):
        repo = GithubRepository(self, "richgel999/miniz")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/richgel999/miniz/archive/refs/tags/3.1.1.tar.gz",
            sha256="8bb29c7bd6f22356e5583e794bed4a0b3e6dfcbcadb49974fc9270ccca1e5557",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_FUZZERS"] = False
        tc.variables["AMALGAMATE_SOURCES"] = False
        tc.variables["BUILD_HEADER_ONLY"] = False
        tc.variables["INSTALL_PROJECT"] = True
        tc.cache_variables["BUILD_TESTS"] = False
        # Honor BUILD_SHARED_LIBS from conan_toolchain (see https://github.com/conan-io/conan/issues/11840)
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "miniz")
        self.cpp_info.set_property("cmake_target_name", "miniz::miniz")
        self.cpp_info.set_property("pkg_config_name", "miniz")
        self.cpp_info.libs = ["miniz"]
        self.cpp_info.includedirs.append(os.path.join("include", "miniz"))
