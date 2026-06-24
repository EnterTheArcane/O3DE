import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "vulkan-headers"
    version = "1.4.352"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-Headers")
        return Version(repo.latest_release.removeprefix("vulkan-sdk-").lstrip("v"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.352.tar.gz",
            sha256="4850909d22a8a9767c27daea2b972e49d7c298560573d5b6221ee50db9bf49db",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["VULKAN_HEADERS_ENABLE_MODULE"] = False
        tc.cache_variables["VULKAN_HEADERS_ENABLE_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        copy(self, "*", src=os.path.join(self.folders.package, "share", "vulkan", "registry"), dst=os.path.join(self.folders.package, "res", "vulkan", "registry"))
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "VulkanHeaders")
        self.info.components["vulkanheaders"].set_property("cmake_target_name", "Vulkan::Headers")
        self.info.components["vulkanheaders"].bindirs = []
        self.info.components["vulkanheaders"].libdirs = []

        self.info.components["vulkanregistry"].set_property("cmake_target_name", "Vulkan::Registry")
        self.info.components["vulkanregistry"].includedirs = [os.path.join("res", "vulkan", "registry")]
        self.info.components["vulkanregistry"].bindirs = []
        self.info.components["vulkanregistry"].libdirs = []
        self.info.components["vulkanregistry"].resdirs = ["res"]
