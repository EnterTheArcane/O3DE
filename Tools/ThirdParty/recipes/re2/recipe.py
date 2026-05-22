# Ported from conan-center-index/re2 by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "re2"
    version = "20251105"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_icu": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_icu": False,
    }

    implements = ["auto_shared_fpic"]

    def requirements(self) -> list[str]:
        return ["abseil"]  # icu is optional (with_icu defaults to False)

    def source(self):
        get(
            url="https://github.com/google/re2/releases/download/2025-11-05/re2-2025-11-05.tar.gz",
            dest=self.source_folder,
            sha256="87f6029d2f6de8aa023654240a03ada90e876ce9a4676e258dd01ea4c26ffd67",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["RE2_BUILD_TESTING"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
