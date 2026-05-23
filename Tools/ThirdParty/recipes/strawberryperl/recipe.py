from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "strawberryperl"
    version = "5.40.2.1"
    license = ("Artistic-1.0", "GPL-1.0")
    package_type = "application"
    settings = "os", "arch", "compiler", "build_type"
    def compatibility(self):
        if self.settings.arch == "armv8":
            return [{"settings": [("arch", "x86_64")]}]

    def source(self):
        pass

    def build(self):
        get(
            self,
            url="https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54021_64bit_UCRT/strawberry-perl-5.40.2.1-64bit-portable.zip",
            sha256="7707700d5ad027773b775134fe48cd9610abf221433fcfb68c8eb0ec9c6fde8c",
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

        # TODO remove once conan v2 is the only support and recipes have been migrated
        if Version(conan_version).major < 2:
            bin_path = os.path.join(self.package_folder, "bin")
            self.env_info.PATH.append(bin_path)
            self.user_info.perl = perl_path
