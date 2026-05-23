from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "tsl-robin-map"
    version = "1.4.0"
    license = "MIT"
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def source(self):
        get(self, url="https://github.com/Tessil/robin-map/archive/v1.4.0.tar.gz", sha256="7930dbf9634acfc02686d87f615c0f4f33135948130b8922331c16d90a03250c", destination=self.source_folder, strip_root=True)

    def build(self):
        pass

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "tsl-robin-map")
        self.cpp_info.set_property("cmake_target_name", "tsl::robin_map")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # TODO: to remove in conan v2 once cmake_find_package* generators removed
        self.cpp_info.filenames["cmake_find_package"] = "tsl-robin-map"
        self.cpp_info.filenames["cmake_find_package_multi"] = "tsl-robin-map"
        self.cpp_info.names["cmake_find_package"] = "tsl"
        self.cpp_info.names["cmake_find_package_multi"] = "tsl"
        self.cpp_info.components["robin_map"].names["cmake_find_package"] = "robin_map"
        self.cpp_info.components["robin_map"].names["cmake_find_package_multi"] = "robin_map"
        self.cpp_info.components["robin_map"].set_property("cmake_target_name", "tsl::robin_map")
        self.cpp_info.components["robin_map"].bindirs = []
        self.cpp_info.components["robin_map"].libdirs = []
