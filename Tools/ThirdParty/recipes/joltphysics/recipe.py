# Ported from conan-center-index/joltphysics by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "joltphysics"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "simd": ["sse", "sse41", "sse42", "avx", "avx2", "avx512"],
        "debug_renderer": [True, False],
        "profile": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "simd": "sse42",
        "debug_renderer": False,
        "profile": False,
    }

    @property
    def _min_cppstd(self):
        return "17"

    @property
    def _compilers_minimum_version(self):
        return {
            "Visual Studio": "16",
            "msvc": "192",
            "gcc": "9.2", # due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81429
            "clang": "5",
            "apple-clang": "12",
        }

    @property
    def _has_sse41(self):
        return self.options.get("simd") in ("sse41", "sse42", "avx", "avx2", "avx512")

    @property
    def _has_sse42(self):
        return self.options.get("simd") in ("sse42", "avx", "avx2", "avx512")

    @property
    def _has_avx(self):
        return self.options.get("simd") in ("avx", "avx2", "avx512")

    @property
    def _has_avx2(self):
        return self.options.get("simd") in ("avx2", "avx512")

    @property
    def _has_avx512(self):
        return self.options.get("simd") == "avx512"

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["TARGET_UNIT_TESTS"] = False
        tc.variables["TARGET_HELLO_WORLD"] = False
        tc.variables["TARGET_PERFORMANCE_TEST"] = False
        tc.variables["TARGET_SAMPLES"] = False
        tc.variables["TARGET_VIEWER"] = False
        tc.variables["GENERATE_DEBUG_SYMBOLS"] = False
        tc.variables["TARGET_UNIT_TESTS"] = False
        tc.variables["USE_SSE4_1"] = self._has_sse41
        tc.variables["USE_SSE4_2"] = self._has_sse42
        tc.variables["USE_AVX"] = self._has_avx
        tc.variables["USE_AVX2"] = self._has_avx2
        tc.variables["USE_AVX512"] = self._has_avx512
        if self.is_windows:
            tc.variables["USE_STATIC_MSVC_RUNTIME_LIBRARY"] = False
        tc.variables["JPH_DEBUG_RENDERER"] = self.options.debug_renderer
        tc.variables["JPH_PROFILE_ENABLED"] = self.options.profile
        if Version(self.version) >= "3.0.0":
            tc.variables["ENABLE_ALL_WARNINGS"] = False
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "Build"))
        cmake.build()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
