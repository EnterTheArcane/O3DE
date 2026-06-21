import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain, CMakeConfigDeps
from thirdparty.files import copy, get, rmdir, replace_in_file
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "vulkan-utility-libraries"
    version = "1.4.352"
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
        self.requires(f"vulkan-headers", transitive_headers=True)

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-Utility-Libraries")
        return Version(repo.latest_release.removeprefix("vulkan-sdk-").lstrip("v"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/refs/tags/v1.4.352.tar.gz",
            sha256="a8dd82f0f52714a2a1c9deae1e3b21553a7e312aae50445ee9ab7f2dfc1b90c6",
            destination=self.source_folder,
            strip_root=True)
        for text in ["set(CMAKE_CXX_STANDARD 17)", "set(CMAKE_CXX_STANDARD_REQUIRED ON)",
                     "set(CMAKE_POSITION_INDEPENDENT_CODE ON)"]:
            replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                            text, "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTS"] = False
        tc.cache_variables["VUL_ENABLE_INSTALL"] = True
        tc.generate()

        deps = CMakeConfigDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "VulkanUtilityLibraries")

        self.cpp_info.components["UtilityHeaders"].libs = []
        self.cpp_info.components["UtilityHeaders"].includedirs = ["include"]
        self.cpp_info.components["UtilityHeaders"].set_property("cmake_target_name", "Vulkan::UtilityHeaders")
        self.cpp_info.components["UtilityHeaders"].requires = ["vulkan-headers::vulkan-headers"]

        self.cpp_info.components["SafeStruct"].libs = ["VulkanSafeStruct"]
        self.cpp_info.components["SafeStruct"].set_property("cmake_target_name", "Vulkan::SafeStruct")
        self.cpp_info.components["SafeStruct"].requires = ["UtilityHeaders", "vulkan-headers::vulkan-headers"]

        self.cpp_info.components["LayerSettings"].libs = ["VulkanLayerSettings"]
        self.cpp_info.components["LayerSettings"].set_property("cmake_target_name", "Vulkan::LayerSettings")
        self.cpp_info.components["LayerSettings"].requires = ["UtilityHeaders", "vulkan-headers::vulkan-headers"]
