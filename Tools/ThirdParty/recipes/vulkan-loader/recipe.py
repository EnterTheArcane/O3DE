import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "vulkan-loader"
    version = "1.4.350.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("vulkan-headers")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-Loader")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/Vulkan-Loader/archive/vulkan-sdk-1.4.350.0.tar.gz",
            sha256="91f88fc43abb36821a568c7fbb3f815e9baf946d5fe187928df279708d45e509",
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(
            self,
            os.path.join(self.source_folder, "CMakeLists.txt"),
            "set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")",
            "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = False
        tc.variables["LOADER_CODEGEN"] = False
        vulkan_headers = self.dependencies["vulkan-headers"].package_folder.replace("\\", "/")
        tc.variables["VULKAN_HEADERS_INSTALL_DIR"] = vulkan_headers
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "loader"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "VulkanLoader")
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_module_file_name", "Vulkan")
        self.cpp_info.set_property("cmake_target_name", "Vulkan::Loader")
        self.cpp_info.set_property("cmake_target_aliases", ["Vulkan::Vulkan"])
        self.cpp_info.set_property("cmake_module_target_name", "Vulkan::Vulkan")
        self.cpp_info.includedirs = []
        if self.settings.os == "Windows":
            self.cpp_info.libs = ["vulkan-1"]
        else:
            self.cpp_info.libs = ["vulkan"]
