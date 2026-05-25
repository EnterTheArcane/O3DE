from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import is_msvc, unix_path
import os


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

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def build_requirements(self):
        self.tool_requires("libtool")
        if self.settings.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

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
        if self.settings.os == "Windows":
            replace_in_file(self, os.path.join(self.build_folder, "libtool"), "-DPIC", "")
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
