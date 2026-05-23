from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir, apply_conandata_patches
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "manifold"
    version = "3.2.1"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_parallel_acceleration": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_parallel_acceleration": False,
    }
    implements = ["auto_shared_fpic"]

    def requirements(self):
        # For CrossSection for 2D support
        self.requires("clipper2/1.4.0")
        if self.options.with_parallel_acceleration:
            self.requires("onetbb/2022.0.0")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.18]")

    def source(self):
        get(self, url="https://github.com/elalish/manifold/archive/refs/tags/v3.2.1.tar.gz", sha256="c2fddb0f4b2289caff660b29677883f0324415a9901f8f2aed4c83851f994c13", destination=self.source_folder, strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MANIFOLD_DOWNLOADS"] = False
        tc.cache_variables["MANIFOLD_TEST"] = False
        tc.cache_variables["MANIFOLD_CBIND"] = False
        tc.cache_variables["MANIFOLD_PYBIND"] = False
        tc.cache_variables["MANIFOLD_STRICT"] = False # no -Werror
        tc.cache_variables["MANIFOLD_PAR"] = self.options.with_parallel_acceleration
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("clipper2::clipper2", "cmake_target_name", "Clipper2")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libs = ["manifold"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
