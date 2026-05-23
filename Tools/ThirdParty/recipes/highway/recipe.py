import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rmdir
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "highway"
    version = "1.4.0"
    license = ("Apache-2.0", "BSD-3-Clause")

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_test": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_test": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def latest_version(self):
        repo = GithubRepository(self, "google/highway")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/google/highway/archive/1.4.0.tar.gz",
            sha256="e72241ac9524bb653ae52ced768b508045d4438726a303f10181a38f764a453c",
            destination=self.source_folder,
            strip_root=True)
        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        tc.variables["HWY_ENABLE_EXAMPLES"] = False
        tc.variables["HWY_ENABLE_TESTS"] = self.options.get_safe("with_test", False)
        tc.generate()

    def _patch_sources(self):
        # No hardcoded CMAKE_CXX_STANDARD
        replace_in_file(self, os.path.join(self.source_folder, "cmake", "FindAtomics.cmake"),
                        "set(CMAKE_CXX_STANDARD 11)", "")
        replace_in_file(self, os.path.join(self.source_folder, "cmake", "FindAtomics.cmake"),
                        "unset(CMAKE_CXX_STANDARD)", "")
        # Honor fPIC option
        cmakelists = os.path.join(self.source_folder, "CMakeLists.txt")
        replace_in_file(self, cmakelists, "set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)", "")
        replace_in_file(self, cmakelists,
                              "set_property(TARGET hwy PROPERTY POSITION_INDEPENDENT_CODE ON)",
                              "")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        license_folder = os.path.join(self.package_folder, "licenses")
        copy(self, "LICENSE", src=self.source_folder, dst=license_folder)
        copy(self, "LICENSE-BSD3", src=self.source_folder, dst=license_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.components["hwy"].set_property("pkg_config_name", "libhwy")
        self.cpp_info.components["hwy"].libs = ["hwy"]
        self.cpp_info.components["hwy"].defines.append(
            "HWY_SHARED_DEFINE" if self.options.shared else "HWY_STATIC_DEFINE"
        )
        self.cpp_info.components["hwy_contrib"].set_property("pkg_config_name", "libhwy-contrib")
        self.cpp_info.components["hwy_contrib"].libs = ["hwy_contrib"]
        self.cpp_info.components["hwy_contrib"].requires = ["hwy"]
        self.cpp_info.components["hwy_contrib"].system_libs.append("pthread")
        if self.options.with_test:
            self.cpp_info.components["hwy_test"].set_property("pkg_config_name", "libhwy-test")
            self.cpp_info.components["hwy_test"].libs = ["hwy_test"]
            self.cpp_info.components["hwy_test"].requires = ["hwy"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
