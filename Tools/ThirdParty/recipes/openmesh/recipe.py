import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.microsoft import is_msvc


class Recipe(RecipeBase):
    name = "openmesh"
    version = "11.0"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def source(self):
        get(
            self,
            url="https://www.graphics.rwth-aachen.de/media/openmesh_static/Releases/11.0/OpenMesh-11.0.0.tar.bz2",
            sha256="9d22e65bdd6a125ac2043350a019ec4346ea83922cafdf47e125a03c16f6fa07",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        if self.settings.os == "Windows":
            tc.variables["OPENMESH_BUILD_SHARED"] = self.options.shared
        tc.variables["BUILD_APPS"] = False
        tc.variables["OPENMESH_DOCS"] = False
        tc.variables["CMAKE_CXX_STANDARD"] = 11
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "libdata"))
        rmdir(self, os.path.join(self.folders.package, "share"))
        if self.settings.os != "Windows":
            if self.options.shared:
                rm(self, "*.a", os.path.join(self.folders.package, "lib"))
            else:
                rm(self, "*.so*", os.path.join(self.folders.package, "lib"))
                rm(self, "*.dylib", os.path.join(self.folders.package, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenMesh")

        suffix = "d" if self.settings.build_type == "Debug" else ""

        self.cpp_info.components["openmeshcore"].set_property("cmake_target_name", "OpenMesh::OpenMeshCore")
        self.cpp_info.components["openmeshcore"].libs = ["OpenMeshCore" + suffix]
        if not self.options.shared:
            self.cpp_info.components["openmeshcore"].defines.append("OM_STATIC_BUILD")
        if is_msvc(self):
            self.cpp_info.components["openmeshcore"].defines.append("_USE_MATH_DEFINES")

        self.cpp_info.components["openmeshtools"].set_property("cmake_target_name", "OpenMesh::OpenMeshTools")
        self.cpp_info.components["openmeshtools"].libs = ["OpenMeshTools" + suffix]
        self.cpp_info.components["openmeshtools"].requires = ["openmeshcore"]
