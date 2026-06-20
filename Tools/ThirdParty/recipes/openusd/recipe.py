import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeConfigDeps, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "openusd"
    version = "26.05"
    license = "LicenseRef-LICENSE.txt"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("cpython")
        self.requires("onetbb")

    def build_requirements(self):
        self.tool_requires("cmake")
        self.tool_requires("cpython")

    def latest_version(self):
        repo = GithubRepository(self, "PixarAnimationStudios/OpenUSD")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/PixarAnimationStudios/OpenUSD/archive/refs/tags/v26.05.tar.gz",
            sha256="bf514f62ac9508d3c5b121dc1107f3b29bf3c954473b9b0bf8324b7cf04c64c1",
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(
            self,
            os.path.join(self.source_folder, "pxr", "base", "work", "workTBB", "dispatcher_impl.h"),
            "#include <tbb/blocked_range.h>",
            "#include <tbb/version.h>\n#include <tbb/blocked_range.h>")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["PXR_BUILD_ALEMBIC_PLUGIN"] = False
        tc.variables["PXR_BUILD_DOCUMENTATION"] = False
        tc.variables["PXR_BUILD_DRACO_PLUGIN"] = False
        tc.variables["PXR_BUILD_EMBREE_PLUGIN"] = False
        tc.variables["PXR_BUILD_EXEC"] = True
        tc.variables["PXR_BUILD_EXAMPLES"] = False
        tc.variables["PXR_BUILD_IMAGING"] = False
        tc.variables["PXR_BUILD_MONOLITHIC"] = True
        tc.variables["PXR_BUILD_OPENCOLORIO_PLUGIN"] = False
        tc.variables["PXR_BUILD_OPENIMAGEIO_PLUGIN"] = False
        tc.variables["PXR_BUILD_PRMAN_PLUGIN"] = False
        tc.variables["PXR_BUILD_PYTHON_DOCUMENTATION"] = False
        tc.variables["PXR_BUILD_TESTS"] = False
        tc.variables["PXR_BUILD_TUTORIALS"] = False
        tc.variables["PXR_BUILD_USD_IMAGING"] = False
        tc.variables["PXR_BUILD_USD_TOOLS"] = False
        tc.variables["PXR_BUILD_USD_VALIDATION"] = False
        tc.variables["PXR_BUILD_USDVIEW"] = False
        tc.variables["PXR_ENABLE_GL_SUPPORT"] = False
        tc.variables["PXR_ENABLE_HDF5_SUPPORT"] = False
        tc.variables["PXR_ENABLE_MATERIALX_SUPPORT"] = False
        tc.variables["PXR_ENABLE_METAL_SUPPORT"] = False
        tc.variables["PXR_ENABLE_OPENVDB_SUPPORT"] = False
        tc.variables["PXR_ENABLE_OSL_SUPPORT"] = False
        tc.variables["PXR_ENABLE_PTEX_SUPPORT"] = False
        tc.variables["PXR_ENABLE_PYTHON_SUPPORT"] = True
        tc.variables["PXR_ENABLE_VULKAN_SUPPORT"] = False
        tc.variables["PXR_STRICT_BUILD_MODE"] = False
        tc.variables["PXR_VALIDATE_GENERATED_CODE"] = False

        python_pkg = self.dependencies["cpython"]
        python_root = python_pkg.package_folder.replace("\\", "/")
        tc.variables["Python3_ROOT_DIR"] = python_root
        tc.variables["Python3_FIND_STRATEGY"] = "LOCATION"
        if self.settings.os == "Windows":
            tc.variables["Python3_FIND_REGISTRY"] = "NEVER"

        tc.generate()

        deps = CMakeConfigDeps(self)
        deps.set_property("onetbb", "cmake_file_name", "TBB")
        deps.set_property("onetbb", "cmake_target_name", "TBB::tbb")
        # Don't generate CMake files for cpython - use FindPython3 directly via Python3_ROOT_DIR
        deps.set_property("cpython", "cmake_find_mode", "none")
        # Don't generate CMake files for transitive deps that OpenUSD finds via FindPython3
        # ncurses is a transitive dep of cpython
        deps.set_property("ncurses", "cmake_find_mode", "none")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "pxr")
        self.cpp_info.set_property("cmake_target_name", "pxr::usd_m")

        self.cpp_info.libs = ["usd_m"]
        if not self.options.shared:
            self.cpp_info.defines = ["PXR_STATIC=1"]
        if self.settings.os == "Windows":
            self.cpp_info.defines = (self.cpp_info.defines or []) + ["NOMINMAX"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread", "dl", "m"]

        self.cpp_info.requires = ["onetbb::onetbb"]
        self.cpp_info.requires.append("cpython::cpython")
