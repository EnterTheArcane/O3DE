from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os, fix_apple_shared_install_name
from thirdparty.tools.env import Environment, VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.files import copy, get, rm, rmdir, replace_in_file
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.microsoft import is_msvc_static_runtime, is_msvc
from thirdparty.tools.scm import Version

import os


class Recipe(RecipeBase):
    name = "harfbuzz"
    version = "12.3.0"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_freetype": [True, False],
        "with_icu": [True, False],
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
        "with_freetype": True,
        "with_icu": False,
        "with_glib": False,
        "with_gdi": True,
        "with_uniscribe": True,
        "with_directwrite": False,
        "with_subset": True,
        "with_coretext": True,
    }

    def requirements(self) -> list[str]:
        return ["freetype"]  # icu is optional; harfbuzz has built-in Unicode tables

    def source(self):
        get(
            url="https://github.com/harfbuzz/harfbuzz/releases/download/12.3.0/harfbuzz-12.3.0.tar.xz",
            dest=self.source_folder,
            sha256="8660ebd3c27d9407fc8433b5d172bafba5f0317cb0bb4339f28e5370c93d42b7",
        )

    def generate(self):
        def is_enabled(value):
            return "enabled" if value else "disabled"

        def meson_backend_and_flags():
            def is_vs_2017():
                raw = self.settings.compiler.version
                if raw is None:
                    return False
                version = Version(raw)
                return version == "15" or version == "191"

            if self.is_windows and is_vs_2017() and self.build_type == "Debug":
                # Mitigate https://learn.microsoft.com/en-us/cpp/build/reference/zf?view=msvc-170
                return "vs", ["/bigobj"]
            return "ninja", []

        # Avoid conflicts with libiconv
        # see: https://github.com/conan-io/conan-center-index/pull/17046#issuecomment-1554629094
        if self.is_macos:
            env = Environment()
            env.define_path("DYLD_FALLBACK_LIBRARY_PATH", "$DYLD_LIBRARY_PATH")
            env.define_path("DYLD_LIBRARY_PATH", "")
            env.vars(self, scope="build").save_script("conanbuild_macos_runtimepath")
        backend, cxxflags = meson_backend_and_flags()
        tc = MesonToolchain(self, backend=backend)
        tc.project_options["auto_features"] = "disabled"
        tc.project_options.update(
            {
                "glib": is_enabled(self.options.with_glib),
                "icu": is_enabled(self.options.with_icu),
                "freetype": is_enabled(self.options.with_freetype),
                "gdi": is_enabled(self.options.get("with_gdi")),
                "coretext": is_enabled(self.options.get("with_coretext")),
                "directwrite": is_enabled(self.options.get("with_directwrite")),
                "gobject": is_enabled(self.options.with_glib),
                "introspection": is_enabled(False),
                "tests": "disabled",
                "docs": "disabled",
                "benchmark": "disabled",
                "icu_builtin": "false",
            }
        )
        tc.cpp_args += cxxflags
        tc.generate()

    def build(self):
        replace_in_file(
            os.path.join(self.source_folder, "meson.build"), "subdir('util')", ""
        )
        # harfbuzz 12.x mistakenly set freetype_min_version = '>= 12.0.6' (dropped the '2.' prefix).
        # FreeType 2.x will never satisfy '>= 12.0.6'; patch to require '>= 2.0.0' instead.
        replace_in_file(
            os.path.join(self.source_folder, "meson.build"),
            "freetype_min_version = '>= 12.0.6'",
            "freetype_min_version = '>= 2.0.0'",
        )
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(
            "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses")
        )
        meson = Meson(self)
        meson.install()
        rm("*.pdb", os.path.join(self.package_folder, "bin"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        fix_apple_shared_install_name(self)
        fix_msvc_libname(self)


def fix_msvc_libname(conanfile, remove_lib_prefix=True):
    """remove lib prefix & change extension to .lib in case of cl like compiler"""
    from thirdparty.tools.files import rename
    import glob

    if not conanfile.settings.get_safe("compiler.runtime"):
        return
    libdirs = getattr(conanfile.cpp.package, "libdirs")
    for libdir in libdirs:
        for ext in [".dll.a", ".dll.lib", ".a"]:
            full_folder = os.path.join(conanfile.package_folder, libdir)
            for filepath in glob.glob(os.path.join(full_folder, f"*{ext}")):
                libname = os.path.basename(filepath)[0 : -len(ext)]
                if remove_lib_prefix and libname[0:3] == "lib":
                    libname = libname[3:]
                rename(
                    conanfile,
                    filepath,
                    os.path.join(os.path.dirname(filepath), f"{libname}.lib"),
                )
