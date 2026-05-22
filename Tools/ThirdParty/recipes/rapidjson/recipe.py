from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
import os


class Recipe(RecipeBase):
    name = "rapidjson"
    version = "1.1.0"
    license = "MIT"
    package_id_embed_mode = "minor_mode"

    def source(self):
        get(
            url="https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz",
            sha256="bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
            strip_root=True,
            dest=self.source_folder,
        )

    def package(self):
        copy(
            pattern="license.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            pattern="*",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
