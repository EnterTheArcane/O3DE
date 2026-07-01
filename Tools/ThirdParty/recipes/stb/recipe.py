from thirdparty import RecipeBase
from thirdparty.files import copy, get, rmdir


class Recipe(RecipeBase):
    name = "stb"
    version = "20240531"
    license = "MIT", "Unlicense"

    def source(self):
        get(
            self,
            url="https://github.com/nothings/stb/archive/013ac3beddff3dbffafd5177e7972067cd2b5083.zip",
            sha256="b7f476902bbef1b30f8ecc2d9d95c459c32302c8b559d09b589b5955463b7af8",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "*.h", src=self.folders.source, dst=self.folders.package / "include")
        copy(self, "stb_vorbis.c", src=self.folders.source, dst=self.folders.package / "include")
        rmdir(self, self.folders.package / "include" / "tests")
        rmdir(self, self.folders.package / "include" / "deprecated")
        copy(self, "*.h", src=self.folders.source / "deprecated", dst=self.folders.package / "include")
        copy(self, "stb_image.c", src=self.folders.source / "deprecated", dst=self.folders.package / "include")

    def package_info(self):
        self.info.bindirs = []
        self.info.libdirs = []
        self.info.defines.append("STB_TEXTEDIT_KEYTYPE=unsigned")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")

    @property
    def _version(self):
        return str(self.version)[4:]
