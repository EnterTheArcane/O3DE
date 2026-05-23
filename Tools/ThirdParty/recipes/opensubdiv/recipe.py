import os

from thirdparty import RecipeBase
from thirdparty.tools.build import valid_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "opensubdiv"
    version = "3.7.0"
    license = "LicenseRef-LICENSE.txt"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tbb": [True, False],
        "with_opengl": [True, False],
        "with_omp": [True, False],
        "with_cuda": [True, False],
        "with_clew": [True, False],
        "with_opencl": [True, False],
        "with_dx": [True, False],
        "with_metal": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tbb": False,
        "with_opengl": True,
        "with_omp": False,
        "with_cuda": False,
        "with_clew": False,
        "with_opencl": False,
        "with_dx": False,
        "with_metal": True
    }

    @property
    def _min_cppstd(self):
        if self.options.get_safe("with_metal"):
            return "14"
        return "11"

    @property
    def _minimum_compilers_version(self):
        return {
            "Visual Studio": "15",
            "msvc": "191",
            "gcc": "5",
            "clang": "11",
            "apple-clang": "11.0",
        }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        else:
            del self.options.with_dx
        if self.settings.os != "Macos":
            del self.options.with_metal
        self.license = "DocumentRef-LICENSE.txt:LicenseRef-Tomorrow-Open-Source-Technology"

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.with_tbb:
            self.requires("onetbb", transitive_headers=True)
        if self.options.with_opengl:
            self.requires("opengl")
            self.requires("glfw")
        if self.options.get_safe("with_metal"):
            self.requires("metal-cpp")

    def latest_version(self):
        repo = GithubRepository(self, "PixarAnimationStudios/OpenSubdiv")
        return Version(repo.latest_release.removeprefix("v").replace("_", "."))

    def source(self):
        get(
            self,
            url="https://github.com/PixarAnimationStudios/OpenSubdiv/archive/refs/tags/v3_7_0.tar.gz",
            sha256="f843eb49daf20264007d807cbc64516a1fed9cdb1149aaf84ff47691d97491f9",
            destination=self.source_folder,
            strip_root=True)

    @property
    def _osd_gpu_enabled(self):
        return any(
            [
                self.options.with_opengl,
                self.options.with_opencl,
                self.options.with_cuda,
                self.options.get_safe("with_dx"),
                self.options.get_safe("with_metal"),
            ]
        )

    def generate(self):
        tc = CMakeToolchain(self)
        if not valid_min_cppstd(self, self._min_cppstd):
            tc.variables["CMAKE_CXX_STANDARD"] = self._min_cppstd
        tc.variables["NO_TBB"] = not self.options.with_tbb
        tc.variables["NO_OPENGL"] = not self.options.with_opengl
        tc.variables["BUILD_SHARED_LIBS"] = self.options.get_safe("shared")
        tc.variables["NO_OMP"] = not self.options.with_omp
        tc.variables["NO_CUDA"] = not self.options.with_cuda
        tc.variables["NO_DX"] = not self.options.get_safe("with_dx")
        tc.variables["NO_METAL"] = not self.options.get_safe("with_metal")
        tc.variables["NO_CLEW"] = not self.options.with_clew
        tc.variables["NO_OPENCL"] = not self.options.with_opencl
        tc.variables["NO_PTEX"] = True  # Note: PTEX is for examples only, but we skip them..
        tc.variables["NO_DOC"] = True
        tc.variables["NO_EXAMPLES"] = True
        tc.variables["NO_TUTORIALS"] = True
        tc.variables["NO_REGRESSION"] = True
        tc.variables["NO_TESTS"] = True
        tc.variables["NO_GLTESTS"] = True
        tc.variables["NO_MACOS_FRAMEWORK"] = True
        tc.generate()

        tc = CMakeDeps(self)
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        if self.settings.os == "Macos" and not self._osd_gpu_enabled:
            path = os.path.join(self.source_folder, "opensubdiv", "CMakeLists.txt")
            replace_in_file(self, path, "$<TARGET_OBJECTS:osd_gpu_obj>", "")
        # No warnings as errors
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "/WX", "")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        if self.options.shared:
            rm(self, "*.a", os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenSubdiv")
        target_suffix = "" if self.options.shared else "_static"

        self.cpp_info.components["osdcpu"].set_property("cmake_target_name", f"OpenSubdiv::osdcpu{target_suffix}")
        self.cpp_info.components["osdcpu"].libs = ["osdCPU"]
        if self.options.with_tbb:
            self.cpp_info.components["osdcpu"].requires = ["onetbb::onetbb"]

        if self._osd_gpu_enabled:
            self.cpp_info.components["osdgpu"].set_property("cmake_target_name", f"OpenSubdiv::osdgpu{target_suffix}")
            self.cpp_info.components["osdgpu"].libs = ["osdGPU"]
            self.cpp_info.components["osdgpu"].requires = ["osdcpu"]
            if self.options.with_opengl:
                self.cpp_info.components["osdgpu"].requires.extend(["opengl::opengl", "glfw::glfw"])
            if self.options.get_safe("with_metal"):
                self.cpp_info.components["osdgpu"].requires.append("metal-cpp::metal-cpp")
            dl_required = self.options.with_opengl or self.options.with_opencl
            if self.settings.os in ["Linux", "FreeBSD"] and dl_required:
                self.cpp_info.components["osdgpu"].system_libs = ["dl"]

