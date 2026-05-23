from thirdparty import RecipeBase
from thirdparty.tools.build import cross_building
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_conandata_patches, copy, get, rmdir, save
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import is_msvc, unix_path
from thirdparty.tools.scm import Version
import os
import shutil

class Recipe(RecipeBase):
    name = "m4"
    version = "1.4.20"
    license = "GPL-3.0-only"

    def build_requirements(self):
        if self.settings_build.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

    def source(self):
        get(self, url="https://ftpmirror.gnu.org/gnu/m4/m4-1.4.20.tar.xz", sha256="e236ea3a1ccf5f6c270b1c4bb60726f371fa49459a8eaaebc90b216b328daf2b", destination=self.source_folder, strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        if is_msvc(self):
            tc.extra_cflags.append("-FS")
            # Avoid a `Assertion Failed Dialog Box` during configure with build_type=Debug
            # Visual Studio does not support the %n format flag:
            # https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions
            # Because the %n format is inherently insecure, it is disabled by default. If %n is encountered in a format string,
            # the invalid parameter handler is invoked, as described in Parameter Validation. To enable %n support, see _set_printf_count_output.
            tc.configure_args.extend([
                "gl_cv_func_printf_directive_n=no",
                "gl_cv_func_snprintf_directive_n=no",
                "gl_cv_func_snprintf_directive_n=no",
            ])
            if self.settings.build_type in ("Debug", "RelWithDebInfo"):
                tc.extra_ldflags.append("-PDB")
        elif self.version == "1.4.19" and self.settings.compiler == "gcc" and Version(self.settings.compiler) >= "15":
            # FIXME: https://savannah.gnu.org/support/?func=detailitem&item_id=111150
            # WORKAROUND: https://lists.buildroot.org/pipermail/buildroot/2025-May/777741.html
            tc.extra_cflags.append("-std=gnu17")

        if cross_building(self) and is_msvc(self):
            triplet_arch_windows = {"x86_64": "x86_64", "x86": "i686", "armv8": "aarch64"}
            
            host_arch = triplet_arch_windows.get(str(self.settings.arch))
            build_arch = triplet_arch_windows.get(str(self.settings_build.arch))

            if host_arch and build_arch:
                host = f"{host_arch}-w64-mingw32"
                build = f"{build_arch}-w64-mingw32"
                tc.configure_args.extend([
                    f"--host={host}",
                    f"--build={build}",
                ])
        if self.settings.os == "Windows":
            tc.configure_args.append("ac_cv_func__set_invalid_parameter_handler=yes")
        env = tc.environment()
        # help2man trick
        env.prepend_path("PATH", self.source_folder)
        # handle msvc
        if is_msvc(self):
            ar_wrapper = unix_path(self, os.path.join(self.source_folder, "build-aux", "ar-lib"))
            env.define("CC", "cl -nologo")
            env.define("CXX", "cl -nologo")
            env.define("AR", f"{ar_wrapper} lib")
            env.define("LD", "link")
            env.define("NM", "dumpbin -symbols")
            env.define("OBJDUMP", ":")
            env.define("RANLIB", ":")
            env.define("STRIP", ":")
        tc.generate(env)

    def _patch_sources(self):
        apply_conandata_patches(self)
        if shutil.which("help2man") == None:
            # dummy file for configure
            help2man = os.path.join(self.source_folder, "help2man")
            save(self, help2man, "#!/usr/bin/env bash\n:")
            if os.name == "posix":
                os.chmod(help2man, os.stat(help2man).st_mode | 0o111)

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []

        # M4 environment variable is used by a lot of scripts as a way to override a hard-coded embedded m4 path
        bin_ext = ".exe" if self.settings.os == "Windows" else ""
        m4_bin = os.path.join(self.package_folder, "bin", f"m4{bin_ext}").replace("\\", "/")
        self.runenv_info.define_path("M4", m4_bin)
        self.buildenv_info.define_path("M4", m4_bin)
