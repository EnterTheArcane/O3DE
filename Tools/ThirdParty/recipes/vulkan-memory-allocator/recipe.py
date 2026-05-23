from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import apply_conandata_patches, copy, get
import os

class Recipe(RecipeBase):
    name = "vulkan-memory-allocator"
    version = "3.3.0"
    license = "MIT"

    @property
    def _min_cppstd(self):
        return "14"

    def requirements(self):
        self.requires("vulkan-headers")

    def source(self):
        get(
            self,
            url="https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz",
            sha256="c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e",
            destination=self.source_folder,
            strip_root=True)

    def build(self):
        apply_conandata_patches(self)

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        include_dir = os.path.join(self.source_folder, "include")
        copy(self, "vk_mem_alloc.h", src=include_dir, dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "VulkanMemoryAllocator")
        self.cpp_info.set_property("cmake_target_name", "GPUOpen::VulkanMemoryAllocator")
        self.cpp_info.set_property("cmake_target_aliases", ["VulkanMemoryAllocator",])
