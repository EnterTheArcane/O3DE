import os
from pathlib import Path
from typing import Any

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name, is_apple_os
from thirdparty.build import cross_building
from thirdparty.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.files import apply_patches, chdir, collect_libs, copy, get, replace_in_file, rmdir
from thirdparty.autotools import Autotools, AutotoolsDeps, AutotoolsToolchain
from thirdparty.nmake import NMakeDeps, NMakeToolchain
from thirdparty.microsoft import is_msvc, is_msvc_static_runtime, msvc_runtime_flag
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "tcl"
    version = "8.6.13"
    license = "TCL"

    def latest_version(self):
        repo = GithubRepository(self, "tcltk/tcl")
        tag = repo.latest_tag("core-")
        return Version(tag.removeprefix("core-").replace("-", "."))

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        self.requires("zlib")
        if self.settings.os == "Windows" and not is_msvc(self):
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.requires_tool("msys2")

    def source(self):
        get(
            self,
            url="https://downloads.sourceforge.net/project/tcl/Tcl/8.6.13/tcl8.6.13-src.tar.gz",
            sha256="43a1fae7412f61ff11de2cfd05d28cfc3a73762f354a417c62370a54e2caf066",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        if is_msvc(self):
            tc = NMakeToolchain(self)
            tc.generate()

            deps = NMakeDeps(self)
            deps.generate()
        else:
            VirtualBuildEnv(self).generate()
            if not cross_building(self):
                VirtualRunEnv(self).generate(scope="build")

            tc = AutotoolsToolchain(self)

            def yes_no(v: Any) -> str:
                return "yes" if v else "no"

            tc.configure_args.extend(
                [
                    "--enable-threads",
                    f"--enable-symbols={yes_no(self.settings.build_type == "Debug")}",
                    f"--enable-64bit={yes_no(self.settings.arch == "X64")}",
                ])
            if self.settings.os == "Linux":
                # Ensure the library has a soname, fix https://github.com/recipe-io/recipe-center-index/issues/27691
                # (mirror debian behavior)
                tc.configure_args.append("TCL_SHLIB_LD_EXTRAS=-Wl,-soname,${TCL_LIB_FILE}")
            tc.generate()

            deps = AutotoolsDeps(self)
            deps.generate()

    def build(self):
        self._patch_sources()
        if is_msvc(self):
            self._build_nmake(["release"])
        else:
            autotools = Autotools(self)
            autotools.configure(build_script_folder=self._get_configure_subdir())
            # https://core.tcl.tk/tcl/tktview/840660e5a1
            for root, _, list_of_files in os.walk(self.folders.build):
                if "Makefile" in list_of_files:
                    replace_in_file(self, os.path.join(root, "Makefile"), "-Dstrtod=fixstrtod", "", strict=False)
            # For some reason this target "binaries" may not be built before others
            # on Windows while it's a dependency of many other targets
            autotools.make(target="binaries")
            autotools.make()

    def package(self):
        copy(self, "license.terms", src=self.folders.source, dst=self.folders.package / "licenses")
        if is_msvc(self):
            self._build_nmake(["install-binaries", "install-libraries"])
        else:
            autotools = Autotools(self)
            autotools.install()
            autotools.install(target="install-private-headers")

            rmdir(self, self.folders.package / "lib" / "pkgconfig")
            rmdir(self, self.folders.package / "man")
            rmdir(self, self.folders.package / "share")
            fix_apple_shared_install_name(self)

        # Relocatable tclConfig.sh
        tclConfigShPath = self.folders.package / "lib" / "tclConfig.sh"
        ## Comment out references to build folder
        replace_in_file(self, tclConfigShPath, "\nTCL_BUILD_", "\n#TCL_BUILD_")
        replace_in_file(self, tclConfigShPath, "\nTCL_SRC_DIR", "\n#TCL_SRC_DIR")
        ## Replace references to package folder by TCL_ROOT env var supposed to be defined by VirtualRunEnv
        if is_msvc(self):
            replace_in_file(self, tclConfigShPath, os.fspath(self.folders.package), "${TCL_ROOT}")
        else:
            replace_in_file(self, tclConfigShPath, "TCL_PREFIX='/'", "TCL_PREFIX='${TCL_ROOT}'")
            replace_in_file(self, tclConfigShPath, "TCL_EXEC_PREFIX='/'", "TCL_EXEC_PREFIX='${TCL_ROOT}'")
            for to_replace in ["//", "/"]:
                replace_in_file(self, tclConfigShPath, f"-L{to_replace}lib", "-L${TCL_ROOT}/lib", strict=False)
                replace_in_file(self, tclConfigShPath, f"{{{to_replace}lib}}", "{${TCL_ROOT}/lib}", strict=False)
                replace_in_file(self, tclConfigShPath, f"='{to_replace}lib", "='${TCL_ROOT}/lib", strict=False)
                replace_in_file(self, tclConfigShPath, f"-I{to_replace}include", "-I${TCL_ROOT}/include", strict=False)

    def package_info(self):
        self.info.set_property("cmake_file_name", "TCL")

        # There are other libs in subfolders, but they are only used
        # for TCL extensions and should not be linked against.
        self.info.libs = collect_libs(self, (self.folders.package / "lib").as_posix())

        if self.settings.os == "Windows":
            self.info.system_libs.extend(["ws2_32", "netapi32", "userenv"])
        elif self.settings.os in ("FreeBSD", "Linux"):
            self.info.system_libs.extend(["dl", "m", "pthread"])
        elif is_apple_os(self):
            self.info.frameworks.append("CoreFoundation")

        if is_msvc(self) and not self.options.shared:
            self.info.defines.append("STATIC_BUILD")

        tcl_version = Version(self.version)
        tcl_library = self.folders.package / "lib" / f"tcl{tcl_version.major}.{tcl_version.minor}"
        self.runenv_info.define_path("TCL_LIBRARY", tcl_library)

        tcl_root = self.folders.package
        self.runenv_info.define_path("TCL_ROOT", tcl_root)

        tclsh_list = list(filter(lambda fn: fn.startswith("tclsh"), os.listdir(self.folders.package / "bin")))
        tclsh = self.folders.package / "bin" / tclsh_list[0]
        self.runenv_info.define_path("TCLSH", tclsh)

    def _patch_sources(self):
        apply_patches(self)

        if is_apple_os(self) and self.settings.arch not in ("X64",):
            macos_configure = self.folders.source / "macosx" / "configure"
            replace_in_file(self, macos_configure, "#define HAVE_CPUID 1", "#undef HAVE_CPUID")

        unix_config_dir = self.folders.source / "unix"
        # When disabling 64-bit support (in 32-bit), this test must be 0 in order to use "long long" for 64-bit ints
        # (${tcl_type_64bit} can be either "__int64" or "long long")
        replace_in_file(
            self, unix_config_dir / "configure",
            "(sizeof(${tcl_type_64bit})==sizeof(long))",
            "(sizeof(${tcl_type_64bit})!=sizeof(long))")

        unix_makefile_in = unix_config_dir / "Makefile.in"
        # Avoid building internal libraries as shared libraries
        replace_in_file(self, unix_makefile_in, "--enable-shared --enable-threads", "--enable-threads")
        # Avoid clearing CFLAGS and LDFLAGS in the makefile
        replace_in_file(self, unix_makefile_in, "\nCFLAGS\t", "\n#CFLAGS\t")
        replace_in_file(self, unix_makefile_in, "\nLDFLAGS\t", "\n#LDFLAGS\t")
        # Use CFLAGS and CPPFLAGS as argument to CC
        replace_in_file(self, unix_makefile_in, "${CFLAGS}", "${CFLAGS} ${CPPFLAGS}")

        win_config_dir = self.folders.source / "win"

        # Fix install for MinGW
        win_makefile_in = win_config_dir / "Makefile.in"
        replace_in_file(self, win_makefile_in, "INSTALL_ROOT	=", "INSTALL_ROOT	= $(DESTDIR)")
        # No link to static libgcc for MinGW
        win_tcl_m4 = win_config_dir / "tcl.m4"
        replace_in_file(self, win_tcl_m4, "-static-libgcc", "")

        # nmake creates a temporary file with mixed forward/backward slashes
        # force the filename to avoid cryptic error messages
        win_makefile_vc = win_config_dir / "makefile.vc"
        replace_in_file(self, win_makefile_vc, "@type << >$@", "type <<temp.tmp >$@")

        win_rules_vc = self.folders.source / "win" / "rules.vc"
        # do not treat nmake build warnings as errors
        replace_in_file(self, win_rules_vc, "cwarn = $(cwarn) -WX", "")
        # disable whole program optimization to be portable across different MSVC versions.
        # See recipe-io/recipe-center-index#4811 recipe-io/recipe-center-index#4094
        replace_in_file(
            self,
            win_rules_vc,
            "OPTIMIZATIONS  = $(OPTIMIZATIONS) -GL",
            "")

    def _build_nmake(self, targets: list[str]):
        opts: list[str] = []
        # https://core.tcl.tk/tips/doc/trunk/tip/477.md
        if not self.options.shared:
            opts.append("static")
        if self.settings.build_type == "Debug":
            opts.append("symbols")
        if is_msvc_static_runtime(self):
            opts.append("nomsvcrt")
        else:
            opts.append("msvcrt")
        if "d" not in msvc_runtime_flag(self):
            opts.append("unchecked")

        win_config_dir = self.folders.source / "win"
        with chdir(self, win_config_dir):
            self.run(
                'nmake -nologo -f "{cfgdir}/makefile.vc" INSTALLDIR="{pkgdir}" OPTS={opts} {targets}'.format(
                    cfgdir=win_config_dir,
                    pkgdir=self.folders.package,
                    opts=",".join(opts),
                    targets=" ".join(targets),
                ))

    def _get_configure_subdir(self) -> Path:
        return Path({
            "Mac": "macosx",
            "Linux": "unix",
            "FreeBSD": "unix",
            "Windows": "win",
        }[str(self.settings.os)])
