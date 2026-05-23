from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_conandata_patches, copy, get, rmdir, save
import os

from thirdparty.tools.gnu import PkgConfigDeps

class Recipe(RecipeBase):
    name = "ptex"
    version = "2.4.2"
    license = "BSD-3-Clause"

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

    def requirements(self):
        self.requires("zlib")

    def source(self):
        get(self, url="https://github.com/wdas/ptex/archive/refs/tags/v2.4.2.tar.gz", sha256="c8235fb30c921cfb10848f4ea04d5b662ba46886c5e32ad5137c5086f3979ee1", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PTEX_BUILD_STATIC_LIBS"] = not self.options.shared
        tc.variables["PTEX_BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()
        cd = CMakeDeps(self)
        cd.generate()

    def _patch_sources(self):
        apply_conandata_patches(self)
        # disable subdirs
        save(self, os.path.join(self.source_folder, "src", "utils", "CMakeLists.txt"), "")
        save(self, os.path.join(self.source_folder, "src", "tests", "CMakeLists.txt"), "")
        save(self, os.path.join(self.source_folder, "src", "doc", "CMakeLists.txt"), "")

    def build(self):
        self._patch_sources()
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
        self.cpp_info.components["_ptex"].requires = ["zlib::zlib"]

        self.cpp_info.components["_ptex"].set_property("cmake_target_name", f"Ptex::{cmake_target}")
