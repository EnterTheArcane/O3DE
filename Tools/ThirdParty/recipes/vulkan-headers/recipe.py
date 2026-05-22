# Ported from conan-center-index/vulkan-headers by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
import os

class Recipe(RecipeBase):
    name = "vulkan-headers"
    license = "Apache-2.0"

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def build(self):
        pass

    def package(self):
        copy("LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy("*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
        copy("*", src=os.path.join(self.source_folder, "registry"), dst=os.path.join(self.package_folder, "res", "vulkan", "registry"))
