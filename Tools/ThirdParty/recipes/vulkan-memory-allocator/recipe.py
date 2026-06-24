import os

from thirdparty import RecipeBase
from thirdparty.files import apply_patches, copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "vulkan-memory-allocator"
    version = "3.3.0"
    license = "MIT"

    def requirements(self):
        self.requires("vulkan-headers")

    def latest_version(self):
        repo = GithubRepository(self, "GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz",
            sha256="c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        include_dir = os.path.join(self.folders.source, "include")
        copy(self, "vk_mem_alloc.h", src=include_dir, dst=os.path.join(self.folders.package, "include"))

    def package_info(self):
        self.info.bindirs = []
        self.info.libdirs = []
        self.info.set_property("cmake_file_name", "VulkanMemoryAllocator")
        self.info.set_property("cmake_target_name", "GPUOpen::VulkanMemoryAllocator")
        self.info.set_property("cmake_target_aliases", ["VulkanMemoryAllocator", ])
