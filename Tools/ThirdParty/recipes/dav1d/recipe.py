from typing import Literal

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    bit_depth: Literal["all", 8, 16] = "all"
    with_tools: bool = True
    assembly: bool = True


class Recipe(RecipeBase[_Options]):
    name = "dav1d"
    version = "1.5.3"
    license = "BSD-2-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "videolan/dav1d")
        return Version(repo.latest_release)

    def configure(self):
        if is_msvc(self) and self.settings.build_type == "Debug":
            # debug builds with assembly often causes linker hangs or LNK1000
            self.options.assembly = False

        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires_tool("meson")
        if self.options.assembly and self.settings.arch in ("X64",):
            self.requires_tool("nasm")
        if is_msvc(self) and self.settings.arch == "ARM":
            self.requires_tool("gas-preprocessor")
            self.requires_tool("strawberryperl")

    def source(self):
        get(
            self,
            url="https://downloads.videolan.org/videolan/dav1d/1.5.3/dav1d-1.5.3.tar.xz",
            sha256="732010aa5ef461fa93355ed2c6c5fedb48ddc4b74e697eaabe8907eaeb943011",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(self, self.folders.source / "meson.build", "subdir('doc')", "")

    def generate(self):
        VirtualBuildEnv(self).generate()

        tc = MesonToolchain(self)
        tc.project_options["enable_tests"] = False
        tc.project_options["enable_asm"] = self.options.assembly
        tc.project_options["enable_tools"] = self.options.with_tools
        if self.options.bit_depth == "all":
            tc.project_options["bitdepths"] = "8,16"
        else:
            tc.project_options["bitdepths"] = str(self.options.bit_depth)
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", src=self.folders.source, dst=self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rm(self, "*.pdb", self.folders.package / "bin")
        rm(self, "*.pdb", self.folders.package / "lib")
        fix_apple_shared_install_name(self)
        fix_msvc_libname(self)

    def package_info(self):
        self.info.set_property("pkg_config_name", "dav1d")
        self.info.libs = ["dav1d"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.extend(["dl", "pthread"])


def fix_msvc_libname(recipe: RecipeBase, remove_lib_prefix: bool = True):
    """remove lib prefix & change extension to .lib in case of cl like compiler"""
    from thirdparty.files import rename
    if not recipe.settings.get_safe("compiler.runtime"):
        return
    libdirs = recipe.info.libdirs
    for libdir in libdirs:
        for ext in [".dll.a", ".dll.lib", ".a"]:
            full_folder = recipe.folders.package / libdir
            for filepath in full_folder.glob(f"*{ext}"):
                libname = filepath.name[0:-len(ext)]
                if remove_lib_prefix and libname[0:3] == "lib":
                    libname = libname[3:]
                rename(recipe, filepath, filepath.parent / f"{libname}.lib")
