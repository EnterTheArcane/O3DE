# Ported from conan-center-index/rapidjson by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
import os

class Recipe(RecipeBase):
    name = "rapidjson"
    license = "MIT"
    package_id_embed_mode = "minor_mode"

    def source(self):
        get(**self.thirdparty_data["versions"][self.version], strip_root=True,
                    dest=self.source_folder)

    def package(self):
        copy(pattern="license.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(pattern="*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
