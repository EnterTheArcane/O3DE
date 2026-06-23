import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import apply_patches, chdir, copy, get, rmdir
from thirdparty.autotools import Autotools, AutotoolsToolchain
from thirdparty.microsoft import is_msvc, unix_path


class Recipe(RecipeBase):
    name = "gperf"
    version = "3.1"
    license = "GPL-3.0-or-later"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        if self.settings_build.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

    def source(self):
        get(
            self,
            url="https://ftpmirror.gnu.org/gnu/gperf/gperf-3.1.tar.gz",
            sha256="588546b945bba4b70b6a3a616e80b4ab466e3f33024a352fc2198112cdbb3ae2",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        tcenv = tc.environment()
        if is_msvc(self):
            compile_wrapper = unix_path(self, os.path.join(self.folders.source, "build-aux", "compile"))
            ar_wrapper = unix_path(self, os.path.join(self.folders.source, "build-aux", "ar-lib"))
            tcenv.define("CC", f"{compile_wrapper} cl -nologo")
            tcenv.define("CXX", f"{compile_wrapper} cl -nologo")
            tcenv.append("CPPFLAGS", "-D_WIN32_WINNT=_WIN32_WINNT_WIN8")
            tcenv.define("LD", "link -nologo")
            tcenv.define("AR", f'{ar_wrapper} "lib -nologo"')
            tcenv.define("NM", "dumpbin -symbols")
            tcenv.define("OBJDUMP", ":")
            tcenv.define("RANLIB", ":")
            tcenv.define("STRIP", ":")
            # Prevent msys2 from converting the -Tp C++ flag handled by the compile wrapper.
            tcenv.define("MSYS2_ARG_CONV_EXCL", "-Tp")
        tc.generate(tcenv)

    def build(self):
        apply_patches(self)
        autotools = Autotools(self)
        with chdir(self, self.folders.source):
            autotools.configure()
            autotools.make()

    def package(self):
        copy(self, "COPYING", src=self.folders.source,
             dst=os.path.join(self.folders.package, "licenses"))
        autotools = Autotools(self)
        with chdir(self, self.folders.source):
            autotools.install()
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
