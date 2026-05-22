# Ported from conan-center-index/libpng by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.tools.files import copy, get, rm, rmdir
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "libpng"
    version = "1.6.58"
    license = "libpng-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "neon": [True, "check", False],
        "msa": [True, False],
        "sse": [True, False],
        "vsx": [True, False],
        "api_prefix": ["ANY"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "neon": True,
        "msa": True,
        "sse": True,
        "vsx": True,
        "api_prefix": "",
    }

    @property
    def _is_clang_cl(self):
        return (
            self.is_windows
            and self.settings.compiler == "clang"
            and self.settings.compiler.get_safe("runtime")
        )

    @property
    def _has_neon_support(self):
        return "arm" in self.settings.arch

    @property
    def _has_msa_support(self):
        return "mips" in self.settings.arch

    @property
    def _has_sse_support(self):
        return self.settings.arch in ["x86", "x86_64"]

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

    def requirements(self) -> list[str]:
        return ["zlib"]

    def source(self):
        get(
            url="https://download.sourceforge.net/libpng/libpng-1.6.58.tar.xz",
            dest=self.source_folder,
            sha256="28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["PNG_TESTS"] = False
        tc.cache_variables["PNG_SHARED"] = self.options.shared
        tc.cache_variables["PNG_STATIC"] = not self.options.shared
        tc.cache_variables["PNG_DEBUG"] = self.build_type == "Debug"
        tc.cache_variables["PNG_PREFIX"] = self.options.api_prefix
        tc.cache_variables["PNG_FRAMEWORK"] = (
            False  # changed from False to True by default in PNG 1.6.41
        )
        tc.cache_variables["PNG_TOOLS"] = False
        tc.cache_variables["CMAKE_MACOSX_BUNDLE"] = False
        if self._has_neon_support:
            tc.cache_variables["PNG_ARM_NEON"] = self._neon_msa_sse_vsx_mapping[
                str(self.options.neon)
            ]
        if self._has_msa_support:
            tc.cache_variables["PNG_MIPS_MSA"] = self._neon_msa_sse_vsx_mapping[
                str(self.options.msa)
            ]
        if self._has_sse_support:
            tc.cache_variables["PNG_INTEL_SSE"] = self._neon_msa_sse_vsx_mapping[
                str(self.options.sse)
            ]
        if self._has_vsx_support:
            tc.cache_variables["PNG_POWERPC_VSX"] = self._neon_msa_sse_vsx_mapping[
                str(self.options.vsx)
            ]

        tc.generate()
        tc = CMakeDeps(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        if self.options.shared:
            rm("*[!.dll]", os.path.join(self.package_folder, "bin"))
        else:
            rmdir(os.path.join(self.package_folder, "bin"))
        rmdir(os.path.join(self.package_folder, "lib", "libpng"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
