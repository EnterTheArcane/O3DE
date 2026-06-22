import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "vulkan-tools"
    version = "1.4.350.0"
    license = "Apache-2.0"

    options = {
        "build_cube": [True, False],
        "build_vulkaninfo": [True, False],
        "build_icd": [True, False],
    }
    default_options = {
        "build_cube": True,
        "build_vulkaninfo": True,
        "build_icd": True,
    }

    def requirements(self):
        self.requires("vulkan-headers")
        self.requires("vulkan-loader")

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-Tools")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/Vulkan-Tools/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz",
            sha256="3079796d51b29ce49dc7b7c7e243df93b343d54c3be9d4a8292c3231b9698deb",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        vulkan_headers_pkg = self.dependencies["vulkan-headers"].folders.package
        tc.variables["VULKAN_HEADERS_INSTALL_DIR"] = vulkan_headers_pkg.replace("\\", "/")
        tc.variables["BUILD_CUBE"] = self.options.build_cube
        tc.variables["BUILD_VULKANINFO"] = self.options.build_vulkaninfo
        tc.variables["BUILD_ICD"] = self.options.build_icd
        tc.variables["VULKAN_TOOLS_TESTS"] = False
        if self.settings.os == "Mac":
            # Use system ICD discovery instead of requiring MoltenVK source tree layout
            tc.variables["APPLE_USE_SYSTEM_ICD"] = True
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.bindirs = ["bin"]
