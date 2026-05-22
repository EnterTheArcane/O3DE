from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
import os


class Recipe(RecipeBase):
    name = "vulkan-headers"
    version = "1.4.313.0"
    license = "Apache-2.0"

    def source(self):
        get(
            url="https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.313.0.tar.gz",
            dest=self.source_folder,
            sha256="20743c99a96c07290f24377360e7a12bdd2c465ba202e0c7ef2ec25d446cf61d",
        )

    def build(self):
        pass

    def package(self):
        copy(
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            "*",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        copy(
            "*",
            src=os.path.join(self.source_folder, "registry"),
            dst=os.path.join(self.package_folder, "res", "vulkan", "registry"),
        )
