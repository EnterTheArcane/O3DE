from thirdparty import RecipeBase
from thirdparty.errors import RecipeException
from thirdparty.files import copy, get, load, save, apply_patches


class Recipe(RecipeBase):
    name = "gnu-config"
    version = "2021.08.14"
    license = "GPL-3.0-or-later", "autoconf-special-exception"

    def source(self):
        get(
            self,
            url="https://github.com/build2/config/archive/191bcb948f7191c36eefe634336f5fc5c0c4c2be.tar.gz",
            sha256="302e5e7f3c4996976c58efde8b2f28f71d51357e784330eeed738e129300dc33",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def build(self):
        pass

    def package(self):
        save(self, self.folders.package / "licenses" / "COPYING", self._extract_license())
        copy(self, "config.guess", src=self.folders.source, dst=self.folders.package / "bin")
        copy(self, "config.sub", src=self.folders.source, dst=self.folders.package / "bin")

    def package_info(self):
        self.info.includedirs = []
        self.info.libdirs = []

        bin_path = self.folders.package / "bin"
        self.info.conf.define("user.gnu-config:config_guess", bin_path / "config.guess")
        self.info.conf.define("user.gnu-config:config_sub", bin_path / "config.sub")

    def _extract_license(self):
        txt_lines = load(self, self.folders.source / "config.guess").splitlines()
        start_index = None
        end_index = None
        for line_i, line in enumerate(txt_lines):
            if start_index is None:
                if "This file is free" in line:
                    start_index = line_i
            if end_index is None:
                if "Please send patches" in line:
                    end_index = line_i
        if not all((start_index, end_index)):
            raise RecipeException("Failed to extract the license")
        return "\n".join(txt_lines[start_index:end_index])
