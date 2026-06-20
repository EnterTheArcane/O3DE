import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


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

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "OpenMesh/OpenMesh")
        return Version(repo.latest_release.removeprefix("OpenMesh-"))

    def source(self):
        get(
            self,
            url="https://www.graphics.rwth-aachen.de/media/openmesh_static/Releases/11.0/OpenMesh-11.0.0.tar.bz2",
            sha256="9d22e65bdd6a125ac2043350a019ec4346ea83922cafdf47e125a03c16f6fa07",
            destination=self.source_folder,
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
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "libdata"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        if self.settings.os != "Windows":
            if self.options.shared:
                rm(self, "*.a", os.path.join(self.package_folder, "lib"))
            else:
                rm(self, "*.so*", os.path.join(self.package_folder, "lib"))
                rm(self, "*.dylib", os.path.join(self.package_folder, "lib"))

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
