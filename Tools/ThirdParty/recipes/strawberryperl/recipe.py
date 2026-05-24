import os
import re

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "strawberryperl"
    version = "5.42.2.1"
    license = "Artistic-1.0", "GPL-1.0"

    def compatibility(self):
        if self.settings.arch == "armv8":
            return [{"settings": [("arch", "x86_64")]}]

    def latest_version(self):
        repo = GithubRepository(self, "StrawberryPerl/Perl-Dist-Strawberry")
        tag = repo.latest_release_matching(r"SP_")
        m = re.match(r"SP_(\d+)_", tag)
        if not m:
            raise RuntimeError(f"unexpected tag: {tag}")
        digits = m.group(1)
        return Version(".".join([digits[0], digits[1:3]] + list(digits[3:])))

    def build(self):
        get(
            self,
            url="https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54221_64bit/strawberry-perl-5.42.2.1-64bit-portable.zip",
            sha256="32d83be90cf04b807cfb9477482bc36302cdee6f5b04cf57e81adecbd8f07898",
            destination=self.build_folder
        )

    def package(self):
        copy(self, pattern="License.rtf*", src=os.path.join(self.build_folder, "licenses"), dst=os.path.join(self.package_folder, "licenses"))
        copy(self, pattern="*", src=os.path.join(self.build_folder, "perl", "bin"), dst=os.path.join(self.package_folder, "bin"))
        copy(self, pattern="*", src=os.path.join(self.build_folder, "perl", "lib"), dst=os.path.join(self.package_folder, "lib"))
        copy(self, pattern="*", src=os.path.join(self.build_folder, "perl", "vendor", "lib"), dst=os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []

        perl_path = os.path.join(self.package_folder, "bin", "perl.exe").replace("\\", "/")
        self.conf_info.define("user.strawberryperl:perl", perl_path)
