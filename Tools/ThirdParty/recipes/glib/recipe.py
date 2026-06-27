import os
import shutil

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name, is_apple_os
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import apply_patches, copy, get, replace_in_file, rm, rmdir
from thirdparty.pkgconfig import PkgConfigDeps
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    with_elf: bool = True
    with_selinux: bool = True
    with_mount: bool = True


class Recipe(RecipeBase[_Options]):
    name = "glib"
    version = "2.85.3"
    license = "LGPL-2.1-or-later"

    def latest_version(self):
        repo = GithubRepository(self, "GNOME/glib")
        return Version(repo.latest_release)

    def config_options(self):
        if self.settings.os != "Linux":
            del self.options.with_mount
            del self.options.with_selinux
        self.options.with_elf = self.settings.os == "Linux"
        if is_msvc(self):
            del self.options.with_elf

        if self.settings.os == "Neutrino":
            del self.options.with_elf

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("zlib")
        self.requires("libffi")
        self.requires("pcre2")
        if self.options.get_safe("with_elf"):
            self.requires("elfutils")
        if self.options.get_safe("with_mount"):
            self.requires("libmount")
        if self.options.get_safe("with_selinux"):
            self.requires("libselinux")
        if self.settings.os != "Linux":
            # for Linux, gettext is provided by libc
            self.requires("libgettext")

        if is_apple_os(self):
            self.requires("libiconv")
        self.requires_tool("meson")
        if not self.conf.get("tools.gnu:pkg_config", check_type=str):
            self.requires_tool("pkgconf")

    def source(self):
        get(
            self,
            url="https://download.gnome.org/sources/glib/2.85/glib-2.85.3.tar.xz",
            sha256="af229e1de191d66aebcdb03c7493c724fd4d0a6628b1ca4ea1f35739259b311d",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        virtual_build_env = VirtualBuildEnv(self)
        virtual_build_env.generate()
        deps = PkgConfigDeps(self)
        deps.generate()
        tc = MesonToolchain(self)

        tc.project_options["selinux"] = "enabled" if self.options.get_safe("with_selinux") else "disabled"
        tc.project_options["libmount"] = "enabled" if self.options.get_safe("with_mount") else "disabled"
        if self.settings.os == "FreeBSD" or self.settings.os == "Neutrino":
            tc.project_options["xattr"] = "false"
        tc.project_options["tests"] = "false"
        tc.project_options["libelf"] = "enabled" if self.options.get_safe("with_elf") else "disabled"

        if self.settings.os == "Neutrino":
            tc.cross_build["host"]["system"] = "qnx"
            tc.c_link_args.append("-lm")
            tc.c_link_args.append("-lsocket")

        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        replace_in_file(
            self,
            self.folders.source / "meson.build",
            "subdir('fuzzing')",
            "#subdir('fuzzing')",
            )  # https://gitlab.gnome.org/GNOME/glib/-/issues/2152
        if self.settings.os != "Linux" and self.settings.os != "Neutrino":
            # allow to find gettext
            replace_in_file(
                self,
                self.folders.source / "meson.build",
                "libintl = dependency('intl', required: false",
                "libintl = dependency('libgettext', method : 'pkg-config', required : false",
                )

        replace_in_file(
            self,
            self.folders.source / "gio" / "gdbus-2.0" / "codegen" / "gdbus-codegen.in",
            "'share'",
            "'res'",
            )

    def build(self):
        self._patch_sources()
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, pattern="LGPL-2.1-or-later.txt", dst=self.folders.package / "licenses", src=self.folders.source / "LICENSES")
        meson = Meson(self)
        meson.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "libexec")
        shutil.move(
            self.folders.package / "share",
            self.folders.package / "res",
        )
        rm(self, "*.pdb", self.folders.package / "bin")
        fix_apple_shared_install_name(self)
        fix_msvc_libname(self)

    def package_info(self):
        self.info.components["glib-2.0"].set_property("pkg_config_name", "glib-2.0")
        self.info.components["glib-2.0"].libs = ["glib-2.0"]
        self.info.components["glib-2.0"].includedirs += [
            os.path.join("include", "glib-2.0"),
            os.path.join("lib", "glib-2.0", "include"),
        ]
        self.info.components["glib-2.0"].resdirs = ["res"]

        self.info.components["gmodule-no-export-2.0"].set_property("pkg_config_name", "gmodule-no-export-2.0")
        self.info.components["gmodule-no-export-2.0"].libs = ["gmodule-2.0"]
        self.info.components["gmodule-no-export-2.0"].resdirs = ["res"]
        self.info.components["gmodule-no-export-2.0"].requires.append("glib-2.0")

        self.info.components["gmodule-export-2.0"].set_property("pkg_config_name", "gmodule-export-2.0")
        self.info.components["gmodule-export-2.0"].requires += ["gmodule-no-export-2.0", "glib-2.0"]

        self.info.components["gmodule-2.0"].set_property("pkg_config_name", "gmodule-2.0")
        self.info.components["gmodule-2.0"].requires += ["gmodule-no-export-2.0", "glib-2.0"]

        self.info.components["gobject-2.0"].set_property("pkg_config_name", "gobject-2.0")
        self.info.components["gobject-2.0"].libs = ["gobject-2.0"]
        self.info.components["gobject-2.0"].resdirs = ["res"]
        self.info.components["gobject-2.0"].requires += ["glib-2.0", "libffi::libffi"]

        self.info.components["gthread-2.0"].set_property("pkg_config_name", "gthread-2.0")
        self.info.components["gthread-2.0"].libs = ["gthread-2.0"]
        self.info.components["gthread-2.0"].resdirs = ["res"]
        self.info.components["gthread-2.0"].requires.append("glib-2.0")

        self.info.components["gio-2.0"].set_property("pkg_config_name", "gio-2.0")
        self.info.components["gio-2.0"].libs = ["gio-2.0"]
        self.info.components["gio-2.0"].resdirs = ["res"]
        self.info.components["gio-2.0"].requires += ["glib-2.0", "gobject-2.0", "gmodule-2.0", "zlib::zlib"]

        self.info.components["gresource"].set_property("pkg_config_name", "gresource")
        self.info.components["gresource"].libs = []  # this is actually an executable

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.components["glib-2.0"].system_libs.append("pthread")
            self.info.components["gmodule-no-export-2.0"].system_libs.append("pthread")
            self.info.components["gmodule-no-export-2.0"].system_libs.append("dl")
            self.info.components["gmodule-export-2.0"].sharedlinkflags.append("-Wl,--export-dynamic")
            self.info.components["gmodule-2.0"].sharedlinkflags.append("-Wl,--export-dynamic")
            self.info.components["gthread-2.0"].system_libs.append("pthread")
            self.info.components["gio-2.0"].system_libs.append("dl")

        if self.settings.os == "Neutrino":
            self.info.components["gmodule-export-2.0"].sharedlinkflags.append("-Wl,--export-dynamic")
            self.info.components["gmodule-2.0"].sharedlinkflags.append("-Wl,--export-dynamic")
            self.info.components["glib-2.0"].system_libs.append("m")
            self.info.components["glib-2.0"].system_libs.append("socket")
            self.info.components["gmodule-no-export-2.0"].system_libs.append("c")
            self.info.components["gio-2.0"].system_libs.append("c")
            self.info.components["gio-2.0"].system_libs.append("socket")

        if self.settings.os == "Windows":
            self.info.components["glib-2.0"].system_libs += ["ws2_32", "ole32", "shell32", "user32", "advapi32"]
            self.info.components["gio-2.0"].system_libs.extend(["iphlpapi", "dnsapi", "shlwapi"])
            self.info.components["gio-windows-2.0"].set_property("pkg_config_name", "gio-windows-2.0")
            self.info.components["gio-windows-2.0"].requires = ["gobject-2.0", "gmodule-no-export-2.0", "gio-2.0"]
            self.info.components["gio-windows-2.0"].includedirs = [os.path.join("include", "gio-win32-2.0")]
        else:
            self.info.components["gio-unix-2.0"].set_property("pkg_config_name", "gio-unix-2.0")
            self.info.components["gio-unix-2.0"].requires += ["gobject-2.0", "gio-2.0"]
            self.info.components["gio-unix-2.0"].includedirs = [os.path.join("include", "gio-unix-2.0")]

        if self.settings.os == "Mac":
            self.info.components["glib-2.0"].system_libs.append("resolv")
            self.info.components["glib-2.0"].frameworks += ["Foundation", "CoreServices", "CoreFoundation"]
            self.info.components["gio-2.0"].frameworks.append("AppKit")

            if is_apple_os(self):
                self.info.components["glib-2.0"].requires.append("libiconv::libiconv")

        self.info.components["glib-2.0"].requires.append("pcre2::pcre2")

        if self.settings.os == "Linux":
            self.info.components["gio-2.0"].system_libs.append("resolv")
        else:
            self.info.components["glib-2.0"].requires.append("libgettext::libgettext")

        if self.options.get_safe("with_mount"):
            self.info.components["gio-2.0"].requires.append("libmount::libmount")

        if self.options.get_safe("with_selinux"):
            self.info.components["gio-2.0"].requires.append("libselinux::libselinux")

        if self.options.get_safe("with_elf"):
            self.info.components["gresource"].requires.append("elfutils::libelf")  # this is actually an executable

        pkgconfig_variables = {
            'datadir': '${prefix}/res',
            'schemasdir': '${datadir}/glib-2.0/schemas',
            'bindir': '${prefix}/bin',
            # Can't use libdir here as it is libdir1 when using the PkgConfigDeps generator.
            'giomoduledir': '${prefix}/lib/gio/modules',
            'gio': '${bindir}/gio',
            'gio_querymodules': '${bindir}/gio-querymodules',
            'glib_compile_schemas': '${bindir}/glib-compile-schemas',
            'glib_compile_resources': '${bindir}/glib-compile-resources',
            'gdbus': '${bindir}/gdbus',
            'gdbus_codegen': '${bindir}/gdbus-codegen',
            'gresource': '${bindir}/gresource',
            'gsettings': '${bindir}/gsettings',
        }
        self.info.components["gio-2.0"].set_property(
            "pkg_config_custom_content",
            "\n".join(f"{key}={value}" for key, value in pkgconfig_variables.items()))

        pkgconfig_variables = {
            'bindir': '${prefix}/bin',
            'glib_genmarshal': '${bindir}/glib-genmarshal',
            'gobject_query': '${bindir}/gobject-query',
            'glib_mkenums': '${bindir}/glib-mkenums',
        }
        self.info.components["glib-2.0"].set_property(
            "pkg_config_custom_content",
            "\n".join(f"{key}={value}" for key, value in pkgconfig_variables.items()))


def fix_msvc_libname(recipe, remove_lib_prefix=True):
    """remove lib prefix & change extension to .lib in case of cl like compiler"""
    from thirdparty.files import rename
    import glob
    if not recipe.settings.get_safe("compiler.runtime"):
        return
    libdirs = getattr(recipe.cpp.package, "libdirs")
    for libdir in libdirs:
        for ext in [".dll.a", ".dll.lib", ".a"]:
            full_folder = recipe.folders.package / libdir
            for filepath in glob.glob(full_folder / f"*{ext}"):
                libname = os.path.basename(filepath)[0:-len(ext)]
                if remove_lib_prefix and libname[0:3] == "lib":
                    libname = libname[3:]
                rename(recipe, filepath, os.path.join(os.path.dirname(filepath), f"{libname}.lib"))
