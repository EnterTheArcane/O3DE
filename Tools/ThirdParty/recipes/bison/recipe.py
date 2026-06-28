from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get, replace_in_file
from thirdparty.autotools import Autotools, AutotoolsToolchain


class Recipe(RecipeBase):
    name = "bison"
    version = "3.8.2"
    license = "GPL-3.0-or-later"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def validate(self):
        if self.settings.os == "Windows":
            raise RecipeInvalidConfiguration("Windows is not supported")

    def requirements(self):
        # bison invokes m4 at runtime to expand its parser skeletons
        self.requires("m4")
        self.requires_tool("m4")
        self.requires_tool("flex")

    def source(self):
        get(
            self,
            url="https://ftpmirror.gnu.org/gnu/bison/bison-3.8.2.tar.gz",
            sha256="06c9e13bdf7eb24d4ceb6b59205a4f67c2c7e7213119644430fe82fbd14a0abb",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        VirtualBuildEnv(self).generate()

        tc = AutotoolsToolchain(self)
        tc.configure_args.extend([
            "--enable-relocatable",
            "--disable-nls",
            "--datarootdir=${prefix}/res",
        ])
        tc.generate()

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING", src=self.folders.source,
             dst=self.folders.package / "licenses")
        autotools = Autotools(self)
        autotools.install()

    def package_info(self):
        self.info.includedirs = []
        self.info.libs = ["y"]
        self.info.resdirs = ["res"]
        self.buildenv_info.define_path("CONAN_BISON_ROOT", self.folders.package.as_posix())
        self.buildenv_info.define_path(
            "BISON_PKGDATADIR", self.folders.package / "res" / "bison")

    def _patch_sources(self):
        # Make the installed ``yacc`` wrapper relocatable.
        yacc = self.folders.source / "src" / "yacc.in"
        replace_in_file(self, yacc, "@prefix@", "$CONAN_BISON_ROOT")
        replace_in_file(self, yacc, "@bindir@", "$CONAN_BISON_ROOT/bin")
