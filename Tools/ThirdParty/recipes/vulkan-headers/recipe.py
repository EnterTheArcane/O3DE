import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir

class Recipe(RecipeBase):
    name = "vulkan-headers"
    version = "1.4.350.0"
    license = "Apache-2.0"
    no_copy_source = True

    def source(self):
        get(self, url="https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz", sha256="70270d10bf2c1e074a06ee37a50b75d332993d1b80a1d9526eeed2da6d82ed22", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["VULKAN_HEADERS_ENABLE_MODULE"] = False
        tc.cache_variables["VULKAN_HEADERS_ENABLE_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        copy(self, "*", src=os.path.join(self.package_folder, "share", "vulkan", "registry"), dst=os.path.join(self.package_folder, "res", "vulkan", "registry"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "VulkanHeaders")
        self.cpp_info.components["vulkanheaders"].set_property("cmake_target_name", "Vulkan::Headers")
        self.cpp_info.components["vulkanheaders"].bindirs = []
        self.cpp_info.components["vulkanheaders"].libdirs = []

        self.cpp_info.components["vulkanregistry"].set_property("cmake_target_name", "Vulkan::Registry")
        self.cpp_info.components["vulkanregistry"].includedirs = [os.path.join("res", "vulkan", "registry")]
        self.cpp_info.components["vulkanregistry"].bindirs = []
        self.cpp_info.components["vulkanregistry"].libdirs = []
        self.cpp_info.components["vulkanregistry"].resdirs = ["res"]
