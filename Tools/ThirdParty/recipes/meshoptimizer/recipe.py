from thirdparty import RecipeBase
from thirdparty.tools.build import stdcpp_library
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rm, rmdir
import os

class Recipe(RecipeBase):
    name = "meshoptimizer"
    version = "1.0"
    license = "MIT"

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

    def source(self):
        get(
            self,
            url="https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.0.tar.gz",
            sha256="30d1c3651986b2074e847b17223a7269c9612ab7f148b944250f81214fed4993",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MESHOPT_BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "meshoptimizer")
        self.cpp_info.set_property("cmake_target_name", "meshoptimizer::meshoptimizer")
        self.cpp_info.libs = ["meshoptimizer"]
        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.system_libs.append(libcxx)
        if self.options.shared:
            self.cpp_info.defines = ["MESHOPTIMIZER_ALLOC_EXPORT"]
            if self.settings.os == "Windows":
                self.cpp_info.defines.append("MESHOPTIMIZER_API=__declspec(dllimport)")
