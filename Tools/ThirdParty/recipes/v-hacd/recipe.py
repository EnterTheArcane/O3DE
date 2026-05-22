# Ported from conan-center-index/v-hacd by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "v-hacd"
    license = "BSD-3-Clause"

    @property
    def _min_cppstd(self):
        return "11"

    @property
    def _compilers_minimum_version(self):
        return {
            "gcc": "6",
        }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def build(self):
        pass

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy("*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
