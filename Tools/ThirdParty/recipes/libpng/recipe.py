import os
from typing import Literal

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    neon: Literal[True, 'check', False] = True
    msa: bool = True
    sse: bool = True
    vsx: bool = True
    api_prefix: str = ''


class Recipe(RecipeBase[_Options]):
    name = "libpng"
    version = "1.6.58"
    license = "libpng-2.0"

    @property
    def _is_clang_cl(self):
        return self.settings.os == "Windows" and self.settings.compiler == "clang" and \
            self.settings.compiler.get_safe("runtime")

    @property
    def _has_neon_support(self):
        return "arm" in self.settings.arch

    @property
    def _has_msa_support(self):
        return "mips" in self.settings.arch

    @property
    def _has_sse_support(self):
        return self.settings.arch in ["X64"]

    @property
    def _has_vsx_support(self):
        return "ppc" in self.settings.arch

    @property
    def _neon_msa_sse_vsx_mapping(self):
        return {
            "True": "on",
            "False": "off",
            "check": "check",
        }

    def config_options(self):
        if not self._has_neon_support:
            del self.options.neon
        if not self._has_msa_support:
            del self.options.msa
        if not self._has_sse_support:
            del self.options.sse
        if not self._has_vsx_support:
            del self.options.vsx

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        self.requires("zlib")

    def latest_version(self):
        repo = GithubRepository(self, "pnggroup/libpng")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://download.sourceforge.net/libpng/libpng-1.6.58.tar.xz",
            sha256="28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["PNG_TESTS"] = False
        tc.cache_variables["PNG_SHARED"] = self.options.shared
        tc.cache_variables["PNG_STATIC"] = not self.options.shared
        tc.cache_variables["PNG_DEBUG"] = self.settings.build_type == "Debug"
        tc.cache_variables["PNG_PREFIX"] = self.options.api_prefix
        tc.cache_variables["PNG_FRAMEWORK"] = False  # changed from False to True by default in PNG 1.6.41
        tc.cache_variables["PNG_TOOLS"] = False
        tc.cache_variables["CMAKE_MACOSX_BUNDLE"] = False
        if self._has_neon_support:
            tc.cache_variables["PNG_ARM_NEON"] = self._neon_msa_sse_vsx_mapping[str(self.options.neon)]
        if self._has_msa_support:
            tc.cache_variables["PNG_MIPS_MSA"] = self._neon_msa_sse_vsx_mapping[str(self.options.msa)]
        if self._has_sse_support:
            tc.cache_variables["PNG_INTEL_SSE"] = self._neon_msa_sse_vsx_mapping[str(self.options.sse)]
        if self._has_vsx_support:
            tc.cache_variables["PNG_POWERPC_VSX"] = self._neon_msa_sse_vsx_mapping[str(self.options.vsx)]

        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        if self.options.shared:
            rm(self, "*[!.dll]", os.path.join(self.folders.package, "bin"))
        else:
            rmdir(self, os.path.join(self.folders.package, "bin"))
        rmdir(self, os.path.join(self.folders.package, "lib", "libpng"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "share"))
        rm(self, "*.cmake", os.path.join(self.folders.package, "lib", "cmake", "PNG"))

    def package_info(self):
        major_min_version = f"{Version(self.version).major}{Version(self.version).minor}"

        self.info.set_property("cmake_file_name", "PNG")
        self.info.set_property("cmake_target_name", "PNG::PNG")
        self.info.set_property("pkg_config_name", "libpng")
        self.info.set_property("pkg_config_aliases", [f"libpng{major_min_version}"])

        prefix = "lib" if (is_msvc(self) or self._is_clang_cl) else ""
        suffix = major_min_version if self.settings.os == "Windows" else ""
        if is_msvc(self) or self._is_clang_cl:
            suffix += "_static" if not self.options.shared else ""
        suffix += "d" if self.settings.os == "Windows" and self.settings.build_type == "Debug" else ""
        self.info.libs = [f"{prefix}png{suffix}"]
        if self.settings.os in ["Linux", "Android", "FreeBSD", "SunOS", "AIX"]:
            self.info.system_libs.append("m")
