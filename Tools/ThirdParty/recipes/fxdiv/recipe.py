from thirdparty import RecipeBase
from thirdparty.files import get, copy


class Recipe(RecipeBase):
    name = "fxdiv"
    version = "cci.20200417"
    license = "MIT"

    def source(self):
        get(
            self,
            url="https://github.com/Maratyszcza/FXdiv/archive/b408327ac2a15ec3e43352421954f5b1967701d1.zip",
            sha256="ab7dfb08829bee33dca38405d647868fb214ac685e379ec7ef2bebcd234cd44d",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "*", src=self.folders.source / "include", dst=self.folders.package / "include")

    def package_info(self):
        self.info.set_property("cmake_file_name", "fxdiv")
        self.info.set_property("cmake_target_name", "fxdiv::fxdiv")
        self.info.bindirs = []
        self.info.libdirs = []
