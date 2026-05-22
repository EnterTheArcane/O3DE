# Ported from conan-center-index/metal-cpp by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy

import os

class Recipe(RecipeBase):
    name = "metal-cpp"
    license = "Apache-2.0"

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def package(self):
        if not self.is_macos:
            return  # metal-cpp is macOS only
        copy("LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy("**.hpp", src=self.source_folder, dst=os.path.join(self.package_folder, "include"))
