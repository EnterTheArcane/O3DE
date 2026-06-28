import os
import shutil

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import apply_patches, chdir, copy, get, rename, replace_in_file, rm, rmdir
from thirdparty.autotools import Autotools, AutotoolsToolchain
from thirdparty.nmake import NMakeToolchain
from thirdparty.microsoft import is_msvc


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "libmp3lame"
    version = "3.100"
    license = "LGPL-2.0"

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if not is_msvc(self) and not self._is_clang_cl:
            self.requires_tool("gnu-config")
            if self.settings.os == "Windows":
                self.win_bash = True
                if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                    self.requires_tool("msys2")

    def source(self):
        get(
            self,
            url="https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz",
            sha256="ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)
        replace_in_file(self, self.folders.source / "include" / "libmp3lame.sym", "lame_init_old\n", "", strict=False)

    def generate(self):
        if is_msvc(self) or self._is_clang_cl:
            NMakeToolchain(self).generate()
        else:
            VirtualBuildEnv(self).generate()
            tc = AutotoolsToolchain(self)
            tc.configure_args.append("--disable-frontend")
            if self.settings.compiler == "clang" and self.settings.arch in ["X64"]:
                tc.extra_cxxflags.extend(["-mmmx", "-msse"])
            tc.generate()

    def build(self):
        if is_msvc(self) or self._is_clang_cl:
            self._build_vs()
        else:
            self._build_autotools()

    def package(self):
        copy(self, pattern="LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        if is_msvc(self) or self._is_clang_cl:
            copy(self, pattern="*.h", src=self.folders.source / "include", dst=self.folders.package / "include" / "lame")
            name = "libmp3lame.lib" if self.options.shared else "libmp3lame-static.lib"
            copy(self, name, src=self.folders.source / "output", dst=self.folders.package / "lib")
            if self.options.shared:
                copy(self, pattern="*.dll", src=self.folders.source / "output", dst=self.folders.package / "bin")
            rename(
                self, self.folders.package / "lib" / name,
                self.folders.package / "lib" / "mp3lame.lib")
        else:
            autotools = Autotools(self)
            autotools.install()
            rmdir(self, self.folders.package / "share")
            rm(self, "*.la", self.folders.package / "lib")
            fix_apple_shared_install_name(self)

    def package_info(self):
        self.info.libs = ["mp3lame"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs = ["m"]

    @property
    def _is_clang_cl(self):
        return str(self.settings.compiler) in ["clang"] and str(self.settings.os) in ["Windows"]

    def _build_vs(self):
        with chdir(self, self.folders.source):
            shutil.copy2("configMS.h", "config.h")
            # Honor vc runtime
            replace_in_file(self, "Makefile.MSVC", "CC_OPTS = $(CC_OPTS) /MT", "", strict=False)
            # Do not hardcode LTO
            replace_in_file(self, "Makefile.MSVC", " /GL", "", strict=False)
            replace_in_file(self, "Makefile.MSVC", " /LTCG", "", strict=False)
            replace_in_file(self, "Makefile.MSVC", "ADDL_OBJ = bufferoverflowU.lib", "", strict=False)
            command = "nmake -f Makefile.MSVC comp=msvc"
            if self._is_clang_cl:
                compilers_from_conf = self.conf.get("tools.build:compiler_executables", default={}, check_type=dict)
                buildenv_vars = VirtualBuildEnv(self).vars()
                cl = compilers_from_conf.get("c", buildenv_vars.get("CC", "clang-cl"))
                link = buildenv_vars.get("LD", "lld-link")
                replace_in_file(self, "Makefile.MSVC", "CC = cl", f"CC = {cl}", strict=False)
                replace_in_file(self, "Makefile.MSVC", "LN = link", f"LN = {link}", strict=False)
                # what is /GAy? MSDN doesn't know it
                # clang-cl: error: no such file or directory: '/GAy'
                # https://docs.microsoft.com/en-us/cpp/build/reference/ga-optimize-for-windows-application?view=msvc-170
                replace_in_file(self, "Makefile.MSVC", "/GAy", "/GA", strict=False)
            if self.settings.arch == "X64":
                replace_in_file(self, "Makefile.MSVC", "MACHINE = /machine:I386", "MACHINE =/machine:X64", strict=False)
                command += " MSVCVER=Win64 asm=yes"
            elif self.settings.arch == "ARM":
                replace_in_file(self, "Makefile.MSVC", "MACHINE = /machine:I386", "MACHINE =/machine:ARM64", strict=False)
                command += " MSVCVER=Win64"
            else:
                command += " asm=yes"
            command += " libmp3lame.dll" if self.options.shared else " libmp3lame-static.lib"
            self.run(command)

    def _build_autotools(self):
        for gnu_config in [
            self.conf.get("user.gnu-config:config_guess", check_type=str),
            self.conf.get("user.gnu-config:config_sub", check_type=str),
        ]:
            if gnu_config:
                copy(self, os.path.basename(gnu_config), src=os.path.dirname(gnu_config), dst=self.folders.source)
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()
