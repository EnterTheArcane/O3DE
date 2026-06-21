import os

from thirdparty import RecipeBase
from thirdparty.apple import is_apple_os, fix_apple_shared_install_name
from thirdparty.build import stdcpp_library
from thirdparty.env import Environment, VirtualBuildEnv
from thirdparty.files import copy, get, rm, rmdir, replace_in_file
from thirdparty.gnu import PkgConfigDeps
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "harfbuzz"
    version = "14.2.0"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_glib": [True, False],
        "with_gdi": [True, False],
        "with_uniscribe": [True, False],
        "with_directwrite": [True, False],
        "with_subset": [True, False],
        "with_coretext": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_glib": False,
        "with_gdi": True,
        "with_uniscribe": True,
        "with_directwrite": False,
        "with_subset": True,
        "with_coretext": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        else:
            del self.options.with_gdi
            del self.options.with_uniscribe
            del self.options.with_directwrite
        if not is_apple_os(self):
            del self.options.with_coretext

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if self.options.shared and self.options.with_glib:
            self.options["glib"].shared = True

    def requirements(self):
        self.requires("freetype")
        self.requires("icu")
        if self.options.with_glib:
            self.requires("glib")

    def build_requirements(self):
        self.tool_requires("meson")
        if not self.conf.get("tools.gnu:pkg_config", check_type=str):
            self.tool_requires("pkgconf")
        if self.options.with_glib:
            self.tool_requires("glib")
        if self.settings.os == "Macos":
            # Ensure that the gettext we use at build time is compatible
            # with the libiconv that is transitively exposed by glib
            self.tool_requires("gettext")

    def latest_version(self):
        repo = GithubRepository(self, "harfbuzz/harfbuzz")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/harfbuzz/harfbuzz/releases/download/14.2.0/harfbuzz-14.2.0.tar.xz",
            sha256="94017020f96d025bb66ae91574e4cf334bcad23e8175a8a40565b3721bc2eaff",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        def is_enabled(value):
            return "enabled" if value else "disabled"

        def meson_backend_and_flags():
            def is_vs_2017():
                version = Version(self.settings.compiler.version)
                return version == "15" or version == "191"

            if is_msvc(self) and is_vs_2017() and self.settings.build_type == "Debug":
                # Mitigate https://learn.microsoft.com/en-us/cpp/build/reference/zf?view=msvc-170
                return "vs", ["/bigobj"]
            return "ninja", []

        VirtualBuildEnv(self).generate()
        PkgConfigDeps(self).generate()

        # Avoid conflicts with libiconv
        # see: https://github.com/recipe-io/recipe-center-index/pull/17046#issuecomment-1554629094
        if self.settings_build.os == "Macos":
            env = Environment()
            env.define_path("DYLD_FALLBACK_LIBRARY_PATH", "$DYLD_LIBRARY_PATH")
            env.define_path("DYLD_LIBRARY_PATH", "")
            env.vars(self, scope="build").save_script("buildenv_macos_runtimepath")

        backend, cxxflags = meson_backend_and_flags()
        tc = MesonToolchain(self, backend=backend)
        tc.project_options["auto_features"] = "disabled"
        tc.project_options.update({
            "glib": is_enabled(self.options.with_glib),
            "icu": is_enabled(True),
            "freetype": is_enabled(True),
            "gdi": is_enabled(self.options.get_safe("with_gdi")),
            "coretext": is_enabled(self.options.get_safe("with_coretext")),
            "directwrite": is_enabled(self.options.get_safe("with_directwrite")),
            "gobject": is_enabled(self.options.with_glib),
            "introspection": is_enabled(False),
            "tests": "disabled",
            "docs": "disabled",
            "benchmark": "disabled",
            "icu_builtin": "false"
        })
        tc.cpp_args += cxxflags
        tc.generate()

    def build(self):
        replace_in_file(self, os.path.join(self.source_folder, "meson.build"), "subdir('util')", "", strict=False)
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        fix_apple_shared_install_name(self)
        fix_msvc_libname(self)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "harfbuzz")
        self.cpp_info.set_property("pkg_config_name", "harfbuzz")

        # TODO in next Harfbuzz major version:
        # - rename "core" component to "harfbuzz"
        # - add self.cpp_info.set_property("pkg_config_name", "none")
        # - set "harfbuzz" as the pkg_config_name of the harfbuzz component
        self.cpp_info.components["core"].set_property("cmake_target_name", "harfbuzz::harfbuzz")
        self.cpp_info.components["core"].libs = ["harfbuzz"]
        self.cpp_info.components["core"].includedirs.append(os.path.join("include", "harfbuzz"))
        self.cpp_info.components["core"].requires.append("freetype::freetype")
        if self.options.with_glib:
            self.cpp_info.components["core"].requires.append("glib::glib")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["core"].system_libs.append("m")
        if self.settings.os == "Windows" and not self.options.shared:
            self.cpp_info.components["core"].system_libs.append("user32")
            if self.options.with_gdi or self.options.with_uniscribe:
                self.cpp_info.components["core"].system_libs.append("gdi32")
            if self.options.with_uniscribe or self.options.with_directwrite:
                self.cpp_info.components["core"].system_libs.append("rpcrt4")
            if self.options.with_uniscribe:
                self.cpp_info.components["core"].system_libs.append("usp10")
            if self.options.with_directwrite:
                self.cpp_info.components["core"].system_libs.append("dwrite")
        if is_apple_os(self) and self.options.get_safe("with_coretext", False):
            if self.settings.os == "Macos":
                self.cpp_info.components["core"].frameworks.append("ApplicationServices")
            else:
                self.cpp_info.frameworks.extend(["CoreFoundation", "CoreGraphics", "CoreText"])
        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.components["core"].system_libs.append(libcxx)

        self.cpp_info.components["icu"].libs = ["harfbuzz-icu"]
        self.cpp_info.components["icu"].set_property("cmake_target_name", "harfbuzz::icu")
        self.cpp_info.components["icu"].set_property("pkg_config_name", "harfbuzz-icu")
        self.cpp_info.components["icu"].requires = ["core", "icu::icu"]

        if self.options.with_subset:
            self.cpp_info.components["subset"].libs = ["harfbuzz-subset"]
            self.cpp_info.components["subset"].set_property("cmake_target_name", "harfbuzz::subset")
            self.cpp_info.components["subset"].set_property("pkg_config_name", "harfbuzz-subset")
            self.cpp_info.components["subset"].requires = ["core"]

        if self.options.with_glib:
            self.cpp_info.components["gobject"].libs = ["harfbuzz-gobject"]
            self.cpp_info.components["gobject"].set_property("cmake_target_name", "harfbuzz::gobject")
            self.cpp_info.components["gobject"].set_property("pkg_config_name", "harfbuzz-gobject")
            self.cpp_info.components["gobject"].requires = ["core", "glib::glib"]

def fix_msvc_libname(recipe, remove_lib_prefix=True):
    """remove lib prefix & change extension to .lib in case of cl like compiler"""
    from thirdparty.files import rename
    import glob
    if not recipe.settings.get_safe("compiler.runtime"):
        return
    libdirs = getattr(recipe.cpp.package, "libdirs")
    for libdir in libdirs:
        for ext in [".dll.a", ".dll.lib", ".a"]:
            full_folder = os.path.join(recipe.package_folder, libdir)
            for filepath in glob.glob(os.path.join(full_folder, f"*{ext}")):
                libname = os.path.basename(filepath)[0:-len(ext)]
                if remove_lib_prefix and libname[0:3] == "lib":
                    libname = libname[3:]
                dst = os.path.join(os.path.dirname(filepath), f"{libname}.lib")
                if os.path.exists(dst):
                    os.remove(dst)
                rename(recipe, filepath, dst)
