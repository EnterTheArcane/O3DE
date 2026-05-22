from thirdparty import RecipeBase
from thirdparty.tools.files import apply_patches, get, copy
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "wil"
    version = "1.0.250325.1"
    license = "MIT"

    @property
    def _min_cppstd(self):
        return 11

    @property
    def _compilers_minimum_version(self):
        # About compiler version: https://github.com/microsoft/wil/issues/207#issuecomment-991722592
        return {"Visual Studio": "15", "msvc": "191"}

    def source(self):
        get(
            url="https://github.com/microsoft/wil/archive/refs/tags/v1.0.250325.1.tar.gz",
            dest=self.source_folder,
            sha256="c9e667d5f86ded43d17b5669d243e95ca7b437e3a167c170805ffd4aa8a9a786",
        )

    def build(self):
        apply_patches(self)

    def package(self):
        copy(
            pattern="LICENSE",
            dst=os.path.join(self.package_folder, "licenses"),
            src=self.source_folder,
        )
        copy(
            pattern="*.h",
            dst=os.path.join(self.package_folder, "include"),
            src=os.path.join(self.source_folder, "include"),
        )
