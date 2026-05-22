import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get


class Recipe(RecipeBase):
    name = "vulkan-utility-libraries"
    version = "1.4.313.0"
    license = "Apache-2.0"

    def requirements(self) -> list[str]:
        return ["vulkan-headers"]

    def source(self):
        get(
            url="https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/refs/tags/vulkan-sdk-1.4.313.0.tar.gz",
            sha256="3e04f32c6023997c153ad4b63e2fd344257e40a57ff5229ab7373e08a4fa2dd2",
            dest=self.source_folder,
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTS"] = False
        tc.cache_variables["VUL_WERROR"] = False
        tc.cache_variables["VUL_ENABLE_INSTALL"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # The native cmake config defines Vulkan::LayerSettings, Vulkan::SafeStruct,
        # Vulkan::UtilityHeaders, and Vulkan::CompilerConfiguration.  Point CMakeDeps
        # to the installed config so all four targets are available.
        self.cpp_info.set_property("cmake_file_name", "VulkanUtilityLibraries")
        self.cpp_info.set_property("cmake_target_name", "Vulkan::LayerSettings")
        self.cpp_info.set_property(
            "cmake_package_file",
            "lib/cmake/VulkanUtilityLibraries/VulkanUtilityLibrariesConfig.cmake",
        )
