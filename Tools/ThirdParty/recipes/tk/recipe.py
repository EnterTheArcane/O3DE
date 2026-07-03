import os
from pathlib import Path
from typing import Any

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import is_apple_os, fix_apple_shared_install_name
from thirdparty.build import cross_building
from thirdparty.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.errors import RecipeException
from thirdparty.files import apply_patches, chdir, copy, get, replace_in_file, rmdir
from thirdparty.autotools import Autotools, AutotoolsDeps, AutotoolsToolchain
from thirdparty.nmake import NMakeDeps, NMakeToolchain
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "tk"
    version = "8.6.10"
    license = "TCL"

    def latest_version(self):
        repo = GithubRepository(self, "tcltk/tk")
        tag = repo.latest_tag("core-")
        return Version(tag.removeprefix("core-").replace("-", "."))

    def configure(self):
        self.settings.compiler_libcxx = None
        self.settings.compiler_cxx_standard = None

    def requirements(self):
        self.requires(f"tcl")
        if self.settings.os == "Linux":
            self.requires("fontconfig")
            self.requires("xorg")
        if not is_msvc(self):
            if self.settings.os == "Windows":
                self.requires_tool("msys2")

    def source(self):
        get(
            self,
            # NB: the GitHub archive omits release-only files (macosx/configure
            # and doc/man.macros), so use the release tarball that ships them.
            url=f"https://prdownloads.sourceforge.net/tcl/tk{self.version}-src.tar.gz", sha256="63df418a859d0a463347f95ded5cd88a3dd3aaa1ceecaeee362194bc30f3e386", strip_root=True,
            destination=self.folders.source)
        apply_patches(self)

    def generate(self):
        VirtualBuildEnv(self).generate()

        if is_msvc(self):
            NMakeToolchain(self).generate()
            NMakeDeps(self).generate()
        else:
            # Inject runenv variables into buildenv
            # This is required because tcl needs to be available when configure tries to
            # run a test executable
            if not cross_building(self):
                VirtualRunEnv(self).generate(scope="build")
                
            def yes_no(v: Any) -> str:
                return "yes" if v else "no"

            tc = AutotoolsToolchain(self)
            tc.configure_args.append("--enable-threads")
            tc.configure_args.append(
                f"--enable-symbols={yes_no(self.settings.build_type == "Debug")}"
            )
            tc.configure_args.append(
                f"--enable-64bit={yes_no(self.settings.arch == "X64")}"
            )
            tc.configure_args.append(f"--enable-aqua={yes_no(is_apple_os(self))}")
            tc.configure_args.append(
                f"--with-tcl={self.dependencies["tcl"].folders.package / "lib"}"
            )
            tc.configure_args.append(f"--with-x={yes_no(self.settings.os == "Linux")}")
            tc.make_args.append(
                f"TCL_GENERIC_DIR={self.dependencies["tcl"].folders.package / "include"}"
            )
            if self.settings.os == "Windows":
                tc.extra_defines.extend(
                    [
                        "UNICODE",
                        "_UNICODE",
                        "_ATL_XP_TARGETING",
                    ]
                )
            if not is_apple_os(self):
                tc.extra_ldflags.append("-Wl,--as-needed")
            tc.generate()

            if self.settings.os == "Linux":
                deps = AutotoolsDeps(self)
                deps.generate()

    def build(self):
        if is_msvc(self):
            self._build_nmake()
        else:
            autotools = Autotools(self)
            autotools.configure(build_script_folder=self._get_configure_folder())
            autotools.make()

    def package(self):
        copy(
            self,
            pattern="license.terms",
            src=self.folders.source,
            dst=self.folders.package / "licenses",
        )
        if is_msvc(self):
            self._build_nmake("install")
        else:
            with chdir(self, self.folders.build):
                autotools = Autotools(self)
                autotools.install()
                # DESTDIR is only default initialized for target="install"
                autotools.make(
                    target="install-private-headers",
                    args=[f"DESTDIR={self.folders.package}"],
                )
                rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "man")
        rmdir(self, self.folders.package / "share")

        tkConfigShPath = self.folders.package / "lib" / "tkConfig.sh"
        if os.path.exists(tkConfigShPath):
            # This can only be modified after build since the value being replaced is a result
            # of variable substitution in tkConfig.sh.in
            replace_in_file(self, tkConfigShPath, "//", "${TK_ROOT}/")

        fix_apple_shared_install_name(self)

    def package_info(self):
        tk_version = Version(self.version)
        lib_infix = f"{tk_version.major}.{tk_version.minor}"
        if is_msvc(self):
            lib_infix = f"{tk_version.major}{tk_version.minor}"
            tk_suffix = "t{}{}{}".format(
                "" if self.options.shared else "s",
                "g" if self.settings.build_type == "Debug" else "",
                "x" if ("dynamic" in str(self.settings.compiler_runtime) or "MD" in str(self.settings.compiler_runtime)) and not self.options.shared else "",
            )
        else:
            tk_suffix = ""
        self.info.libs = [f"tk{lib_infix}{tk_suffix}", f"tkstub{lib_infix}"]
        if self.settings.os == "Mac":
            self.info.frameworks = ["CoreFoundation", "Cocoa", "Carbon", "IOKit"]
        elif self.settings.os == "Windows":
            self.info.system_libs = [
                "netapi32",
                "kernel32",
                "user32",
                "advapi32",
                "userenv",
                "ws2_32",
                "gdi32",
                "comdlg32",
                "imm32",
                "comctl32",
                "shell32",
                "uuid",
                "ole32",
                "oleaut32",
            ]
        elif self.settings.os == "Linux":
            self.info.requires = [
                "tcl::tcl",
                "fontconfig::fontconfig",
                "xorg::x11",
                "xorg::xcb",
                "xorg::xrender",
                "xorg::xau",
                "xorg::xdmcp",
            ]

        tk_library = (self.folders.package / "lib" / f"{self.name}{tk_version.major}.{tk_version.minor}").as_posix()
        self.info.runenv.define("TK_LIBRARY", tk_library)

        tk_root = self.folders.package.as_posix()
        self.info.runenv.define("TK_ROOT", tk_root)

    def _get_default_build_system(self):
        if is_apple_os(self):
            return "macosx"
        elif self.settings.os in ("Linux", "FreeBSD"):
            return "unix"
        elif self.settings.os == "Windows":
            return "win"
        else:
            raise ValueError("tk recipe does not recognize os")

    def _get_configure_folder(self, build_system: str | None = None) -> Path:
        if build_system is None:
            build_system = self._get_default_build_system()
        if build_system not in ["win", "unix", "macosx"]:
            raise RecipeException(f"Invalid build system: {build_system}")
        return self.folders.source / build_system

    def _build_nmake(self, target: str = "release"):
        # https://core.tcl.tk/tips/doc/trunk/tip/477.md
        opts: list[str] = []
        if not self.options.shared:
            opts.append("static")
        if self.settings.build_type == "Debug":
            opts.append("symbols")
        if "dynamic" in str(self.settings.compiler_runtime) or "MD" in str(self.settings.compiler_runtime):
            opts.append("msvcrt")
        else:
            opts.append("nomsvcrt")
        if "d" not in str(self.settings.compiler_runtime):
            opts.append("unchecked")
        # https://core.tcl.tk/tk/tktview?name=3d34589aa0
        # https://wiki.tcl-lang.org/page/Building+with+Visual+Studio+2017
        tcl_lib_path: Path = self.dependencies["tcl"].folders.package / "lib"
        tclimplib, tclstublib = None, None
        for lib in os.listdir(tcl_lib_path):
            if not lib.endswith(".lib"):
                continue
            if lib.startswith("tcl{}".format("".join(self.version.split(".")[:2]))):
                tclimplib = tcl_lib_path / lib
            elif lib.startswith(
                    "tclstub{}".format("".join(self.version.split(".")[:2]))
            ):
                tclstublib = tcl_lib_path / lib

        if tclimplib is None or tclstublib is None:
            raise RecipeException("tcl dependency misses tcl and/or tclstub library")

        flags = {
            "INSTALLDIR": self.folders.package,
            "OPTS": ",".join(opts),
            "TCLDIR": self.dependencies["tcl"].folders.package,
            "TCL_LIBRARY": self.dependencies["tcl"].info.runenv.vars(self).get("TCL_LIBRARY"),
            "TCLIMPLIB": tclimplib,
            "TCLSTUBLIB": tclstublib,
        }
        config_dir = self._get_configure_folder("win")
        with chdir(self, config_dir):
            self.run(
                f"""nmake -nologo -f makefile.vc {" ".join([f'{k}="{v}"' for k, v in flags.items()])} {target}""",
                env="env_build",
            )
