# Ported from conan-center-index/libjpeg-turbo by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rm, rmdir, apply_patches
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "libjpeg-turbo"
    license = ("IJG", "BSD-3-Clause", "Zlib")
    provides = "libjpeg"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "SIMD": [True, False],
        "arithmetic_encoder": [True, False],
        "arithmetic_decoder": [True, False],
        "libjpeg7_compatibility": [True, False],
        "libjpeg8_compatibility": [True, False],
        "mem_src_dst": [True, False],
        "turbojpeg": [True, False],
        "java": [True, False],
        "enable12bit": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "SIMD": True,
        "arithmetic_encoder": True,
        "arithmetic_decoder": True,
        "libjpeg7_compatibility": True,
        "libjpeg8_compatibility": True,
        "mem_src_dst": True,
        "turbojpeg": True,
        "java": False,
        "enable12bit": False,
    }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])
        apply_patches(self)

    @property
    def _is_arithmetic_encoding_enabled(self):
        return self.options.get("arithmetic_encoder", False) or \
               self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility

    @property
    def _is_arithmetic_decoding_enabled(self):
        return self.options.get("arithmetic_decoder", False) or \
               self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = CMakeToolchain(self)
        tc.variables["ENABLE_STATIC"] = not self.options.shared
        tc.variables["ENABLE_SHARED"] = self.options.shared
        tc.variables["WITH_SIMD"] = self.options.get("SIMD", False)
        tc.variables["WITH_ARITH_ENC"] = self._is_arithmetic_encoding_enabled
        tc.variables["WITH_ARITH_DEC"] = self._is_arithmetic_decoding_enabled
        tc.variables["WITH_JPEG7"] = self.options.libjpeg7_compatibility
        tc.variables["WITH_JPEG8"] = self.options.libjpeg8_compatibility
        tc.variables["WITH_TURBOJPEG"] = self.options.get("turbojpeg", False)
        tc.variables["WITH_JAVA"] = self.options.get("java", False)
        tc.cache_variables["WITH_TOOLS"] = False
        if Version(self.version) < "3.0.0":
            tc.variables["WITH_MEM_SRCDST"] = self.options.get("mem_src_dst", False)
            tc.variables["WITH_12BIT"] = self.options.enable12bit
        if self.is_windows:
            tc.variables["WITH_CRT_DLL"] = True # avoid replacing /MD by /MT in compiler flags
        if Version(self.version) <= "2.1.0":
            tc.variables["CMAKE_MACOSX_BUNDLE"] = False # avoid configuration error if building for iOS/tvOS/watchOS
        if Version(self.version) < "3.0.2":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        if self.options.get("java", False):
            tc.cache_variables["CMAKE_INSTALL_JAVADIR"] = os.path.join(self.package_folder, "lib", "java")
        tc.generate()

    def _patch_sources(self):
        # do not override /MT by /MD if shared
        replace_in_file(os.path.join(self.source_folder, "sharedlib", "CMakeLists.txt"),
                              """string(REGEX REPLACE "/MT" "/MD" ${var} "${${var}}")""",
                              "")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy("README.ijg", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        # remove unneeded directories
        rmdir(os.path.join(self.package_folder, "share"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "doc"))
        # remove binaries and pdb files
        for pattern_to_remove in ["cjpeg*", "djpeg*", "jpegtran*", "tjbench*", "wrjpgcom*", "rdjpgcom*", "*.pdb"]:
            rm(pattern_to_remove, os.path.join(self.package_folder, "bin"))
