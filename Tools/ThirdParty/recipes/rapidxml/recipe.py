# Ported from conan-center-index/rapidxml by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, apply_patches
import os

class Recipe(RecipeBase):
    name = "rapidxml"
    license = ["BSL-1.0", "MIT"]

    def source(self):
        get(**self.thirdparty_data["versions"][self.version],
            dest=self.source_folder, strip_root=True)

    def build(self):
        apply_patches(self)

    def package(self):
        copy("license.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy("*.hpp", src=self.source_folder, dst=os.path.join(self.package_folder, "include", "rapidxml"))
