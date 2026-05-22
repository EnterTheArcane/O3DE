# Ported from conan-center-index/zstd by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import (
    apply_patches,
    collect_libs,
    copy,
    get,
    replace_in_file,
    rmdir,
    rm,
)
from thirdparty.tools.scm import Version
import glob
import os


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

    def source(self):
        get(
            url="https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz",
            dest=self.source_folder,
            sha256="eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ZSTD_BUILD_PROGRAMS"] = self.options.build_programs
        tc.variables["ZSTD_BUILD_STATIC"] = (
            not self.options.shared or self.options.build_programs
        )
        tc.variables["ZSTD_BUILD_SHARED"] = self.options.shared
        tc.variables["ZSTD_MULTITHREAD_SUPPORT"] = self.options.threading
        if Version(self.version) < "1.5.6":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # Don't force PIC
        replace_in_file(
            os.path.join(self.source_folder, "build", "cmake", "lib", "CMakeLists.txt"),
            "POSITION_INDEPENDENT_CODE On",
            "",
        )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=os.path.join(self.source_folder, "build", "cmake")
        )
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))

        if self.options.shared and self.options.build_programs:
            # If we build programs we have to build static libs (see logic in generate()),
            # but if shared is True, we only want shared lib in package folder.
            rm("*_static.*", os.path.join(self.package_folder, "lib"))
            for lib in glob.glob(os.path.join(self.package_folder, "lib", "*.a")):
                if not lib.endswith(".dll.a"):
                    os.remove(lib)
