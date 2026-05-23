from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.cmake import CMakeToolchain, CMake
import os

class Recipe(RecipeBase):
    name = "utfcpp"
    version = "4.0.9"
    license = "BSL-1.0"
    no_copy_source = True

    def source(self):
        get(self, url="https://github.com/nemtrif/utfcpp/archive/v4.0.9.tar.gz", sha256="397a9a2a6ed5238f854f490b0177b840abc6b62571ec3e07baa0bb94d3f14d5a", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "utf8cpp")
        self.cpp_info.set_property("cmake_target_name", "utf8cpp::utf8cpp")
        # FIXME: Keep CMake target utf8cpp for backward compatibility as more projects are using it in CCI.
        self.cpp_info.set_property("cmake_target_aliases", ["utf8::cpp", "utf8cpp"])
        self.cpp_info.includedirs.append(os.path.join("include", "utf8cpp"))

        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
