import os

from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import copy, get, rm, rmdir
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.gitlab import GitlabRepository


class Recipe(RecipeBase):
    name = "fontconfig"
    version = "2.18.0"
    license = "MIT"

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

    def requirements(self):
        self.requires("freetype")
        self.requires("expat")

    def build_requirements(self):
        self.tool_requires("gperf")
        self.tool_requires("meson")
        if not self.conf.get("tools.gnu:pkg_config", default=False, check_type=str):
            self.tool_requires("pkgconf")

    def latest_version(self):
        repo = GitlabRepository(self, "fontconfig/fontconfig", host="gitlab.freedesktop.org")
        return Version(repo.latest_formal_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://gitlab.freedesktop.org/api/v4/projects/890/packages/generic/fontconfig/2.18.0/fontconfig-2.18.0.tar.xz",
            sha256="e7064a4725431ddba06ff8b971ec5a4b422e23b0169ce215747beedcb30e9073",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        deps = PkgConfigDeps(self)
        deps.generate()

        tc = MesonToolchain(self)
        tc.project_options.update({
            "doc": "disabled",
            "nls": "disabled",
            "tests": "disabled",
            "tools": "disabled",
            "sysconfdir": os.path.join("res", "etc"),
            "datadir": os.path.join("res", "share"),
        })
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rm(self, "*.pdb", self.package_folder, recursive=True)
        rm(self, "*.conf", os.path.join(self.package_folder, "res", "etc", "fonts", "conf.d"))
        rm(self, "*.def", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Fontconfig")
        self.cpp_info.set_property("cmake_target_name", "Fontconfig::Fontconfig")
        self.cpp_info.set_property("pkg_config_name", "fontconfig")
        self.cpp_info.libs = ["fontconfig"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.extend(["m", "pthread"])

        fontconfig_path = os.path.join(self.package_folder, "res", "etc", "fonts")
        self.runenv_info.append_path("FONTCONFIG_PATH", fontconfig_path)

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
                libname = os.path.basename(filepath)[0:-len(ext)]
                if remove_lib_prefix and libname[0:3] == "lib":
                    libname = libname[3:]
                rename(conanfile, filepath, os.path.join(os.path.dirname(filepath), f"{libname}.lib"))
