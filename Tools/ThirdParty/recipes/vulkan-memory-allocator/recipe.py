# Ported from conan-center-index/vulkan-memory-allocator by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, copy, get
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "vulkan-memory-allocator"
    license = "MIT"

    @property
    def _min_cppstd(self):
        return "11" if Version(self.version) < "3.0.0" else "14"

    def requirements(self) -> list[str]:
        return ["vulkan-headers"]

    def source(self):
        get(**self.thirdparty_data["versions"][self.version],
            dest=self.source_folder, strip_root=True)

    def build(self):
        apply_patches(self)

    def package(self):
        copy("LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        if Version(self.version) < "3.0.0":
            include_dir = os.path.join(self.source_folder, "src")
        else:
            include_dir = os.path.join(self.source_folder, "include")
        copy("vk_mem_alloc.h", src=include_dir, dst=os.path.join(self.package_folder, "include"))
