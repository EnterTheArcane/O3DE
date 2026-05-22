# Ported from conan-center-index/wil by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, get, copy
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "wil"
    license = "MIT"

    @property
    def _min_cppstd(self):
        return 11

    @property
    def _compilers_minimum_version(self):
        # About compiler version: https://github.com/microsoft/wil/issues/207#issuecomment-991722592 
        return {
            "Visual Studio": "15",
            "msvc": "191"
        }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def build(self):
        apply_patches(self)

    def package(self):
        copy(pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        copy(
            self,
            pattern="*.h",
            dst=os.path.join(self.package_folder, "include"),
            src=os.path.join(self.source_folder, "include"),
        )
