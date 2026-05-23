from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import collect_libs, copy, get, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "libdeflate"
    version = "1.25"
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
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def latest_version(self):
        repo = GithubRepository(self, "ebiggers/libdeflate")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/ebiggers/libdeflate/archive/refs/tags/v1.25.tar.gz",
            sha256="d11473c1ad4c57d874695e8026865e38b47116bbcb872bfc622ec8f37a86017d",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LIBDEFLATE_BUILD_STATIC_LIB"] = not self.options.shared
        tc.variables["LIBDEFLATE_BUILD_SHARED_LIB"] = self.options.shared
        tc.variables["LIBDEFLATE_BUILD_GZIP"] = False
        tc.variables["LIBDEFLATE_BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "libdeflate")
        target_suffix = "_shared" if self.options.shared else "_static"
        self.cpp_info.set_property("cmake_target_name", f"libdeflate::libdeflate{target_suffix}")
        self.cpp_info.set_property("cmake_target_aliases", ["libdeflate::libdeflate"]) # not official, avoid to break users
        self.cpp_info.set_property("pkg_config_name", "libdeflate")
        self.cpp_info.components["_libdeflate"].libs = collect_libs(self)
        if self.settings.os == "Windows" and self.options.shared:
            self.cpp_info.components["_libdeflate"].defines.append("LIBDEFLATE_DLL")

        self.cpp_info.components["_libdeflate"].set_property("cmake_target_name", f"libdeflate::libdeflate{target_suffix}")
        self.cpp_info.components["_libdeflate"].set_property("pkg_config_name", "libdeflate")
