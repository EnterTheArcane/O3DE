from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.scm import Version
import os


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
        "with_opengl": False,
        "with_omp": False,
        "with_cuda": False,
        "with_clew": False,
        "with_opencl": False,
        "with_dx": False,
        "with_metal": False,
    }

    @property
    def _min_cppstd(self):
        if self.options.get("with_metal"):
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

    def requirements(self) -> list[str]:
        return (
            []
        )  # onetbb/opengl/glfw not in recipe set; opengl is system lib; metal-cpp only on macOS

    def source(self):
        get(
            url="https://github.com/PixarAnimationStudios/OpenSubdiv/archive/refs/tags/v3_7_0.tar.gz",
            dest=self.source_folder,
            sha256="f843eb49daf20264007d807cbc64516a1fed9cdb1149aaf84ff47691d97491f9",
        )

    @property
    def _osd_gpu_enabled(self):
        return any(
            [
                self.options.with_opengl,
                self.options.with_opencl,
                self.options.with_cuda,
                self.options.get("with_dx"),
                self.options.get("with_metal"),
            ]
        )

    def generate(self):
        tc = CMakeToolchain(self)
        if not valid_min_cppstd(self, self._min_cppstd):
            tc.variables["CMAKE_CXX_STANDARD"] = self._min_cppstd
        tc.variables["NO_TBB"] = not self.options.with_tbb
        tc.variables["NO_OPENGL"] = not self.options.with_opengl
        tc.variables["BUILD_SHARED_LIBS"] = self.options.get("shared")
        tc.variables["NO_OMP"] = not self.options.with_omp
        tc.variables["NO_CUDA"] = not self.options.with_cuda
        tc.variables["NO_DX"] = not self.options.get("with_dx")
        tc.variables["NO_METAL"] = not self.options.get("with_metal")
        tc.variables["NO_CLEW"] = not self.options.with_clew
        tc.variables["NO_OPENCL"] = not self.options.with_opencl
        tc.variables["NO_PTEX"] = (
            True  # Note: PTEX is for examples only, but we skip them..
        )
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
        if self.is_macos and not self._osd_gpu_enabled:
            path = os.path.join(self.source_folder, "opensubdiv", "CMakeLists.txt")
            replace_in_file(path, "$<TARGET_OBJECTS:osd_gpu_obj>", "")
        # No warnings as errors
        replace_in_file(os.path.join(self.source_folder, "CMakeLists.txt"), "/WX", "")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        if self.options.shared:
            rm("*.a", os.path.join(self.package_folder, "lib"))
