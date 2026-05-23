# CPython recipe — Windows (PCbuild/build.bat) and Unix (autotools ./configure)
#
# External dependencies (bzip2, zlib, xz_utils, sqlite3) are always required.
# On Windows, CPython's PCbuild compiles these libraries FROM SOURCE, so we
# redirect the PCbuild external-dir properties to point at the source trees
# already downloaded by those recipes.  get_externals.bat is replaced with a
# no-op script to prevent any network downloads.
#
# On Linux/Mac, the standard autotools build is used with CPPFLAGS/LDFLAGS
# pointing at the recipe-built packages.
#
# NOTE: ctypes (_ctypes extension) is excluded on Windows because it requires
# libffi prebuilt binaries which are not yet available as a recipe.
import os
import shutil
import subprocess
from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get


# config.h for liblzma built inside CPython's PCbuild on Windows (MSVC).
# Originally shipped as xz-5.2.x/windows/vs2019/config.h; XZ 5.4+ dropped the
# native MSBuild solution but the header is still valid for any 5.x release.
_XZ_WIN_CONFIG_H = """\
/* config.h for compiling liblzma (*not* the whole XZ Utils) with MSVC */

#define TUKLIB_SYMBOL_PREFIX lzma_
#define ASSUME_RAM 128
#define HAVE_CHECK_CRC32 1
#define HAVE_CHECK_CRC64 1
#define HAVE_CHECK_SHA256 1
#define HAVE_DECODERS 1
#define HAVE_DECODER_ARM 1
#define HAVE_DECODER_ARMTHUMB 1
#define HAVE_DECODER_DELTA 1
#define HAVE_DECODER_IA64 1
#define HAVE_DECODER_LZMA1 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_DECODER_POWERPC 1
#define HAVE_DECODER_SPARC 1
#define HAVE_DECODER_X86 1
#define HAVE_ENCODERS 1
#define HAVE_ENCODER_ARM 1
#define HAVE_ENCODER_ARMTHUMB 1
#define HAVE_ENCODER_DELTA 1
#define HAVE_ENCODER_IA64 1
#define HAVE_ENCODER_LZMA1 1
#define HAVE_ENCODER_LZMA2 1
#define HAVE_ENCODER_POWERPC 1
#define HAVE_ENCODER_SPARC 1
#define HAVE_ENCODER_X86 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MF_BT2 1
#define HAVE_MF_BT3 1
#define HAVE_MF_BT4 1
#define HAVE_MF_HC3 1
#define HAVE_MF_HC4 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_VISIBILITY 0
#define HAVE__BOOL 1
#ifdef _M_IX86
#define MYTHREAD_WIN95 1
#else
#define MYTHREAD_VISTA 1
#endif
#define NDEBUG 1
#define PACKAGE_NAME "XZ Utils"
#define PACKAGE_URL "https://tukaani.org/xz/"
#ifdef _WIN64
#define SIZEOF_SIZE_T 8
#else
#define SIZEOF_SIZE_T 4
#endif
#define TUKLIB_FAST_UNALIGNED_ACCESS 1
"""


