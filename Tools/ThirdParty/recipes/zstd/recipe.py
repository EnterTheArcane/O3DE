import glob
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, collect_libs, copy, get, replace_in_file, rmdir, rm
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "zstd"
    version = "1.5.7"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "threading": [True, False],
        "build_programs": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "threading": True,
        "build_programs": True,
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
        repo = GithubRepository(self, "facebook/zstd")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz",
            sha256="eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ZSTD_BUILD_PROGRAMS"] = self.options.build_programs
        tc.variables["ZSTD_BUILD_STATIC"] = not self.options.shared or self.options.build_programs
        tc.variables["ZSTD_BUILD_SHARED"] = self.options.shared
        tc.variables["ZSTD_MULTITHREAD_SUPPORT"] = self.options.threading
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # Don't force PIC
        replace_in_file(self, os.path.join(self.source_folder, "build", "cmake", "lib", "CMakeLists.txt"),
                              "POSITION_INDEPENDENT_CODE On", "")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "build", "cmake"))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

        if self.options.shared and self.options.build_programs:
            # If we build programs we have to build static libs (see logic in generate()),
            # but if shared is True, we only want shared lib in package folder.
            rm(self, "*_static.*", os.path.join(self.package_folder, "lib"))
            for lib in glob.glob(os.path.join(self.package_folder, "lib", "*.a")):
                if not lib.endswith(".dll.a"):
                    os.remove(lib)

    def package_info(self):
        zstd_cmake = "libzstd_shared" if self.options.shared else "libzstd_static"
        self.cpp_info.set_property("cmake_file_name", "zstd")
        self.cpp_info.set_property("cmake_target_name", f"zstd::{zstd_cmake}")
        self.cpp_info.set_property("pkg_config_name", "libzstd")
        self.cpp_info.set_property("cmake_target_aliases", ["zstd::libzstd"])
        self.cpp_info.components["zstdlib"].libs = collect_libs(self)
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["zstdlib"].system_libs.append("pthread")
