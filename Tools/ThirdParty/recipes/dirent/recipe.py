from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
import os

class Recipe(RecipeBase):
    name = "dirent"
    version = "1.24"
    license = "MIT"
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def source(self):
        get(self, url="https://github.com/tronkko/dirent/archive/1.24.tar.gz", sha256="37009127a65bb1ddc47d06c097321f87f45ca2e998b2ec3bf2e0b2b19649d6f9", destination=self.source_folder, strip_root=True)

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        copy(
            self,
            pattern="*.h",
            dst=os.path.join(self.package_folder, "include"),
            src=os.path.join(self.source_folder, "include"),
        )

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
