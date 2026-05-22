from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import copy, get, rmdir, replace_in_file
from thirdparty.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import os

class Recipe(ConanFile):
    name = "vulkan-utility-libraries"
    version = "1.4.350.0"
    license = "Apache-2.0"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
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
        self.requires(f"vulkan-headers/{self.version}", transitive_headers=True)

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.22.1]")

    def source(self):
        get(self, url="https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz", sha256="19a215e9469df0749d7c1b389fc667a3f7e160f0b6da71000fadc30140494563", destination=self.source_folder, strip_root=True)
        for text in ["set(CMAKE_CXX_STANDARD 17)", "set(CMAKE_CXX_STANDARD_REQUIRED ON)",
                     "set(CMAKE_POSITION_INDEPENDENT_CODE ON)"]:
            replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                            text, "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTS"] = False
        tc.cache_variables["VUL_ENABLE_INSTALL"] = True
        tc.generate()

        deps = CMakeDeps(self)
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
