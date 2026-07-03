from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.env import VirtualBuildEnv
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.autotools import Autotools, AutotoolsToolchain
from thirdparty.microsoft import unix_path


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "libxcrypt"
    version = "4.4.36"
    license = "LGPL-2.1-or-later"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def validate(self):
        if self.settings.os == "Windows":
            raise RecipeInvalidConfiguration(f"{self.name} is not supported on Windows")

    def requirements(self):
        self.requires_tool("autoconf")
        self.requires_tool("automake")
        self.requires_tool("libtool")

    def source(self):
        get(
            self,
            url=f"https://github.com/besser82/libxcrypt/archive/v{self.version}.tar.gz",
            sha256="b979838d5f1f238869d467484793b72b8bca64c4eae696fdbba0a9e0b6c28453",
            destination=self.folders.source,
            strip_root=True)

        replace_in_file(
            self,
            self.folders.source / "Makefile.am",
            "\nlibcrypt_la_LDFLAGS = ",
            "\nlibcrypt_la_LDFLAGS = -no-undefined ")

    def generate(self):
        VirtualBuildEnv(self).generate()
        tc = AutotoolsToolchain(self)
        tc.configure_args.append("--disable-werror")
        tc.generate()

    def build(self):
        autotools = Autotools(self)
        autotools.autoreconf()
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING.LIB", src=self.folders.source, dst=self.folders.package / "licenses")
        autotools = Autotools(self)
        autotools.install(args=[f"DESTDIR={unix_path(self, self.folders.package)}"])
        rm(self, "*.la", self.folders.package / "lib")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "share")
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.info.set_property("pkg_config_name", "libxcrypt")
        self.info.libs = ["crypt"]
