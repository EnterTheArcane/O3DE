from thirdparty import RecipeBase
from thirdparty.tools.files import apply_conandata_patches, copy, get, load, save
from thirdparty.tools.microsoft import is_msvc
import os

class Recipe(RecipeBase):
    name = "getopt-for-visual-studio"
    version = "20200201"
    license = "MIT", "BSD-2-Clause"

    def source(self):
        get(
            self,
            url="https://github.com/skandhurkat/Getopt-for-Visual-Studio/archive/6708172892a4d89042b743315e8a52e2d9d5defc.zip",
            sha256="9b50026b3f10c3f6a7340e0074a898d6d1105eef068bf98d90af99770375a465",
            destination=self.source_folder,
            strip_root=True)

    def build(self):
        apply_conandata_patches(self)

    @property
    def _license_text(self):
        content = load(self, os.path.join(self.source_folder, "getopt.h"))
        return "\n".join(list(l.strip() for l in content[content.find("/**", 3):content.find("#pragma")].split("\n")))

    def package(self):
        save(self, os.path.join(self.package_folder, "licenses", "LICENSE"), self._license_text)
        copy(self, "getopt.h", src=self.source_folder, dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
