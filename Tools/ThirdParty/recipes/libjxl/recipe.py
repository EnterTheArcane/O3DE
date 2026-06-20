import os

from thirdparty import RecipeBase
from thirdparty.build import cross_building, stdcpp_library
from thirdparty.cmake import CMake, CMakeConfigDeps, CMakeToolchain
from thirdparty.files import copy, get, rmdir, save, rm, replace_in_file
from thirdparty.gnu import PkgConfigDeps
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "libjxl"
    version = "0.11.2"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "avx512": [True, False],
        "avx512_spr": [True, False],
        "avx512_zen4": [True, False],
        "with_tcmalloc": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "avx512": False,
        "avx512_spr": False,
        "avx512_zen4": False,
        "with_tcmalloc": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.arch not in ["x86", "x86_64"]:
            del self.options.avx512
            del self.options.avx512_spr
            del self.options.avx512_zen4

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("brotli")
        self.requires("highway")
        self.requires("lcms")
        if self.options.with_tcmalloc:
            self.requires("gperftools")

    def build_requirements(self):
        # Require newer CMake, which allows INCLUDE_DIRECTORIES to be set on INTERFACE targets
        # Also, v0.9+ require CMake 3.16
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "libjxl/libjxl")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/libjxl/libjxl/archive/v0.11.2.tar.gz",
            sha256="ab38928f7f6248e2a98cc184956021acb927b16a0dee71b4d260dc040a4320ea",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_PROJECT_LIBJXL_INCLUDE"] = os.path.join(self.generators_folder, "conan_deps.cmake").replace("\\", "/")
        tc.variables["BUILD_TESTING"] = False
        tc.variables["JPEGXL_STATIC"] = False
        tc.variables["JPEGXL_BUNDLE_LIBPNG"] = False
        tc.variables["JPEGXL_ENABLE_BENCHMARK"] = False
        tc.variables["JPEGXL_ENABLE_DOXYGEN"] = False
        tc.variables["JPEGXL_ENABLE_EXAMPLES"] = False
        tc.variables["JPEGXL_ENABLE_JNI"] = False
        tc.variables["JPEGXL_ENABLE_MANPAGES"] = False
        tc.variables["JPEGXL_ENABLE_OPENEXR"] = False
        tc.variables["JPEGXL_ENABLE_PLUGINS"] = False
        tc.variables["JPEGXL_ENABLE_SJPEG"] = False
        tc.variables["JPEGXL_ENABLE_SKCMS"] = False
        tc.variables["JPEGXL_ENABLE_TCMALLOC"] = self.options.with_tcmalloc
        tc.variables["JPEGXL_ENABLE_VIEWERS"] = False
        tc.variables["JPEGXL_ENABLE_TOOLS"] = False
        tc.variables["JPEGXL_FORCE_SYSTEM_BROTLI"] = True
        tc.variables["JPEGXL_FORCE_SYSTEM_GTEST"] = True
        tc.variables["JPEGXL_FORCE_SYSTEM_HWY"] = True
        tc.variables["JPEGXL_FORCE_SYSTEM_LCMS2"] = True
        tc.variables["JPEGXL_WARNINGS_AS_ERRORS"] = False
        tc.variables["JPEGXL_FORCE_NEON"] = False
        tc.variables["JPEGXL_ENABLE_AVX512"] = self.options.get_safe("avx512", False)
        tc.variables["JPEGXL_ENABLE_AVX512_SPR"] = self.options.get_safe("avx512_spr", False)
        tc.variables["JPEGXL_ENABLE_AVX512_ZEN4"] = self.options.get_safe("avx512_zen4", False)
        if cross_building(self):
            tc.variables["CMAKE_SYSTEM_PROCESSOR"] = str(self.settings.arch)
        # Allow non-cache_variables to be used
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        # Skip the buggy custom FindAtomic and force the use of atomic library directly for libstdc++
        tc.variables["ATOMICS_LIBRARIES"] = "atomic" if self._atomic_required else ""
        tc.variables["JPEGXL_ENABLE_JPEGLI"] = False
        tc.variables["JPEGXL_ENABLE_JPEGLI_LIBJPEG"] = False
        # TODO: can hopefully be removed in newer versions
        # https://github.com/libjxl/libjxl/issues/3159
        if self.settings.build_type == "Debug" and is_msvc(self):
            tc.preprocessor_definitions["JXL_DEBUG_V_LEVEL"] = 1
        tc.generate()

        deps = CMakeConfigDeps(self)
        deps.set_property("brotli", "cmake_file_name", "Brotli")
        deps.set_property("brotli::brotlicommon", "cmake_target_name", "brotlicommon")
        deps.set_property("brotli::brotlidec", "cmake_target_name", "brotlidec")
        deps.set_property("brotli::brotlienc", "cmake_target_name", "brotlienc")
        deps.set_property("highway", "cmake_file_name", "HWY")
        deps.set_property("highway::hwy", "cmake_target_name", "hwy::hwy")
        deps.set_property("highway::hwy_contrib", "cmake_target_name", "hwy_contrib::hwy_contrib")
        deps.set_property("lcms", "cmake_file_name", "LCMS2")
        deps.set_property("lcms", "cmake_target_name", "lcms2")
        deps.generate()

        # For tcmalloc
        deps = PkgConfigDeps(self)
        deps.generate()

    @property
    def _atomic_required(self):
        return self.settings.get_safe("compiler.libcxx") in ["libstdc++", "libstdc++11"]

    def _patch_sources(self):
        # Disable tools, extras and third_party
        save(self, os.path.join(self.source_folder, "tools", "CMakeLists.txt"), "")
        save(self, os.path.join(self.source_folder, "third_party", "CMakeLists.txt"), "")
        # FindAtomics.cmake values are set by CMakeToolchain instead
        save(self, os.path.join(self.source_folder, "cmake", "FindAtomics.cmake"), "")

        # Allow fPIC to be set by Conan (top-level set() was removed in 0.11.2; individual targets handle it)
        for cmake_file in ["jxl.cmake", "jxl_threads.cmake", "jxl_cms.cmake", "jpegli.cmake"]:
            path = os.path.join(self.source_folder, "lib", cmake_file)
            if os.path.exists(path):
                fpic = "ON" if self.options.get_safe("fPIC", True) else "OFF"
                replace_in_file(self, path, "POSITION_INDEPENDENT_CODE ON", f"POSITION_INDEPENDENT_CODE {fpic}")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        if self.options.shared:
            rm(self, "*.a", os.path.join(self.package_folder, "lib"))
            rm(self, "*-static.lib", os.path.join(self.package_folder, "lib"))

    def package_info(self):
        libcxx = stdcpp_library(self)

        # jxl
        self.cpp_info.components["jxl"].set_property("pkg_config_name", "libjxl")
        self.cpp_info.components["jxl"].libs = ["jxl"]
        self.cpp_info.components["jxl"].requires = ["brotli::brotli", "highway::highway", "lcms::lcms"]
        if self.options.with_tcmalloc:
            self.cpp_info.components["jxl"].requires.append("gperftools::tcmalloc_minimal")
        if self._atomic_required:
            self.cpp_info.components["jxl"].system_libs.append("atomic")
        if not self.options.shared:
            self.cpp_info.components["jxl"].defines.append("JXL_STATIC_DEFINE")
            if libcxx:
                self.cpp_info.components["jxl"].system_libs.append(libcxx)

        # jxl_cms
        self.cpp_info.components["jxl_cms"].set_property("pkg_config_name", "libjxl_cms")
        self.cpp_info.components["jxl_cms"].libs = ["jxl_cms"]
        self.cpp_info.components["jxl_cms"].requires = ["lcms::lcms", "highway::highway"]
        if not self.options.shared:
            self.cpp_info.components["jxl"].defines.append("JXL_CMS_STATIC_DEFINE")
            if libcxx:
                self.cpp_info.components["jxl_cms"].system_libs.append(libcxx)
        self.cpp_info.components["jxl"].requires.append("jxl_cms")

        # jxl_threads
        self.cpp_info.components["jxl_threads"].set_property("pkg_config_name", "libjxl_threads")
        self.cpp_info.components["jxl_threads"].libs = ["jxl_threads"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["jxl_threads"].system_libs = ["pthread"]
        if not self.options.shared:
            self.cpp_info.components["jxl_threads"].defines.append("JXL_THREADS_STATIC_DEFINE")
            if libcxx:
                self.cpp_info.components["jxl_threads"].system_libs.append(libcxx)
