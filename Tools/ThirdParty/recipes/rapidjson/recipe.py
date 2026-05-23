from thirdparty import RecipeBase
from thirdparty.tools.files import get, copy
import os

class Recipe(RecipeBase):
    name = "rapidjson"
    version = "1.1.0"
    license = "MIT"
    package_type = "header-library"
    package_id_embed_mode = "minor_mode"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def source(self):
        get(self, url="https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz", sha256="bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e", strip_root=True,
                    destination=self.source_folder)

    def package(self):
        copy(self, pattern="license.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, pattern="*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "RapidJSON")
        self.cpp_info.set_property("cmake_target_name", "rapidjson")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # TODO: to remove in conan v2 once cmake_find_package* generators removed
        self.cpp_info.names["cmake_find_package"] = "RapidJSON"
        self.cpp_info.names["cmake_find_package_multi"] = "RapidJSON"
