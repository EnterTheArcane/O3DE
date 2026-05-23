from thirdparty import RecipeBase
from thirdparty.tools.apple import XCRun
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import get, copy
from thirdparty.tools.scm import Version

import os
import platform

class Recipe(RecipeBase):
    name = "metal-cpp"
    version = "26"
    license = "Apache-2.0"

    def source(self):
        get(
            self,
            url="https://developer.apple.com/metal/cpp/files/metal-cpp_26.zip",
            sha256="4df3c078b9aadcb516212e9cb03004cbc5ce9a3e9c068fa3144d021db585a3a4",
            destination=self.source_folder,
            strip_root=True)

    def package(self):
        copy(
            self,
            pattern="LICENSE.txt",
            dst=os.path.join(self.package_folder, "licenses"),
            src=os.path.join(self.source_folder)
        )
        copy(
            self,
            pattern="**.hpp",
            dst=os.path.join(self.package_folder, "include"),
            src=os.path.join(self.source_folder),
            keep_path=True
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "metal-cpp")
        self.cpp_info.set_property("cmake_target_name", "metal-cpp::metal-cpp")
        self.cpp_info.set_property("pkg_config_name", "metal-cpp")
        self.cpp_info.bindirs = []
        self.cpp_info.frameworkdirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.resdirs = []

        self.cpp_info.frameworks = ["Foundation", "Metal", "MetalKit", "QuartzCore"]
        self.cpp_info.frameworks.append("MetalFX")
