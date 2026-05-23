from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import copy, get
import os

class Recipe(RecipeBase):
    name = "fast_float"
    version = "8.1.0"
    license = ("Apache-2.0", "MIT", "BSL-1.0")
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def source(self):
        get(self, url="https://github.com/fastfloat/fast_float/archive/refs/tags/v8.1.0.tar.gz", sha256="4bfabb5979716995090ce68dce83f88f99629bc17ae280eae79311c5340143e1", destination=self.source_folder, strip_root=True)

    def build(self):
        pass

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "FastFloat")
        self.cpp_info.set_property("cmake_target_name", "FastFloat::fast_float")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # TODO: to remove in conan v2 once legacy generators removed
        self.cpp_info.names["cmake_find_package"] = "FastFloat"
        self.cpp_info.names["cmake_find_package_multi"] = "FastFloat"
        self.cpp_info.components["fastfloat"].names["cmake_find_package"] = "fast_float"
        self.cpp_info.components["fastfloat"].names["cmake_find_package_multi"] = "fast_float"
        self.cpp_info.components["fastfloat"].set_property("cmake_target_name", "FastFloat::fast_float")
        self.cpp_info.components["fastfloat"].bindirs = []
        self.cpp_info.components["fastfloat"].libdirs = []
