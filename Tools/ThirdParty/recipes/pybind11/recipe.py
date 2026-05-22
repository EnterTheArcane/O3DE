from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import get, copy, replace_in_file, rm, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "pybind11"
    version = "3.0.1"
    license = "BSD-3-Clause"

    def source(self):
        get(
            url="https://github.com/pybind/pybind11/archive/v3.0.1.tar.gz",
            dest=self.source_folder,
            sha256="741633da746b7c738bb71f1854f957b9da660bcd2dce68d71949037f0969d0ca",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PYBIND11_FINDPYTHON"] = True
        tc.variables["PYBIND11_INSTALL"] = True
        tc.variables["PYBIND11_TEST"] = False
        tc.variables["PYBIND11_CMAKECONFIG_INSTALL_DIR"] = "lib/cmake/pybind11"
        if Version(self.version) < "2.11.0":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        tc.generate()

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
        for filename in [
            "pybind11Targets.cmake",
            "pybind11Config.cmake",
            "pybind11ConfigVersion.cmake",
        ]:
            rm(filename, os.path.join(self.package_folder, "lib", "cmake", "pybind11"))

        rmdir(os.path.join(self.package_folder, "share"))

        checked_target = "lto" if self.version < Version("2.11.0") else "pybind11"
        if self.version < Version("3.0.0"):
            replace_in_file(
                os.path.join(
                    self.package_folder,
                    "lib",
                    "cmake",
                    "pybind11",
                    "pybind11Common.cmake",
                ),
                f"if(TARGET pybind11::{checked_target})",
                "if(FALSE)",
            )
        replace_in_file(
            os.path.join(
                self.package_folder, "lib", "cmake", "pybind11", "pybind11Common.cmake"
            ),
            "add_library(",
            "# add_library(",
        )
