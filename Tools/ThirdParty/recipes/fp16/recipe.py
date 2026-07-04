from thirdparty import RecipeBase
from thirdparty.files import get, copy


class Recipe(RecipeBase):
    name = "fp16"
    version = "20210320"
    license = "MIT"

    def source(self):
        get(
            self,
            url="https://github.com/Maratyszcza/FP16/archive/0a92994d729ff76a58f692d3028ca1b64b145d91.zip",
            sha256="e66e65515fa09927b348d3d584c68be4215cfe664100d01c9dbc7655a5716d70",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "*.h", src=self.folders.source / "include", dst=self.folders.package / "include")

    def package_info(self):
        self.info.set_property("cmake_file_name", "fp16")
        self.info.set_property("cmake_target_name", "fp16::fp16")
        self.info.bindirs = []
        self.info.libdirs = []
