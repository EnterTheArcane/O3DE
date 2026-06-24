import os

from thirdparty import RecipeBase
from thirdparty.files import get, copy


class Recipe(RecipeBase):
    name = "metal-cpp"
    version = "26"
    license = "Apache-2.0"

    def source(self):
        get(
            self,
            url="https://developer.apple.com/metal/cpp/files/metal-cpp_26.zip",
            sha256="4df3c078b9aadcb516212e9cb03004cbc5ce9a3e9c068fa3144d021db585a3a4",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(
            self,
            pattern="LICENSE.txt",
            dst=os.path.join(self.folders.package, "licenses"),
            src=os.path.join(self.folders.source))
        copy(
            self,
            pattern="**.hpp",
            dst=os.path.join(self.folders.package, "include"),
            src=os.path.join(self.folders.source),
            keep_path=True)

    def package_info(self):
        self.info.set_property("cmake_file_name", "metal-cpp")
        self.info.set_property("cmake_target_name", "metal-cpp::metal-cpp")
        self.info.set_property("pkg_config_name", "metal-cpp")
        self.info.bindirs = []
        self.info.frameworkdirs = []
        self.info.libdirs = []
        self.info.resdirs = []

        self.info.frameworks = ["Foundation", "Metal", "MetalKit", "QuartzCore"]
        self.info.frameworks.append("MetalFX")
