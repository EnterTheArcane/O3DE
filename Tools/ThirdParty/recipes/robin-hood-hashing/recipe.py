from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import apply_conandata_patches, copy, get
import os

class Recipe(RecipeBase):
    name = "robin-hood-hashing"
    version = "3.11.5"
    license = "MIT"

    def source(self):
        get(self, url="https://github.com/martinus/robin-hood-hashing/archive/refs/tags/3.11.5.tar.gz", sha256="3693e44dda569e9a8b87ce8263f7477b23af448a3c3600c8ab9004fe79c20ad0", destination=self.source_folder, strip_root=True)

    def build(self):
        apply_conandata_patches(self)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "robin_hood.h", src=os.path.join(self.source_folder, "src", "include"),
                                   dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "robin_hood")
        self.cpp_info.set_property("cmake_target_name", "robin_hood::robin_hood")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # TODO: to remove in conan v2 once cmake_find_package_* generators removed
        self.cpp_info.names["cmake_find_package"] = "robin_hood"
        self.cpp_info.names["cmake_find_package_multi"] = "robin_hood"
