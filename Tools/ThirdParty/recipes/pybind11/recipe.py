import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import get, copy, replace_in_file, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "pybind11"
    version = "3.0.4"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "pybind/pybind11")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/pybind/pybind11/archive/v3.0.4.tar.gz",
            sha256="74b6a2c2b4573a400cafb6ecbf60c98df300cd3d0041296b913d02b2cbbb2676",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PYBIND11_FINDPYTHON"] = True
        tc.variables["PYBIND11_INSTALL"] = True
        tc.variables["PYBIND11_TEST"] = False
        tc.variables["PYBIND11_CMAKECONFIG_INSTALL_DIR"] = "lib/cmake/pybind11"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        for filename in ["pybind11Targets.cmake", "pybind11Config.cmake", "pybind11ConfigVersion.cmake"]:
            rm(self, filename, self.folders.package / "lib" / "cmake" / "pybind11")

        rmdir(self, self.folders.package / "share")

        replace_in_file(
            self, self.folders.package / "lib" / "cmake" / "pybind11" / "pybind11Common.cmake",
            "add_library(",
            "# add_library(")

    def package_info(self):
        cmake_base_path = os.path.join("lib", "cmake", "pybind11")
        self.info.set_property("cmake_target_name", "pybind11_all_do_not_use")
        self.info.components["headers"].includedirs = ["include"]
        self.info.components["pybind11_"].set_property("cmake_target_name", "pybind11::pybind11")
        self.info.components["pybind11_"].builddirs = [cmake_base_path]
        self.info.components["pybind11_"].requires = ["headers"]
        cmake_file = os.path.join(cmake_base_path, "pybind11Common.cmake")
        self.info.set_property("cmake_build_modules", [cmake_file])
        self.info.components["embed"].requires = ["pybind11_"]
        self.info.components["module"].requires = ["pybind11_"]
        self.info.components["python_link_helper"].requires = ["pybind11_"]
        self.info.components["windows_extras"].requires = ["pybind11_"]
        self.info.components["lto"].requires = ["pybind11_"]
        self.info.components["thin_lto"].requires = ["pybind11_"]
        self.info.components["opt_size"].requires = ["pybind11_"]
        self.info.components["python2_no_register"].requires = ["pybind11_"]
