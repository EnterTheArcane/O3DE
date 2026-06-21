import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import apply_patches, copy, get, rename, rm, rmdir, replace_in_file
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "pkgconf"
    version = "2.5.1"
    license = "ISC"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_lib": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "enable_lib": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if not self.options.enable_lib:
            self.options.rm_safe("fPIC")
            self.options.rm_safe("shared")
        elif self.options.shared:
            self.options.rm_safe("fPIC")

        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def latest_version(self):
        repo = GithubRepository(self, "pkgconf/pkgconf")
        return Version(repo.latest_release.removeprefix("pkgconf-"))

    def source(self):
        get(
            self,
            url="https://distfiles.ariadne.space/pkgconf/pkgconf-2.5.1.tar.xz",
            sha256="cd05c9589b9f86ecf044c10a2269822bc9eb001eced2582cfffd658b0a50c243",
            destination=self.source_folder,
            strip_root=True)

    def _patch_sources(self):
        apply_patches(self)

        if not self.options.get_safe("shared", False):
            replace_in_file(
                self, os.path.join(self.source_folder, "meson.build"),
                "'-DLIBPKGCONF_EXPORT'",
                "'-DPKGCONFIG_IS_STATIC'", strict=False)
            replace_in_file(
                self, os.path.join(self.source_folder, "meson.build"),
                "project('pkgconf', 'c',",
                "project('pkgconf', 'c',\ndefault_options : ['c_std=gnu99'],", strict=False)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = MesonToolchain(self)
        tc.project_options["tests"] = "disabled"

        if not self.options.enable_lib:
            tc.project_options["default_library"] = "static"
        tc.generate()

    def build(self):
        self._patch_sources()
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

        meson = Meson(self)
        meson.install()

        if is_msvc(self):
            rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))
            if self.options.enable_lib and not self.options.shared:
                rm(self, "pkgconf.lib", os.path.join(self.package_folder, "lib"))
                rename(
                    self, os.path.join(self.package_folder, "lib", "libpkgconf.a"),
                    os.path.join(self.package_folder, "lib", "pkgconf.lib"), )

        if not self.options.enable_lib:
            rmdir(self, os.path.join(self.package_folder, "lib"))
            rmdir(self, os.path.join(self.package_folder, "include"))

        rmdir(self, os.path.join(self.package_folder, "share", "man"))
        copy(
            self, "*", src=os.path.join(self.package_folder, "share", "aclocal"),
            dst=os.path.join(self.package_folder, "bin", "aclocal"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        if self.options.enable_lib:
            self.cpp_info.set_property("pkg_config_name", "libpkgconf")
            self.cpp_info.includedirs.append(os.path.join("include", "pkgconf"))
            self.cpp_info.libs = ["pkgconf"]
            if not self.options.shared:
                self.cpp_info.defines = ["PKGCONFIG_IS_STATIC"]
        else:
            self.cpp_info.includedirs = []
            self.cpp_info.libdirs = []

        bindir = os.path.join(self.package_folder, "bin")

        exesuffix = ".exe" if self.settings.os == "Windows" else ""
        pkg_config = os.path.join(bindir, "pkgconf" + exesuffix).replace("\\", "/")
        self.buildenv_info.define_path("PKG_CONFIG", pkg_config)

        pkgconf_aclocal = os.path.join(self.package_folder, "bin", "aclocal")
        self.buildenv_info.prepend_path("ACLOCAL_PATH", pkgconf_aclocal)
        # TODO: evaluate if `ACLOCAL_PATH` is enough and we can stop using `AUTOMAKE_RECIPE_INCLUDES`
        self.buildenv_info.prepend_path("AUTOMAKE_RECIPE_INCLUDES", pkgconf_aclocal)
