# Ported from conan-center-index/rvo2 by port_recipe.py
# REVIEW: verify all transforms are correct before building

import os

from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file

class Recipe(RecipeBase):
    name = "rvo2"
    license = "Apache-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
        tc.generate()

    def _patch_sources(self):
        replace_in_file(os.path.join(self.source_folder, "CMakeLists.txt"), "add_subdirectory(examples)", "")
        # Add GNUInstallDirs so CMAKE_INSTALL_INCLUDEDIR etc. are defined
        replace_in_file(os.path.join(self.source_folder, "CMakeLists.txt"),
            "project(RVO)",
            "project(RVO)\ninclude(GNUInstallDirs)")
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            "DESTINATION include",
            "DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}",
        )
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            "RVO DESTINATION lib",
            "RVO RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}",
        )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        fix_apple_shared_install_name(self)
