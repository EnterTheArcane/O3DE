from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, copy, get, rmdir
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version

import os

class Recipe(RecipeBase):
    name = "openjph"
    version = "0.27.0"
    license = "BSD-2-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_executables": [True, False],
        "with_tiff": [True, False],
        "with_stream_expand_tool": [True, False],
        "disable_simd": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_executables": True,
        "with_tiff": True,
        "with_stream_expand_tool": False,
        "disable_simd": False,
    }
    implements = ["auto_shared_fpic"]

    def requirements(self):
        if self.options.with_executables and self.options.with_tiff:
            self.requires("libtiff")

    def source(self):
        get(
            self,
            url="https://github.com/aous72/OpenJPH/archive/0.27.0.tar.gz",
            sha256="f6768e927d8e4e4884a2efcf500a88d1b6714a48d69516332a9256803a3c8343",
            destination=self.source_folder,
            strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["OJPH_BUILD_EXECUTABLES"] = self.options.with_executables
        tc.cache_variables["OJPH_ENABLE_TIFF_SUPPORT"] = self.options.with_tiff
        tc.cache_variables["OJPH_BUILD_STREAM_EXPAND"] = self.options.with_stream_expand_tool
        tc.cache_variables["OJPH_DISABLE_SIMD"] = self.options.disable_simd
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cm = CMake(self)
        cm.configure()
        cm.build()

    def package(self):
        cm = CMake(self)
        cm.install()

        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "openjph")
        self.cpp_info.set_property("cmake_target_name", "openjph::openjph")
        self.cpp_info.set_property("pkg_config_name", "openjph")

        version_suffix = "_d" if self.settings.build_type == "Debug" else ""
        if is_msvc(self):
            v = Version(self.version)
            version_suffix = f".{v.major}.{v.minor}"
            if self.settings.build_type == "Debug":
                version_suffix += "d"
        self.cpp_info.libs = ["openjph" + version_suffix]
