from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
import os

class Recipe(ConanFile):
    name = "v-hacd"
    version = "4.1.0"
    license = "BSD-3-Clause"
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    @property
    def _min_cppstd(self):
        return "11"

    @property
    def _compilers_minimum_version(self):
        return {
            "gcc": "6",
        }

    def source(self):
        get(self, url="https://github.com/kmammou/v-hacd/archive/refs/tags/v4.1.0.tar.gz", sha256="9fe895cd10ec995d2171b11bde97aaaa221b418a3aaed0f5d9a068ae057d626b", destination=self.source_folder, strip_root=True)

    def build(self):
        pass

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
