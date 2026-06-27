from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = True
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "openusd"
    version = "26.05"
    license = "LicenseRef-LICENSE.txt"

    def requirements(self):
        self.requires("cpython")
        self.requires("onetbb")
        self.requires_tool("cmake")
        self.requires_tool("cpython")

    def latest_version(self):
        repo = GithubRepository(self, "PixarAnimationStudios/OpenUSD")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/PixarAnimationStudios/OpenUSD/archive/refs/tags/v26.05.tar.gz",
            sha256="bf514f62ac9508d3c5b121dc1107f3b29bf3c954473b9b0bf8324b7cf04c64c1",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(
            self,
            self.folders.source / "pxr" / "base" / "work" / "workTBB" / "dispatcher_impl.h",
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
        python_root = python_pkg.folders.package.as_posix()
        tc.variables["Python3_ROOT_DIR"] = python_root
        tc.variables["Python3_FIND_STRATEGY"] = "LOCATION"
        if self.settings.os == "Windows":
            tc.variables["Python3_FIND_REGISTRY"] = "NEVER"

        tc.generate()

        deps = CMakeDeps(self)
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
        copy(self, "LICENSE.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "cmake")

    def package_info(self):
        self.info.set_property("cmake_file_name", "pxr")
        self.info.set_property("cmake_target_name", "pxr::usd_m")

        self.info.libs = ["usd_m"]
        if not self.options.shared:
            self.info.defines = ["PXR_STATIC=1"]
        if self.settings.os == "Windows":
            self.info.defines = (self.info.defines or []) + ["NOMINMAX"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.info.system_libs = ["pthread", "dl", "m"]

        self.info.requires = ["onetbb::onetbb"]
        self.info.requires.append("cpython::cpython")
