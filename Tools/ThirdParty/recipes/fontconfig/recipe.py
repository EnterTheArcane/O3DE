# Ported from conan-center-index/fontconfig by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.files import copy, get, rm, rmdir
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version

import os


class Recipe(RecipeBase):
    name = "fontconfig"
    version = "2.17.1"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["freetype", "expat"]

    def source(self):
        get(
            url="https://gitlab.freedesktop.org/api/v4/projects/890/packages/generic/fontconfig/2.17.1/fontconfig-2.17.1.tar.xz",
            dest=self.source_folder,
            sha256="9f5cae93f4fffc1fbc05ae99cdfc708cd60dfd6612ffc0512827025c026fa541",
        )

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        deps = PkgConfigDeps(self)
        deps.generate()

        tc = MesonToolchain(self)
        tc.project_options.update(
            {
                "doc": "disabled",
                "nls": "disabled",
                "tests": "disabled",
                "tools": "disabled",
                "sysconfdir": os.path.join("res", "etc"),
                "datadir": os.path.join("res", "share"),
            }
        )
        # expat is built as a static lib; its headers use __declspec(dllimport)
        # unless XML_STATIC is defined.
        tc.c_args.append("-DXML_STATIC")
        # freetype was built with libpng/zlib/bzip2 as static transitive deps;
        # provide them explicitly on the link line since cmake FindFreetype
        # does not propagate them.
        for dep in self.dep_package_paths:
            lib_dir = dep.replace("\\", "/") + "/lib"
            tc.link_args.append(f"-LIBPATH:{lib_dir}")
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(
            "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses")
        )
        meson = Meson(self)
        meson.install()
        rm("*.pdb", self.package_folder, recursive=True)
        rm("*.conf", os.path.join(self.package_folder, "res", "etc", "fonts", "conf.d"))
        rm("*.def", os.path.join(self.package_folder, "lib"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        fix_apple_shared_install_name(self)
        if Version(self.version) <= "2.15.0":
            # TODO: Keep this for versions <= 2.15.0, remove in future versions
            fix_msvc_libname(self)


def fix_msvc_libname(conanfile, remove_lib_prefix=True):
    """remove lib prefix & change extension to .lib in case of cl like compiler"""
    if not conanfile.settings.get_safe("compiler.runtime"):
        return
    from thirdparty.tools.files import rename
    import glob

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
