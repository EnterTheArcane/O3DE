# Ported from conan-center-index/robin-hood-hashing by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, copy, get
import os

class Recipe(RecipeBase):
    name = "robin-hood-hashing"
    license = "MIT"

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def build(self):
        apply_patches(self)

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy("robin_hood.h", src=os.path.join(self.source_folder, "src", "include"),
                                   dst=os.path.join(self.package_folder, "include"))
