# Ported from conan-center-index/eigen by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "eigen"
    version = "5.0.1"
    license = (
        "MPL-2.0",
        "LGPL-3.0-or-later",
    )  # Taking into account the default value of MPL2_only option
    options = {
        "MPL2_only": [True, False],
    }
    default_options = {
        "MPL2_only": False,
    }

    def source(self):
        get(
            url="https://gitlab.com/libeigen/eigen/-/archive/5.0.1/eigen-5.0.1.tar.bz2",
            sha256="e4de6b08f33fd8b8985d2f204381408c660bffa6170ac65b68ae1bd3cd575c0a",
            dest=self.source_folder,
            strip_root=True,
        )
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTING"] = False
        if Version(self.version) >= "5.0.0":
            # TODO consider making EIGEN_BUILD_{BLAS,LAPACK} tunable
            tc.cache_variables["EIGEN_BUILD_BLAS"] = False
            tc.cache_variables["EIGEN_BUILD_LAPACK"] = False
            tc.cache_variables["EIGEN_BUILD_DEMOS"] = False
            tc.cache_variables["EIGEN_BUILD_DOC"] = False
            tc.cache_variables["EIGEN_BUILD_PKGCONFIG"] = False
            tc.cache_variables["EIGEN_BUILD_TESTING"] = tc.cache_variables[
                "BUILD_TESTING"
            ]
        else:
            tc.cache_variables["EIGEN_TEST_NOQT"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

        copy(
            "COPYING.*",
            self.source_folder,
            os.path.join(self.package_folder, "licenses"),
        )
        rmdir(os.path.join(self.package_folder, "share"))
