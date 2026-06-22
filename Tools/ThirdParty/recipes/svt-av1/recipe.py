import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMakeToolchain, CMakeDeps, CMake
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class Recipe(RecipeBase):
    name = "svt-av1"
    version = "2.2.1"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_encoder": [True, False],
        "build_decoder": [True, False],
        "minimal_build": [True, False],
        "with_neon": [True, False],
        "with_arm_crc32": [True, False],
        "with_neon_dotprod": [True, False],
        "with_neon_i8mm": [True, False],
        "with_neon_sve": [True, False],
        "with_neon_sve2": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_encoder": True,
        "build_decoder": True,
        "minimal_build": False,
        "with_neon": True,
        "with_arm_crc32": True,
        "with_neon_dotprod": True,
        "with_neon_i8mm": True,
        "with_neon_sve": True,
        "with_neon_sve2": True,
    }

    def config_options(self):
        del self.options.build_decoder
        if self.settings.arch not in ("ARM",):
            del self.options.with_neon
            del self.options.with_arm_crc32
            del self.options.with_neon_dotprod
            del self.options.with_neon_i8mm
            del self.options.with_neon_sve
            del self.options.with_neon_sve2

    def requirements(self):
        self.requires("cpuinfo")

    def build_requirements(self):
        self.tool_requires("cmake")
        if self.settings.arch in ("X64",):
            self.tool_requires("nasm")

    def latest_version(self):
        repo = GitlabRepository(self, "AOMediaCodec/SVT-AV1")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://gitlab.com/AOMediaCodec/SVT-AV1/-/archive/v2.2.1/SVT-AV1-v2.2.1.tar.gz",
            sha256="d02b54685542de0236bce4be1b50912aba68aff997c43b350d84a518df0cf4e5",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APPS"] = False
        tc.cache_variables["BUILD_ENC"] = self.options.build_encoder
        tc.cache_variables["USE_EXTERNAL_CPUINFO"] = True
        if self.settings.arch in ("X64",):
            tc.cache_variables["ENABLE_NASM"] = True
        tc.cache_variables["MINIMAL_BUILD"] = self.options.minimal_build
        if "with_neon" in self.options:
            tc.cache_variables["ENABLE_NEON"] = self.options.with_neon
        if "with_arm_crc32" in self.options:
            tc.cache_variables["ENABLE_ARM_CRC32"] = self.options.with_arm_crc32
        if "with_neon_dotprod" in self.options:
            tc.cache_variables["ENABLE_NEON_DOTPROD"] = self.options.with_neon_dotprod
        if "with_neon_i8mm" in self.options:
            tc.cache_variables["ENABLE_NEON_i8MM"] = self.options.with_neon_i8mm
        if "with_sve" in self.options:
            tc.cache_variables["ENABLE_SVE"] = self.options.with_sve
        if "with_sve2" in self.options:
            tc.cache_variables["ENABLE_SVE2"] = self.options.with_sve2
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        for license_file in ("LICENSE.md", "PATENTS.md"):
            copy(self, license_file, self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.configure()
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        if self.options.build_encoder:
            self.cpp_info.components["encoder"].libs = ["SvtAv1Enc"]
            self.cpp_info.components["encoder"].includedirs = ["include/svt-av1"]
            self.cpp_info.components["encoder"].set_property("pkg_config_name", "SvtAv1Enc")
            self.cpp_info.components["encoder"].requires = ["cpuinfo::cpuinfo"]
            if self.settings.os in ("FreeBSD", "Linux"):
                self.cpp_info.components["encoder"].system_libs = ["pthread", "dl", "m"]
            if self.settings.os == "Android":
                self.cpp_info.components["encoder"].system_libs = ["m"]
        if self.options.get_safe("build_decoder"):
            self.cpp_info.components["decoder"].libs = ["SvtAv1Dec"]
            self.cpp_info.components["decoder"].includedirs = ["include/svt-av1"]
            self.cpp_info.components["decoder"].set_property("pkg_config_name", "SvtAv1Dec")
            self.cpp_info.components["decoder"].requires = ["cpuinfo::cpuinfo"]
            if self.settings.os in ("FreeBSD", "Linux"):
                self.cpp_info.components["decoder"].system_libs = ["pthread", "dl", "m"]
            if self.settings.os == "Android":
                self.cpp_info.components["decoder"].system_libs = ["m"]
