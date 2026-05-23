import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get

class Recipe(RecipeBase):
    name = "spirv-reflect"
    version = "1.4.350.0"
    license = "Apache-2.0"

    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        self.requires(f"spirv-headers", transitive_headers=True)

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz",
            sha256="c81ea49449d77189574ce0ff1374350533c283f41a9dd1b59f351c26961302b9",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPIRV_REFLECT_STATIC_LIB"] = True
        tc.variables["SPIRV_REFLECT_EXAMPLES"] = False
        tc.variables["SPIRV_REFLECT_BUILD_TESTS"] = False
        tc.generate()

    def build_requirements(self):
        self.tool_requires("cmake")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "spirv-reflect-static")
        self.cpp_info.set_property("cmake_target_name", "spirv-reflect-static")
        self.cpp_info.libs = ["spirv-reflect-static"]
        self.cpp_info.defines.append("SPIRV_REFLECT_USE_SYSTEM_SPIRV_H")
