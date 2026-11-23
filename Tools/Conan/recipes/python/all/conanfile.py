#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from io import StringIO
from pathlib import Path
import os
from runpy import run_path
import shlex
from shutil import which
from subprocess import list2cmdline
import sys

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import build_jobs, cross_building
from conan.tools.env import Environment, VirtualBuildEnv
from conan.tools.files import (
    apply_conandata_patches,
    copy,
    export_conandata_patches,
    get,
    mkdir,
    rename,
)
from conan.tools.gnu import PkgConfigDeps
from conan.tools.layout import basic_layout
from conan.tools.microsoft import VCVars


class Recipe(ConanFile):
    name = "python"
    license = "PSF-2.0"
    package_type = "shared-library"
    python_requires = "apple-signing/1.0"
    settings = "os", "arch", "compiler", "build_type"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def export_sources(self):
        for pattern in ("platform/*/python-config.cmake", "platform/macos/relocate.py"):
            copy(self, pattern, self.recipe_folder, self.export_sources_folder)
        export_conandata_patches(self)

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")
        self.win_bash = (
            self.settings.os == "Android" and self.settings_build.os == "Windows"
        )

    def package_id(self):
        if self.info.settings.os == "Macos":
            self.info.python_requires.revision_mode()
        # The Windows distribution contains both ABIs.
        if self.info.settings.os == "Windows":
            self.info.settings.rm_safe("build_type")
            self.info.settings.rm_safe("compiler.runtime_type")

    def requirements(self):
        if self.settings.os not in ("Windows", "Emscripten"):
            self.requires("openssl/4.1.0")
        if self.settings.os in ("Linux", "iOS", "Android"):
            self.requires("libffi/3.4.8")
            self.requires("sqlite3/3.54.1")
            self.requires("xz_utils/5.8.4")
            self.requires("zlib/1.4")
        if self.settings.os in ("Linux", "Android", "Emscripten"):
            self.requires("bzip2/1.0.8")
        if self.settings.os != "Windows" and self.settings.os != "iOS":
            self.requires("zstd/1.5.7")
        if self.settings.os == "Emscripten":
            self.requires("zlib/1.4")

    def build_requirements(self):
        if self.settings.os != "Windows" and cross_building(self):
            self.tool_requires(f"{self.name}/{self.version}")

    def layout(self):
        basic_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        if self.settings.os == "Windows":
            VCVars(self).generate()
        elif self.settings.os != "Macos":
            PkgConfigDeps(self).generate()

    def build(self):
        if self.settings.os == "Windows":
            self._build_windows()
        elif self.settings.os == "Macos":
            self._build_macos()
        else:
            self._build_unix()

    def _host_interpreter(self):
        launcher = "python" if self.settings.os == "Windows" else "python3"
        output = StringIO()
        self.run(
            f'{launcher} -c "import sys; print(sys.executable)"',
            stdout=output,
            quiet=True,
            env=None,
        )
        return Path(output.getvalue().strip())

    def _build_windows(self):
        source = Path(self.source_folder)
        arm = self.settings.arch == "armv8"
        platform = "ARM64" if arm else "x64"
        output = source / "PCbuild" / ("arm64" if arm else "amd64")
        python = self._host_interpreter()
        toolset = '"/p:PlatformToolset=v145"'
        if self.settings.compiler == "clang":
            compiler = self.conf.get(
                "tools.build:compiler_executables", check_type=dict
            )
            llvm = Path(compiler["c"]).parent.parent
            toolset = f'"/p:PlatformToolset=ClangCL" "/p:LLVMInstallDir={llvm}"'
        env = Environment()
        env.unset("PYTHON")
        env.define("PYTHON_FOR_BUILD", str(python))
        env.define("PYTHONNOUSERSITE", "1")
        with env.vars(self).apply():
            for config in ("Release", "Debug"):
                self.run(
                    f"call PCbuild\\build.bat -p {platform} -c {config} "
                    f'{toolset} "/p:GIT="',
                    cwd=self.source_folder,
                )
        # Layout is a pure Python tool. On a cross build run it with the build
        # machine's interpreter while reading both target CPython images.
        env = Environment()
        env.define_path("PYTHONINCLUDE", str(source / "Include"))
        args = [
            python,
            source / "PC/layout/main.py",
            "--source",
            source,
            "--build",
            output,
            "--arch",
            output.name,
            "--copy",
            Path(self.build_folder) / "image",
            "--include-stable",
            "--include-pip",
            "--include-tcltk",
            "--include-idle",
            "--include-tools",
            "--include-venv",
            "--include-dev",
            "--include-launchers",
            "--include-symbols",
        ]
        with env.vars(self).apply():
            for debug in (True, False):
                command = [*args, "--debug"] if debug else args
                self.run(list2cmdline(map(str, command)), cwd=source)

    def _build_unix(self):
        source = Path(self.source_folder)
        build = Path(self.build_folder)
        ios = self.settings.os == "iOS"
        android = self.settings.os == "Android"
        wasm = self.settings.os == "Emscripten"
        windows_android = android and self.settings_build.os == "Windows"
        env = Environment()
        build_env = VirtualBuildEnv(self, auto_generate=True)
        symbols = self.conf.get("user.package:debug_symbols")
        if symbols:
            env.append("CFLAGS_NODIST", "-g")
        env.define_path("PKG_CONFIG_PATH", self.generators_folder)
        env.define(
            "PKG_CONFIG_LIBDIR", self.generators_folder if cross_building(self) else ""
        )
        pkg_config = which("pkg-config")
        if not pkg_config:
            raise ConanException("CPython requires pkg-config")
        env.define("PKG_CONFIG", f"{pkg_config} --static")
        args = [
            (source / "configure").as_posix(),
            "--disable-test-modules",
            "--with-ensurepip=no",
        ]
        if not wasm:
            openssl = self.dependencies["openssl"].package_folder
            args.extend((f"--with-openssl={openssl}", "--with-openssl-rpath=no"))
        if ios:
            sdk = str(self.settings.os.sdk)
            suffix = "-simulator" if sdk == "iphonesimulator" else ""
            env.define_path(
                "PATH",
                [str(source / "Platforms/Apple/iOS/Resources/bin"), "/usr/bin", "/bin"],
            )
            args.extend(
                (
                    f"--enable-framework={build / 'image'}",
                    f"--host=arm64-apple-ios{self.settings.os.version}{suffix}",
                    "--build=arm64-apple-darwin",
                )
            )
        elif android:
            target = {
                "armv8": "aarch64-linux-android",
                "x86_64": "x86_64-linux-android",
            }[str(self.settings.arch)]
            if windows_android:
                # The Windows NDK also ships shell wrappers for Autoconf.
                compilers = self.conf.get(
                    "tools.build:compiler_executables", check_type=dict
                )
                for var, language in (("CC", "c"), ("CXX", "cpp")):
                    compiler = Path(compilers[language]).with_suffix("")
                    env.define_path(var, compiler.as_posix())
            env.define("CFLAGS", "-D__BIONIC_NO_PAGE_SIZE_MACRO")
            env.define(
                "LDFLAGS",
                "-Wl,--build-id=sha1,--no-rosegment,-z,max-page-size=16384,"
                "--no-undefined -lm",
            )
            build_system = {
                "Linux": "x86_64-linux-gnu",
                "Macos": "aarch64-apple-darwin",
                "Windows": "x86_64-w64-mingw32",
            }[str(self.settings_build.os)]
            args.extend(
                (
                    "--prefix=/python",
                    f"--host={target}",
                    f"--build={build_system}",
                    "--enable-shared",
                    "--without-static-libpython",
                )
            )
        elif wasm:
            arch = "wasm64" if self.settings.arch == "wasm64" else "wasm32"
            build_system = "apple-darwin" if sys.platform == "darwin" else "linux-gnu"
            env.append("LDFLAGS", "-sEMULATE_FUNCTION_POINTER_CASTS=1")
            if symbols:
                env.append("LDFLAGS", "-gseparate-dwarf")
            node = build_env.vars().get("EMSDK_NODE") or os.environ.get(
                "EMSDK_NODE", "node"
            )
            env.define("HOSTRUNNER", shlex.quote(node))
            # CPython defaults to Emscripten ports when these flags are empty.
            for name in ("zlib", "bzip2"):
                dependency = self.dependencies[name].cpp_info.aggregated_components()
                includes = [f"-I{path}" for path in dependency.includedirs]
                libraries = [f"-L{path}" for path in dependency.libdirs]
                libraries.extend(f"-l{library}" for library in dependency.libs)
                env.define(f"{name.upper()}_CFLAGS", shlex.join(includes))
                env.define(f"{name.upper()}_LIBS", shlex.join(libraries))
            # CPython's Emscripten ctypes module needs a patched libffi.
            env.define("py_cv_module__ctypes", "n/a")
            env.define("py_cv_module__ssl", "n/a")
            env.define("py_cv_module__hashlib", "n/a")
            env.define_path(
                "CONFIG_SITE",
                str(source / "Platforms/emscripten/config.site-wasm32-emscripten"),
            )
            args.extend(
                (
                    "--prefix=/python",
                    f"--host={arch}-unknown-emscripten",
                    f"--build={os.uname().machine}-{build_system}",
                    "--disable-shared",
                    "--disable-wasm-dynamic-linking",
                    "--disable-ipv6",
                    "--without-pymalloc",
                    "--without-system-libmpdec",
                )
            )
        else:
            args.extend((f"--prefix={build / 'image'}", "--enable-shared"))
            if cross_building(self):
                args.extend(("--host=aarch64-linux-gnu", "--build=x86_64-linux-gnu"))
                env.define("ac_cv_buggy_getaddrinfo", "no")
                env.define("ac_cv_file__dev_ptmx", "yes")
                env.define("ac_cv_file__dev_ptc", "no")
            # make must preserve the dollar signs for the runtime loader.
            env.define("LDFLAGS", r"-Wl,-rpath,\$$ORIGIN/../lib")
        if cross_building(self):
            native = Path(self.dependencies.build[self.name].package_folder) / "python"
            interpreter = native / "bin/python3"
            if windows_android:
                interpreter = native / "python.exe"
            elif self.settings_build.os == "Macos":
                interpreter = native / "Python.framework/Versions/Current/bin/python3"
            args.append(f"--with-build-python={interpreter.as_posix()}")
        install = "make install"
        if android or wasm:
            staging = build / "staging"
            install += f" {shlex.quote(f'DESTDIR={staging.as_posix()}')}"
        # Apply the environment without shell expansion of the literal $ORIGIN flags.
        env.compose_env(build_env.environment())
        with env.vars(self).apply():
            self.run(shlex.join(args), cwd=build, env=None)
            self.run(f"make -j{build_jobs(self)}", cwd=build, env=None)
            self.run(install, cwd=build, env=None)
        if android or wasm:
            rename(self, staging / "python", build / "image")
        if wasm:
            if not copy(self, "python.wasm", build, build / "image/bin"):
                raise ConanException("Missing python.wasm")
        if self.settings.os == "Linux":
            (build / "image/bin/python").symlink_to("python3")

    def _build_macos(self):
        source = Path(self.source_folder)
        build = Path(self.build_folder)
        scripts = source.parent / "platform/macos"
        staging = build / "installer"
        zstd = Path(self.dependencies["zstd"].package_folder)
        python = self._host_interpreter()
        env = Environment()
        if self.conf.get("user.package:debug_symbols"):
            env.append("CFLAGS_NODIST", "-g")
        env.define_path("OPENSSL_ROOT_DIR", self.dependencies["openssl"].package_folder)
        env.define_path("SDK_TOOLS_BIN", str(python.parent))
        env.define("ac_cv_header_libintl_h", "no")
        env.define("ac_cv_lib_intl_textdomain", "no")
        env.define("tcl_cv_strtod_buggy", "1")
        env.define("ac_cv_func_strtod", "yes")
        env.define("LIBZSTD_CFLAGS", f"-I{shlex.quote(str(zstd / 'include'))}")
        env.define("LIBZSTD_LIBS", shlex.quote(str(zstd / "lib/libzstd.a")))
        command = shlex.join(
            (
                str(python),
                "build-installer.py",
                "--universal-archs=arm64",
                f"--build-dir={staging}",
                f"--third-party={build / 'downloads'}",
                f"--dep-target={self.settings.os.version}",
            )
        )
        with env.vars(self).apply():
            self.run(command, cwd=source / "Mac/BuildScript")
        image = build / "image"
        mkdir(self, image)
        framework = image / "Python.framework"
        rename(self, staging / "_root/Library/Frameworks/Python.framework", framework)
        signing = self.python_requires["apple-signing"].module
        relocate = run_path(str(scripts / "relocate.py"))["relocate"]
        relocate(self, framework, self.version.rsplit(".", 1)[0], signing.is_macho)
        signing.sign_tree(self, framework, identity="-")
        interpreter = framework / "Versions/3.15/bin/python3.15"
        self.run(
            f"{shlex.quote(str(interpreter))} -B -m compileall -q -f "
            "--invalidation-mode checked-hash -o 0 -o 1 -o 2 "
            f"-x 'bad_coding|badsyntax' -s {shlex.quote(str(image))} "
            f"{shlex.quote(str(framework / 'Versions/3.15/lib/python3.15'))}"
        )
        signing.sign_tree(self, framework, identity="-")

    def package(self):
        package = Path(self.package_folder)
        source = Path(self.source_folder)
        if not copy(self, "*", Path(self.build_folder) / "image", package / "python"):
            raise ConanException("Missing the CPython installation")
        copy(self, "LICENSE", source, package / "licenses")
        platform = str(self.settings.os).lower()
        if not copy(
            self, "python-config.cmake", source.parent / "platform" / platform, package
        ):
            raise ConanException(f"Missing python-config.cmake for {platform}")
        for name, dependency in self.dependencies.host.items():
            if dependency.package_folder:
                copy(
                    self,
                    "*",
                    Path(dependency.package_folder) / "licenses",
                    package / "licenses" / name.ref.name,
                )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "O3DEVendoredPython")
        if self.settings.os == "Windows":
            self.cpp_info.bindirs = ["python"]
            self.cpp_info.includedirs = ["python/include"]
            self.cpp_info.libdirs = ["python/libs"]
        elif self.settings.os == "Macos":
            framework = "python/Python.framework/Versions/Current"
            self.cpp_info.bindirs = [f"{framework}/bin"]
            self.cpp_info.includedirs = [f"{framework}/Headers"]
            self.cpp_info.libdirs = [framework]
        elif self.settings.os == "iOS":
            self.cpp_info.bindirs = []
            self.cpp_info.includedirs = ["python/Python.framework/Headers"]
            self.cpp_info.libdirs = ["python/Python.framework"]
        elif self.settings.os == "Android":
            self.cpp_info.bindirs = []
            self.cpp_info.includedirs = ["python/include/python3.15"]
            self.cpp_info.libdirs = ["python/lib"]
            self.cpp_info.libs = ["python3.15"]
        else:
            self.cpp_info.bindirs = ["python/bin"]
            self.cpp_info.includedirs = ["python/include/python3.15"]
            self.cpp_info.libdirs = ["python/lib"]