class Recipe(RecipeBase):
    name = "cpython"
    version = "3.12.7"
    license = "PSF-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "optimizations": [True, False],
        "with_ssl": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "optimizations": False,
        "with_ssl": False,
    }

    def requirements(self) -> None:
        self.requires("bzip2/1.0.8")
        self.requires("zlib/1.3.2")
        self.requires("xz_utils/5.8.3")
        self.requires("sqlite3/3.53.1")
        if self.options.with_ssl:
            self.requires("openssl/[>=1.1 <4]")

    def source(self):
        get(
            self,
            url="https://www.python.org/ftp/python/3.12.7/Python-3.12.7.tgz",
            destination=self.source_folder,
            sha256="73ac8fe780227bf371add8373c3079f42a0dc62deff8d612cd15a618082ab623",
            strip_root=True,
        )

    # ------------------------------------------------------------------ helpers

    def _run(self, cmd, cwd: str, env: dict | None = None) -> None:
        kwargs = dict(cwd=cwd, check=False)
        if env is not None:
            kwargs["env"] = env
        if isinstance(cmd, str):
            result = subprocess.run(cmd, shell=True, **kwargs)
        else:
            result = subprocess.run(cmd, **kwargs)
        if result.returncode != 0:
            raise RuntimeError(f"Command failed (exit {result.returncode}): {cmd}")

    def _dep_source_dir(self, dep_name: str) -> str:
        """Return the source folder for a dependency given its package folder.

        The recipe layout is:  build/<name>/<ver>/package/
                               build/<name>/<ver>/source/
        """
        pkg = self.dependencies[dep_name].package_folder
        return os.path.join(os.path.dirname(pkg), "source")

    # -------------------------------------------------------- Windows / PCbuild

    def _platform_toolset(self) -> str:
        """Detect the MSBuild PlatformToolset from the installed MSVC version.

        CPython's PCbuild projects default to v140 (VS 2015).  We query
        vswhere for the latest cl.exe path which encodes the MSVC version
        number, then derive the toolset string (e.g. 14.51.x → 'v145').
        """
        vswhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
        result = subprocess.run(
            [vswhere, "-latest", "-find", r"VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe"],
            capture_output=True, text=True,
        )
        for line in reversed(result.stdout.strip().splitlines()):
            path = line.strip().replace("\\", "/")
            for part in path.split("/"):
                if part.startswith("14.") and part.count(".") >= 1:
                    nums = part.split(".")
                    try:
                        major = int(nums[0])
                        minor = int(nums[1])
                        return f"v{major}{minor // 10}"
                    except (ValueError, IndexError):
                        continue
        return "v143"  # VS 2022 fallback

    def _ensure_xz_vs2019_config(self, lzma_src: str) -> None:
        """Create windows/vs2019/config.h inside the XZ source tree if absent.

        XZ 5.4+ dropped the native MSBuild solution (and its pre-generated
        config.h) that CPython's liblzma.vcxproj expects at that path.  We
        embed a compatible header so the PCbuild compile succeeds without
        requiring the old XZ 5.2.x externals.
        """
        vs2019_dir = os.path.join(lzma_src, "windows", "vs2019")
        config_h = os.path.join(vs2019_dir, "config.h")
        if not os.path.isfile(config_h):
            os.makedirs(vs2019_dir, exist_ok=True)
            with open(config_h, "w") as fh:
                fh.write(_XZ_WIN_CONFIG_H)

    def _build_windows(self) -> None:
        pcbuild_dir = os.path.join(self.source_folder, "PCbuild")

        # Gather source-tree paths for deps that PCbuild compiles from source.
        bz2_src = self._dep_source_dir("bzip2")
        zlib_src = self._dep_source_dir("zlib")
        lzma_src = self._dep_source_dir("xz_utils")
        sqlite3_src = self._dep_source_dir("sqlite3")

        # XZ 5.4+ lacks windows/vs2019/config.h expected by liblzma.vcxproj.
        self._ensure_xz_vs2019_config(lzma_src)

        # XZ 5.4+ renamed crc_macros.h → crc_common.h and removed crc32/64_table.c
        # (tables are now static arrays inside crc32_fast.c / crc64_fast.c).
        # Create a compat shim so the unchanged vcxproj still resolves the header,
        # and patch out the two .c entries that no longer exist in the source tree.
        crc_macros_h = os.path.join(lzma_src, "src", "liblzma", "check", "crc_macros.h")
        if not os.path.isfile(crc_macros_h):
            with open(crc_macros_h, "w") as fh:
                fh.write("/* compat shim: crc_macros.h was merged into crc_common.h in XZ 5.4+ */\n")
                fh.write('#include "crc_common.h"\n')

        liblzma_vcxproj = os.path.join(pcbuild_dir, "liblzma.vcxproj")
        with open(liblzma_vcxproj, encoding="utf-8") as fh:
            vcx_content = fh.read()
        for removed_src in (
            r'<ClCompile Include="$(lzmaDir)src\liblzma\check\crc32_table.c" />',
            r'<ClCompile Include="$(lzmaDir)src\liblzma\check\crc64_table.c" />',
        ):
            vcx_content = vcx_content.replace(removed_src + "\r\n", "")
            vcx_content = vcx_content.replace(removed_src + "\n", "")
        with open(liblzma_vcxproj, "w", encoding="utf-8") as fh:
            fh.write(vcx_content)

        def _msb(path: str) -> str:
            """Normalise to Windows path with trailing backslash."""
            p = os.path.normpath(path)
            return p if p.endswith(os.sep) else p + os.sep

        # sqlite3.vcxproj extracts version parts from the last path component via
        # a regex that requires a 4-part version (e.g. "3.45.3.0").  Our sqlite3
        # source is versioned as "3.53.1" (3-part), so we create a shim directory
        # with the 4-part name and copy the three amalgamation files into it.
        sqlite3_ver = os.path.basename(os.path.dirname(sqlite3_src))  # "3.53.1"
        ver_parts = sqlite3_ver.split(".")
        while len(ver_parts) < 4:
            ver_parts.append("0")
        sqlite3_shim_name = f"sqlite-{'.'.join(ver_parts)}"
        externals_dir = os.path.join(self.source_folder, "externals")
        sqlite3_shim = os.path.join(externals_dir, sqlite3_shim_name)
        os.makedirs(sqlite3_shim, exist_ok=True)
        for fname in ("sqlite3.c", "sqlite3.h", "sqlite3ext.h"):
            src_file = os.path.join(sqlite3_src, fname)
            dst_file = os.path.join(sqlite3_shim, fname)
            if os.path.isfile(src_file) and not os.path.isfile(dst_file):
                shutil.copy2(src_file, dst_file)

        # Stub out every other directory that get_externals.bat would download.
        # The script checks "if exist <dir>" and skips if present, so empty
        # stub directories are enough to prevent all network traffic.
        #
        # Sources  — actual paths are redirected via ExternalProps below.
        # Binaries — libffi prebuilt DLLs for ctypes; stubbed because we skip
        #            ctypes via --no-ctypes.  openssl-bin and tcltk are skipped
        #            automatically by --no-ssl / --no-tkinter flags.
        for stub in (
            "bzip2-1.0.8",
            "xz-5.2.5",
            "zlib-1.3.1",
            "libffi-3.4.4",  # binary prebuilt (ctypes disabled, but stub prevents download)
        ):
            os.makedirs(os.path.join(externals_dir, stub), exist_ok=True)

        # Write an MSBuild props file that redirects each external-dep directory
        # to our recipe source trees.  python.props imports $(ExternalProps)
        # BEFORE the conditional defaults, so our unconditional values win.
        props_file = os.path.join(pcbuild_dir, "cpython_recipe_externals.props")
        with open(props_file, "w") as fh:
            fh.write(
                '<?xml version="1.0" encoding="utf-8"?>\n'
                '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n'
                "  <PropertyGroup>\n"
                f"    <bz2Dir>{_msb(bz2_src)}</bz2Dir>\n"
                f"    <zlibDir>{_msb(zlib_src)}</zlibDir>\n"
                f"    <lzmaDir>{_msb(lzma_src)}</lzmaDir>\n"
                f"    <sqlite3Dir>{_msb(sqlite3_shim)}</sqlite3Dir>\n"
                "  </PropertyGroup>\n"
                "</Project>\n"
            )

        # msbuild.rsp: auto-loaded by MSBuild from the PCbuild directory.
        # - PlatformToolset: PCbuild defaults to v140 (VS 2015); override for current VS.
        # - ExternalProps:   path to our redirecting props file (read by python.props).
        rsp_file = os.path.join(pcbuild_dir, "msbuild.rsp")
        toolset = self._platform_toolset()
        with open(rsp_file, "w") as fh:
            fh.write(f"/p:PlatformToolset={toolset}\n")
            fh.write(f'/p:ExternalProps="{props_file}"\n')

        build_bat = os.path.join(pcbuild_dir, "build.bat")
        build_type = "Debug" if self.settings.build_type == "Debug" else "Release"
        cmd = [
            build_bat,
            "-c", build_type,
            "-p", "x64",
            "--no-tkinter",
            # ctypes requires libffi prebuilt binaries; no recipe available yet.
            "--no-ctypes",
        ]
        if not self.options.with_ssl:
            cmd.append("--no-ssl")
        elif "openssl" in self.dependencies:
            ssl_dir = self.dependencies["openssl"].package_folder.replace("\\", "/")
            cmd += [f"--ssl={ssl_dir}"]
        if self.options.optimizations:
            cmd.append("--pgo")

        try:
            self._run(cmd, cwd=self.source_folder)
        finally:
            for path in (props_file, rsp_file):
                if os.path.exists(path):
                    os.remove(path)

    # --------------------------------------------------------- Unix (Linux/Mac)

    def _build_unix(self) -> None:
        bz2_pkg = self.dependencies["bzip2"].package_folder
        zlib_pkg = self.dependencies["zlib"].package_folder
        lzma_pkg = self.dependencies["xz_utils"].package_folder
        sqlite3_pkg = self.dependencies["sqlite3"].package_folder

        cppflags = " ".join([
            f"-I{bz2_pkg}/include",
            f"-I{zlib_pkg}/include",
            f"-I{lzma_pkg}/include",
            f"-I{sqlite3_pkg}/include",
        ])
        ldflags = " ".join([
            f"-L{bz2_pkg}/lib",
            f"-L{zlib_pkg}/lib",
            f"-L{lzma_pkg}/lib",
            f"-L{sqlite3_pkg}/lib",
        ])

        configure_args = [
            "./configure",
            f"--prefix={self.package_folder}",
            "--without-ensurepip",
            "--enable-shared" if self.options.shared else "--disable-shared",
        ]
        if not self.options.with_ssl:
            configure_args.append("--without-ssl")
        elif "openssl" in self.dependencies:
            configure_args.append(
                f"--with-openssl={self.dependencies['openssl'].package_folder}"
            )

        env = dict(os.environ)
        env["CPPFLAGS"] = cppflags
        env["LDFLAGS"] = ldflags

        cpu_count = os.cpu_count() or 4
        self._run(configure_args, cwd=self.source_folder, env=env)
        self._run(["make", f"-j{cpu_count}"], cwd=self.source_folder)
        # Install directly to package_folder (configure was given --prefix).
        self._run(["make", "install"], cwd=self.source_folder)

    # ---------------------------------------------------------------- lifecycle

    def build(self) -> None:
        if self.settings.os == "Windows":
            self._build_windows()
        else:
            self._build_unix()

    def package(self) -> None:
        if self.settings.os == "Windows":
            self._package_windows()
        else:
            # make install already populated package_folder via --prefix.
            copy(
                self,
                "LICENSE",
                src=self.source_folder,
                dst=os.path.join(self.package_folder, "licenses"),
            )

    def _package_windows(self) -> None:
        artifacts_dir = os.path.join(self.source_folder, "PCbuild", "amd64")

        bin_dir = os.path.join(self.package_folder, "bin")
        Path(bin_dir).mkdir(parents=True, exist_ok=True)
        copy(self, "python*.exe", src=artifacts_dir, dst=bin_dir)
        copy(self, "python*.dll", src=artifacts_dir, dst=bin_dir)
        copy(self, "*.dll", src=artifacts_dir, dst=bin_dir)

        # Copy all extension modules, skipping test-only .pyds.
        _test_prefixes = ("_test", "_ctypes_test", "xxlimited")
        for pyd in Path(artifacts_dir).glob("*.pyd"):
            if not any(pyd.name.startswith(p) for p in _test_prefixes):
                shutil.copy2(pyd, os.path.join(bin_dir, pyd.name))

        lib_dir = os.path.join(self.package_folder, "lib")
        Path(lib_dir).mkdir(parents=True, exist_ok=True)
        copy(self, "python*.lib", src=artifacts_dir, dst=lib_dir)

        include_src = os.path.join(self.source_folder, "Include")
        include_dst = os.path.join(self.package_folder, "include", "python")
        Path(include_dst).mkdir(parents=True, exist_ok=True)
        copy(self, "*.h", src=include_src, dst=include_dst)
        copy(self, "pyconfig.h", src=os.path.join(self.source_folder, "PC"), dst=include_dst)

        copy(
            self,
            "*",
            src=os.path.join(self.source_folder, "Lib"),
            dst=os.path.join(self.package_folder, "lib", "python"),
        )
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
