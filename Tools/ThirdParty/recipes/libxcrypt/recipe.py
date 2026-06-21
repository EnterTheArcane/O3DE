import os

from thirdparty import RecipeBase
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.env import VirtualBuildEnv
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.gnu import Autotools, AutotoolsToolchain
from thirdparty.microsoft import unix_path


class Recipe(RecipeBase):
    name = "libxcrypt"
    version = "4.4.36"
    license = "LGPL-2.1-or-later"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def validate(self):
        if self.settings.os == "Windows":
            raise RecipeInvalidConfiguration(f"{self.name} is not supported on Windows")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def build_requirements(self):
        self.tool_requires("autoconf")
        self.tool_requires("automake")
        self.tool_requires("libtool")

    def source(self):
        get(
            self,
            url="https://github.com/besser82/libxcrypt/archive/v4.4.36.tar.gz",
            sha256="b979838d5f1f238869d467484793b72b8bca64c4eae696fdbba0a9e0b6c28453",
            destination=self.source_folder,
            strip_root=True)

        replace_in_file(
            self,
            os.path.join(self.source_folder, "Makefile.am"),
            "\nlibcrypt_la_LDFLAGS = ",
            "\nlibcrypt_la_LDFLAGS = -no-undefined ")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        tc = AutotoolsToolchain(self)
        tc.configure_args.append("--disable-werror")
        tc.generate()

    def build(self):
        autotools = Autotools(self)
        autotools.autoreconf()
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING.LIB", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install(args=[f"DESTDIR={unix_path(self, self.package_folder)}"])
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.set_property("pkg_config_name", "libxcrypt")
        self.cpp_info.libs = ["crypt"]
