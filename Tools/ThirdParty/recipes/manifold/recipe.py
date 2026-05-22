# Ported from conan-center-index/manifold by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir, apply_patches
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

    def requirements(self) -> list[str]:
        return []  # clipper2 and onetbb not in recipe set; PAR is disabled by default

    def source(self):
        get(
            url="https://github.com/elalish/manifold/archive/refs/tags/v3.2.1.tar.gz",
            dest=self.source_folder,
            sha256="c2fddb0f4b2289caff660b29677883f0324415a9901f8f2aed4c83851f994c13",
        )
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MANIFOLD_DOWNLOADS"] = False
        tc.cache_variables["MANIFOLD_TEST"] = False
        tc.cache_variables["MANIFOLD_CBIND"] = False
        tc.cache_variables["MANIFOLD_PYBIND"] = False
        tc.cache_variables["MANIFOLD_STRICT"] = False  # no -Werror
        tc.cache_variables["MANIFOLD_PAR"] = self.options.with_parallel_acceleration
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses")
        )
        cmake = CMake(self)
        cmake.install()

        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
