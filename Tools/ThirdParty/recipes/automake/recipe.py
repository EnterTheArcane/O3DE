import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import apply_patches, copy, get, replace_in_file, rename, rmdir, save
from thirdparty.gnu import Autotools, AutotoolsToolchain
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "automake"
    version = "1.16.5"
    license = "GPL-2.0-or-later", "GPL-3.0-or-later"

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("autoconf")

    def build_requirements(self):
        self.tool_requires("autoconf")
        if self.settings_build.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

    def latest_version(self):
        repo = GithubRepository(self, "autotools-mirror/automake")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://ftpmirror.gnu.org/gnu/automake/automake-1.16.5.tar.gz",
            sha256="07bd24ad08a64bc17250ce09ec56e921d6343903943e99ccf63bbf0705e34605",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        if self.settings.os == "Windows":
            # tracing using m4 on Windows returns Windows paths => use cygpath to convert to unix paths
            ac_local_in = os.path.join(self.source_folder, "bin", "aclocal.in")
            with open(ac_local_in, encoding="utf-8") as _f:
                _content = _f.read()
            if "cygpath -u $file" not in _content:
                replace_in_file(
                    self,
                    ac_local_in,
                    "          $map_traced_defs{$arg1} = $file;",
                    "          $file = `cygpath -u $file`;\n"
                    "          $file =~ s/^\\s+|\\s+$//g;\n"
                    "          $map_traced_defs{$arg1} = $file;")
            # handle relative paths during aclocal.m4 creation
            replace_in_file(
                self,
                ac_local_in,
                "$map{$m} eq $map_traced_defs{$m}",
                "abs_path($map{$m}) eq abs_path($map_traced_defs{$m})",
                strict=False)

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        autotools = Autotools(self)
        autotools.install()
        copy(self, "COPYING*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

        rmdir(self, os.path.join(self.package_folder, "share", "info"))
        rmdir(self, os.path.join(self.package_folder, "share", "man"))
        rmdir(self, os.path.join(self.package_folder, "share", "doc"))

        if self.settings.os == "Windows":
            # TODO: consider whether the following is still necessary on Windows
            binpath = os.path.join(self.package_folder, "bin")
            for filename in os.listdir(binpath):
                fullpath = os.path.join(binpath, filename)
                if not os.path.isfile(fullpath):
                    continue
                rename(self, fullpath, fullpath + ".exe")

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        self.cpp_info.frameworkdirs = []

        # For consumers with new integrations (Recipe 1 and 2 compatible):
        ver = Version(self.version)
        automake_helper_scripts_dir = os.path.join(self.package_folder, "share", f"automake-{ver.major}.{ver.minor}")
        compile_wrapper = os.path.join(automake_helper_scripts_dir, "compile")
        lib_wrapper = os.path.join(automake_helper_scripts_dir, "ar-lib")
        self.conf_info.define("user.automake:compile-wrapper", compile_wrapper)
        self.conf_info.define("user.automake:lib-wrapper", lib_wrapper)
