import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeConfigDeps, CMakeToolchain
from thirdparty.files import copy, get, rmdir, save
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "ptex"
    version = "2.5.2"
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
        self.requires("zlib")
        self.requires("libdeflate")

    def latest_version(self):
        repo = GithubRepository(self, "wdas/ptex")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/wdas/ptex/archive/refs/tags/v2.5.2.tar.gz",
            sha256="dd95fbea4b50e9e68fd042f540fb83157a0ff25053066c3439d4527de3621d34",
            destination=self.source_folder,
            strip_root=True)
        save(self, os.path.join(self.source_folder, "src", "utils", "CMakeLists.txt"), "")
        save(self, os.path.join(self.source_folder, "src", "tests", "CMakeLists.txt"), "")
        save(self, os.path.join(self.source_folder, "src", "doc", "CMakeLists.txt"), "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PTEX_BUILD_STATIC_LIBS"] = not self.options.shared
        tc.variables["PTEX_BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()
        cd = CMakeConfigDeps(self)
        cd.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        cmake_target = "Ptex_dynamic" if self.options.shared else "Ptex_static"
        self.cpp_info.set_property("cmake_file_name", "ptex")
        self.cpp_info.set_property("cmake_target_name", f"Ptex::{cmake_target}")
        self.cpp_info.components["_ptex"].libs = ["Ptex"]
        if not self.options.shared:
            self.cpp_info.components["_ptex"].defines.append("PTEX_STATIC")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["_ptex"].system_libs.append("pthread")
        self.cpp_info.components["_ptex"].requires = ["zlib::zlib", "libdeflate::_libdeflate"]

        self.cpp_info.components["_ptex"].set_property("cmake_target_name", f"Ptex::{cmake_target}")
