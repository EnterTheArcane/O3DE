#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import json
import shlex
from base64 import b64decode
from pathlib import Path
from shutil import which

from conan import ConanFile
from conan.errors import ConanException, ConanInvalidConfiguration
from conan.tools.build import build_jobs
from conan.tools.files import (
    apply_conandata_patches,
    chdir,
    copy,
    download,
    export_conandata_patches,
    get,
    load,
    rename,
    replace_in_file,
    save,
)
from conan.tools.microsoft import VCVars

GN_OS = {
    "Windows": "win",
    "Linux": "linux",
    "Macos": "mac",
    "Android": "android",
    "iOS": "ios",
}
GN_CPU = {"x86_64": "x64", "armv8": "arm64"}


class Recipe(ConanFile):
    name = "crashpad"
    license = "Apache-2.0"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "static-library"

    def export_sources(self):
        export_conandata_patches(self)

    def validate(self):
        os = str(self.settings.os)
        build_os = str(self.settings_build.os)
        if os not in GN_OS or str(self.settings.arch) not in GN_CPU:
            raise ConanInvalidConfiguration(
                "Crashpad requires a supported native target"
            )
        if os == "Windows" and (
            build_os != "Windows" or self.settings_build.arch != "x86_64"
        ):
            raise ConanInvalidConfiguration(
                "The O3DE Crashpad handler requires a Windows x64 build host"
            )
        if os == "Android" and build_os not in ("Linux", "Macos"):
            raise ConanInvalidConfiguration(
                "Android Crashpad requires a Linux or macOS build host"
            )
        if os in ("Macos", "iOS") and build_os != "Macos":
            raise ConanInvalidConfiguration(
                "Apple Crashpad targets require Xcode on macOS"
            )

    def requirements(self):
        self.requires("zlib/1.4")
        if self.settings.os == "Linux":
            # Crashpad needs the headers and loads the system libcurl at runtime.
            self.requires("libcurl/[>=8 <9]", libs=False, options={"with_ssl": False})

    def build_requirements(self):
        self.tool_requires("gn/2022.12.12")
        self.tool_requires("mini-chromium/2026.09.04", package_id_mode="full_mode")
        self.tool_requires("ninja/1.13.2")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        if self.settings.os == "Windows":
            VCVars(self).generate()

    def build(self):
        source = Path(self.source_folder)
        os = str(self.settings.os)
        windows = os == "Windows"
        if windows:
            apply_conandata_patches(self)
        mini = source / "third_party/mini_chromium/mini_chromium"
        mini_package = Path(self.dependencies.build["mini-chromium"].package_folder)
        copy(self, "*", mini_package / "src", mini)
        zlib = Path(self.dependencies["zlib"].package_folder).as_posix()
        zlib_name = "zlib.lib" if windows else "libz.a"
        save(
            self,
            source / "third_party/zlib/BUILD.gn",
            f"""config("zlib_config") {{
  defines = [ "CRASHPAD_ZLIB_SOURCE_SYSTEM" ]
  include_dirs = [ "{zlib}/include" ]
}}
source_set("zlib") {{
  public_configs = [ ":zlib_config" ]
  libs = [ "{zlib}/lib/{zlib_name}" ]
}}
""",
        )
        python = which("python") if windows else which("python3")
        if not python:
            raise ConanException("Crashpad requires a host Python interpreter")
        replace_in_file(
            self,
            source / ".gn",
            'script_executable = "python3"',
            f'script_executable = "{Path(python).as_posix()}"',
            strict=False,
        )
        debug = self.settings.build_type == "Debug"
        args = {
            "host_os": GN_OS[str(self.settings_build.os)],
            "host_cpu": GN_CPU[str(self.settings_build.arch)],
            "target_os": GN_OS[os],
            "target_cpu": GN_CPU[str(self.settings.arch)],
            "is_debug": debug,
        }
        symbols = self.conf.get("user.package:debug_symbols")
        if windows:
            # Use the MSVC target environment selected by Conan's VCVars.
            replace_in_file(
                self,
                mini / "build/win_helper.py",
                "        args.extend((script_arch_name, '&&', 'set'))",
                '        args = ["cmd.exe", "/d", "/c", "set"]',
            )
            args["mini_chromium_is_clang"] = self.settings.compiler == "clang"
            flags = ["/MDd" if debug else "/MD"]
            if symbols:
                flags.extend(("/Zi", "/FS", "/Fdcrashpad-compile.pdb"))
        else:
            flags = ["-fPIC"]
            if symbols:
                flags.append("-g")
        if (windows and args["mini_chromium_is_clang"]) or os == "Linux":
            compilers = self.conf.get(
                "tools.build:compiler_executables", check_type=dict
            )
            if compilers:
                args["clang_path"] = Path(compilers["c"]).parent.parent.as_posix()
        if os in ("Linux", "Android"):
            # Gitiles generates archive bytes on demand. Pin its file contents.
            for name, asset in self.conan_data["lss"][self.version].items():
                encoded = source / f"{name}.b64"
                download(self, **asset, filename=encoded)
                save(
                    self,
                    source / "third_party/lss/lss" / name,
                    b64decode(load(self, encoded)).decode(),
                )
        if os == "Linux":
            curl = self.dependencies["libcurl"].cpp_info
            flags.extend(f"-I{directory}" for directory in curl.includedirs)
            args["crashpad_http_transport_impl"] = "libcurl"
        elif os == "Android":
            args["android_ndk_root"] = Path(
                self.dependencies.build["android-ndk"].package_folder
            ).as_posix()
            args["android_api_level"] = int(str(self.settings.os.api_level))
            args["crashpad_http_transport_impl"] = "socket"
        elif os == "Macos":
            args["mac_deployment_target"] = str(self.settings.os.version)
        elif os == "iOS":
            # The final application signs the in-process client.
            args["ios_enable_code_signing"] = False
            args["target_environment"] = (
                "simulator" if self.settings.os.sdk == "iphonesimulator" else "device"
            )
            args["ios_deployment_target"] = str(self.settings.os.version)
        args["extra_cflags"] = " ".join(flags) if windows else shlex.join(flags)
        save(
            self,
            source / "out/args.gn",
            "".join(f"{key} = {json.dumps(value)}\n" for key, value in args.items()),
        )
        if windows:
            targets = [
                "obj/handler/handler.lib",
                "obj/handler/common.lib",
                "obj/client/client.lib",
                "obj/tools/tool_support.lib",
            ]
        else:
            # GN collects the client and its transitive objects into one archive.
            build_file = source / "BUILD.gn"
            save(
                self,
                build_file,
                load(self, build_file)
                + """
static_library("conan_crashpad") {
  output_name = "crashpad"
  complete_static_lib = true
  deps = [ "//client" ]
}
""",
            )
            targets = ["conan_crashpad"]
            if os != "iOS":
                targets.append("crashpad_handler")
            if os == "Android":
                targets.append("crashpad_handler_trampoline")
        with chdir(self, source):
            self.run("gn gen out")
            self.run(f"ninja -C out -j{build_jobs(self)} {' '.join(targets)}")

    def package(self):
        package = Path(self.package_folder)
        source = Path(self.source_folder)
        objects = source / "out/obj"
        for part in (
            "client",
            "compat",
            "handler",
            "minidump",
            "snapshot",
            "tools",
            "util",
            "third_party/getopt",
            "build",
        ):
            copy(self, "*.h", source / part, package / "include" / part)
        mini = Path("third_party/mini_chromium/mini_chromium")
        copy(self, "*.h", source / mini, package / "include/mini_chromium")
        copy(self, "*.h", source / "out/gen", package / "include/mini_chromium")
        copy(self, "LICENSE", source, package / "licenses")
        save(
            self,
            package / "licenses/mini-chromium.txt",
            load(self, source / mini / "LICENSE"),
        )
        save(
            self,
            package / "licenses/getopt.txt",
            load(self, source / "third_party/getopt/LICENSE"),
        )
        libraries = package / "lib"
        if self.settings.os != "Windows":
            if not copy(self, "libcrashpad.a", objects, libraries, keep_path=False):
                raise ConanException("Missing the Crashpad client archive")
            binaries = []
            if self.settings.os != "iOS":
                binaries.append("crashpad_handler")
            if self.settings.os == "Android":
                binaries.append("libcrashpad_handler_trampoline.so")
            for binary in binaries:
                if not copy(
                    self, binary, source / "out", package / "bin", keep_path=False
                ):
                    raise ConanException(f"Missing {binary}")
            if self.settings.os in ("Linux", "Android"):
                save(
                    self,
                    package / "licenses/linux-syscall-support.txt",
                    load(self, source / "third_party/lss/lss/LICENSE"),
                )
            return
        for path, name in (
            ("client/client.lib", "client"),
            ("client/common.lib", "client_common"),
            ("handler/handler.lib", "handler"),
            ("handler/common.lib", "handler_common"),
            (f"{mini.as_posix()}/base/base.lib", "base"),
            ("compat/compat.lib", "compat"),
            ("minidump/minidump.lib", "minidump"),
            ("snapshot/snapshot.lib", "snapshot"),
            ("minidump/format.lib", "format"),
            ("snapshot/context.lib", "context"),
            ("third_party/getopt/getopt.lib", "getopt"),
            ("tools/tool_support.lib", "tool_support"),
            ("util/net.lib", "net"),
            ("util/util.lib", "util"),
        ):
            (library,) = copy(self, path, objects, libraries, keep_path=False)
            if Path(library).name != f"{name}.lib":
                rename(self, library, libraries / f"{name}.lib")

    def package_info(self):
        self.cpp_info.includedirs = ["include", "include/mini_chromium"]
        if self.settings.os == "Windows":
            self.cpp_info.libs = ["client", "client_common", "base", "util"]
            self.cpp_info.system_libs = ["rpcrt4", "user32", "version", "winhttp"]
            return
        self.cpp_info.libs = ["crashpad"]
        if self.settings.os == "Linux":
            self.cpp_info.system_libs = ["dl", "pthread"]
        elif self.settings.os == "Android":
            self.cpp_info.system_libs = ["dl", "log"]
        elif self.settings.os == "Macos":
            self.cpp_info.system_libs = ["bsm"]
            self.cpp_info.frameworks = [
                "CoreFoundation",
                "Foundation",
                "IOKit",
                "Security",
            ]
        elif self.settings.os == "iOS":
            self.cpp_info.frameworks = [
                "CoreFoundation",
                "Foundation",
                "UIKit",
                "Security",
            ]
