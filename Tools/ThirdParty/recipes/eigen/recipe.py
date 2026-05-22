from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, copy, get, rmdir
from thirdparty.tools.scm import Version
import os

class Recipe(ConanFile):
    name = "eigen"
    version = "5.0.1"
    package_type = "header-library"
    license = ("MPL-2.0", "LGPL-3.0-or-later")  # Taking into account the default value of MPL2_only option

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "MPL2_only": [True, False],
    }
    default_options = {
        "MPL2_only": False,
    }

    def configure(self):
        self.license = "MPL-2.0"  # MPL-2 only
        if Version(self.version) >= "5.0.0":
            del self.options.MPL2_only
        elif not self.options.MPL2_only:  # < 5.0.0
            self.license = ("MPL-2.0", "LGPL-3.0-or-later")

    def source(self):
        get(self, url="https://gitlab.com/libeigen/eigen/-/archive/5.0.1/eigen-5.0.1.tar.bz2", sha256="e4de6b08f33fd8b8985d2f204381408c660bffa6170ac65b68ae1bd3cd575c0a", destination=self.source_folder, strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTING"] = not self.conf.get("tools.build:skip_test", default=True, check_type=bool)
        if Version(self.version) >= "5.0.0":
            # TODO consider making EIGEN_BUILD_{BLAS,LAPACK} tunable
            tc.cache_variables["EIGEN_BUILD_BLAS"] = False
            tc.cache_variables["EIGEN_BUILD_LAPACK"] = False
            tc.cache_variables["EIGEN_BUILD_DEMOS"] = False
            tc.cache_variables["EIGEN_BUILD_DOC"] = False
            tc.cache_variables["EIGEN_BUILD_PKGCONFIG"] = False
            tc.cache_variables["EIGEN_BUILD_TESTING"] = tc.cache_variables["BUILD_TESTING"]
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

        copy(self, "COPYING.*", self.source_folder, os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Eigen3")
        self.cpp_info.set_property("cmake_target_name", "Eigen3::Eigen")
        self.cpp_info.set_property("pkg_config_name", "eigen3")
        self.cpp_info.components["eigen3"].bindirs = []
        self.cpp_info.components["eigen3"].libdirs = []
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["eigen3"].system_libs = ["m"]
        if self.options.get_safe("MPL2_only"):
            self.cpp_info.components["eigen3"].defines = ["EIGEN_MPL2_ONLY"]

        self.cpp_info.components["eigen3"].set_property("cmake_target_name", "Eigen3::Eigen")
        self.cpp_info.components["eigen3"].includedirs = [os.path.join("include", "eigen3")]
