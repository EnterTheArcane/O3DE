# Ported from conan-center-index/manifold by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir, apply_patches
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "manifold"
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
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MANIFOLD_DOWNLOADS"] = False
        tc.cache_variables["MANIFOLD_TEST"] = False
        tc.cache_variables["MANIFOLD_CBIND"] = False
        tc.cache_variables["MANIFOLD_PYBIND"] = False
        tc.cache_variables["MANIFOLD_STRICT"] = False # no -Werror
        tc.cache_variables["MANIFOLD_PAR"] = self.options.with_parallel_acceleration
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
