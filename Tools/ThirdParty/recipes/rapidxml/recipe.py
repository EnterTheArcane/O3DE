from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, apply_conandata_patches
import os

class Recipe(RecipeBase):
    name = "rapidxml"
    version = "1.13"
    license = ["BSL-1.0", "MIT"]

    def source(self):
        get(self, url="https://sourceforge.net/projects/rapidxml/files/rapidxml/rapidxml%201.13/rapidxml-1.13.zip/download", sha256="c3f0b886374981bb20fabcf323d755db4be6dba42064599481da64a85f5b3571", destination=self.source_folder, strip_root=True)

    def build(self):
        apply_conandata_patches(self)

    def package(self):
        copy(self, "license.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.hpp", src=self.source_folder, dst=os.path.join(self.package_folder, "include", "rapidxml"))

    def package_info(self):
        self.cpp_info.includedirs.append(os.path.join("include", "rapidxml"))
        self.cpp_info.bindirs = []
        self.cpp_info.frameworkdirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.resdirs = []
