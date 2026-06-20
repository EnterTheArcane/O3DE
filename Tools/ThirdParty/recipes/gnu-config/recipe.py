import os

from thirdparty import RecipeBase
from thirdparty.files import copy, load, save, apply_patches
from thirdparty.scm import Git


class Recipe(RecipeBase):
    name = "gnu-config"
    version = "2021.08.14"
    license = "GPL-3.0-or-later", "autoconf-special-exception"

    def source(self):
        git = Git(self, self.source_folder)
        git.clone(
            url="https://git.savannah.gnu.org/git/config.git",
            target=".",
            args=["--depth", "1"])
        apply_patches(self)

    def build(self):
        pass

    def _extract_license(self):
        txt_lines = load(self, os.path.join(self.source_folder, "config.guess")).splitlines()
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
            raise ConanException("Failed to extract the license")
        return "\n".join(txt_lines[start_index:end_index])

    def package(self):
        save(self, os.path.join(self.package_folder, "licenses", "COPYING"), self._extract_license())
        copy(self, "config.guess", src=self.source_folder, dst=os.path.join(self.package_folder, "bin"))
        copy(self, "config.sub", src=self.source_folder, dst=os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []

        bin_path = os.path.join(self.package_folder, "bin")
        self.conf_info.define("user.gnu-config:config_guess", os.path.join(bin_path, "config.guess"))
        self.conf_info.define("user.gnu-config:config_sub", os.path.join(bin_path, "config.sub"))
