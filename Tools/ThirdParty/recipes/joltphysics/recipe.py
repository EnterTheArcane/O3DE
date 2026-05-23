from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, copy, get
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "joltphysics"
    version = "3.0.1"
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
        return self.options.get_safe("simd") in ("sse41", "sse42", "avx", "avx2", "avx512")

    @property
    def _has_sse42(self):
        return self.options.get_safe("simd") in ("sse42", "avx", "avx2", "avx512")

    @property
    def _has_avx(self):
        return self.options.get_safe("simd") in ("avx", "avx2", "avx512")

    @property
    def _has_avx2(self):
        return self.options.get_safe("simd") in ("avx2", "avx512")

    @property
    def _has_avx512(self):
        return self.options.get_safe("simd") == "avx512"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.arch not in ("x86", "x86_64"):
            del self.options.simd

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def build_requirements(self):
        self.tool_requires("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v3.0.1.tar.gz",
            sha256="7ebb40bf2dddbcf0515984582aaa197ddd06e97581fd55b98cb64f91b243b8a6",
            destination=self.source_folder,
            strip_root=True)

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
        if is_msvc(self):
            tc.variables["USE_STATIC_MSVC_RUNTIME_LIBRARY"] = is_msvc_static_runtime(self)
        tc.variables["JPH_DEBUG_RENDERER"] = self.options.debug_renderer
        tc.variables["JPH_PROFILE_ENABLED"] = self.options.profile
        if Version(self.version) >= "3.0.0":
            tc.variables["ENABLE_ALL_WARNINGS"] = False
        tc.generate()

    def build(self):
        apply_conandata_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "Build"))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["Jolt"]
        if self._has_sse41:
            self.cpp_info.defines.append("JPH_USE_SSE4_1")
        if self._has_sse42:
            self.cpp_info.defines.append("JPH_USE_SSE4_2")
        if self._has_avx:
            self.cpp_info.defines.append("JPH_USE_AVX")
        if self._has_avx2:
            self.cpp_info.defines.append("JPH_USE_AVX2")
        if self._has_avx512:
            self.cpp_info.defines.append("JPH_USE_AVX512")
        if self.options.debug_renderer:
            self.cpp_info.defines.append("JPH_DEBUG_RENDERER")
        if self.options.profile:
            self.cpp_info.defines.append("JPH_PROFILE_ENABLED")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread"])
