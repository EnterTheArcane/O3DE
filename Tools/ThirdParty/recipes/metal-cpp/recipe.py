# Ported from conan-center-index/metal-cpp by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy

import os


class Recipe(RecipeBase):
    name = "metal-cpp"
    version = "26"
    license = "Apache-2.0"

    def source(self):
        get(
            url="https://developer.apple.com/metal/cpp/files/metal-cpp_26.zip",
            dest=self.source_folder,
            sha256="4df3c078b9aadcb516212e9cb03004cbc5ce9a3e9c068fa3144d021db585a3a4",
        )

    def package(self):
        if not self.is_macos:
            return  # metal-cpp is macOS only
        copy(
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            "**.hpp",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "include"),
        )
