import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.errors import RecipeException
from thirdparty.files import copy, get, replace_in_file
from thirdparty.gnu import Autotools, AutotoolsToolchain
from thirdparty.microsoft import is_msvc


class Recipe(RecipeBase):
    name = "bison"
    version = "3.8.2"
    license = "GPL-3.0-or-later"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def validate(self):
        # bison 3.8.2's bundled gnulib does not compile with MSVC.  Use winflexbison on Windows.
        if is_msvc(self):
            raise RecipeException(
                "bison 3.8.2 cannot be built with MSVC; use winflexbison on Windows instead.")

    def requirements(self):
        # bison invokes m4 at runtime to expand its parser skeletons
        self.requires("m4")

    def build_requirements(self):
        if self.settings_build.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")
        self.tool_requires("m4")
        if self.settings.os != "Windows":
            self.tool_requires("flex")

    def source(self):
        get(
            self,
            url="https://ftpmirror.gnu.org/gnu/bison/bison-3.8.2.tar.gz",
            sha256="06c9e13bdf7eb24d4ceb6b59205a4f67c2c7e7213119644430fe82fbd14a0abb",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        tc.configure_args.extend([
            "--enable-relocatable",
            "--disable-nls",
            "--datarootdir=${prefix}/res",
        ])
        tc.generate()

    def _patch_sources(self):
        # Make the installed ``yacc`` wrapper relocatable.
        yacc = os.path.join(self.folders.source, "src", "yacc.in")
        replace_in_file(self, yacc, "@prefix@", "$CONAN_BISON_ROOT")
        replace_in_file(self, yacc, "@bindir@", "$CONAN_BISON_ROOT/bin")

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING", src=self.folders.source,
             dst=os.path.join(self.folders.package, "licenses"))
        autotools = Autotools(self)
        autotools.install()

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libs = ["y"]
        self.cpp_info.resdirs = ["res"]

        bison_root = self.folders.package.as_posix()
        self.buildenv_info.define_path("CONAN_BISON_ROOT", bison_root)
        self.buildenv_info.define_path("BISON_PKGDATADIR",
                                       os.path.join(self.folders.package, "res", "bison"))
