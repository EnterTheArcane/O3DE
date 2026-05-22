# Ported from conan-center-index/robin-hood-hashing by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, copy, get
import os


class Recipe(RecipeBase):
    name = "robin-hood-hashing"
    version = "3.11.5"
    license = "MIT"

    def source(self):
        get(
            url="https://github.com/martinus/robin-hood-hashing/archive/refs/tags/3.11.5.tar.gz",
            dest=self.source_folder,
            sha256="3693e44dda569e9a8b87ce8263f7477b23af448a3c3600c8ab9004fe79c20ad0",
        )

    def build(self):
        apply_patches(self)

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            "robin_hood.h",
            src=os.path.join(self.source_folder, "src", "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
