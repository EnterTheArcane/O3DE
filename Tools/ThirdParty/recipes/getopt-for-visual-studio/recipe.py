from thirdparty import RecipeBase
from thirdparty.files import apply_patches, copy, get, load, save


class Recipe(RecipeBase):
    name = "getopt-for-visual-studio"
    version = "20200201"
    license = "MIT", "BSD-2-Clause"

    def source(self):
        get(
            self,
            url="https://github.com/skandhurkat/Getopt-for-Visual-Studio/archive/6708172892a4d89042b743315e8a52e2d9d5defc.zip",
            sha256="9b50026b3f10c3f6a7340e0074a898d6d1105eef068bf98d90af99770375a465",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    @property
    def _license_text(self):
        content = load(self, self.folders.source / "getopt.h")
        return "\n".join(list(l.strip() for l in content[content.find("/**", 3):content.find("#pragma")].split("\n")))

    def package(self):
        save(self, self.folders.package / "licenses" / "LICENSE", self._license_text)
        copy(self, "getopt.h", src=self.folders.source, dst=self.folders.package / "include")

    def package_info(self):
        self.info.bindirs = []
        self.info.libdirs = []
