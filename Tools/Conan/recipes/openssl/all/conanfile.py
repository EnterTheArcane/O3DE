#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import build_jobs
from conan.tools.files import copy, get, rename, replace_in_file, rmdir
from conan.tools.gnu import AutotoolsToolchain
from conan.tools.layout import basic_layout


CONFIGURE_TARGETS = {
    ("Windows", "x86_64"): "VC-WIN64A",
    ("Windows", "armv8"): "VC-WIN64-CLANGASM-ARM",
    ("Linux", "x86_64"): "linux-x86_64",
    ("Linux", "armv8"): "linux-aarch64",
    ("Macos", "armv8"): "darwin64-arm64-cc",
    ("Android", "armv8"): "android-arm64",
    ("Android", "x86_64"): "android-x86_64",
    ("iOS", "armv8", "iphoneos"): "ios64-xcrun",
    ("iOS", "armv8", "iphonesimulator"): "iossimulator-arm64-xcrun",
    ("Emscripten", "wasm"): "cc",
    ("Emscripten", "wasm64"): "cc",
}


class Recipe(ConanFile):
    name = "openssl"
    license = "Apache-2.0"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    @property
    def _platform(self):
        platform = (str(self.settings.os), str(self.settings.arch))
        if self.settings.os == "iOS":
            platform += (str(self.settings.os.sdk),)
        return platform

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")
        self.win_bash = (
            self.settings.os == "Android" and self.settings_build.os == "Windows"
        )

    def validate(self):
        if self._platform not in CONFIGURE_TARGETS:
            raise ConanInvalidConfiguration(
                f"Unsupported OpenSSL target: {self._platform}"
            )
        if self.settings.os == "Windows" and (
            self.settings.compiler not in ("msvc", "clang")
            or self.settings.compiler.get_safe("runtime") != "dynamic"
        ):
            raise ConanInvalidConfiguration(
                "The Windows SDK requires an MSVC-compatible compiler with the dynamic CRT"
            )

    def layout(self):
        basic_layout(self, src_folder="src")

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("jom/1.1.7")
            self.tool_requires("strawberryperl/[>=5.32 <6]")
            if self.settings.arch == "x86_64":
                self.tool_requires("nasm/2.15.05")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = AutotoolsToolchain(self)
        if self.settings.os == "Windows":
            tc.extra_cflags.append("-nologo")
        tc.generate()

    @property
    def _make(self):
        return "jom" if self.settings.os == "Windows" else "make"

    def build(self):
        windows = self.settings.os == "Windows"
        args = [
            CONFIGURE_TARGETS[self._platform],
            "--debug" if self.settings.build_type == "Debug" else "--release",
            "--prefix=C:/" if windows else "--prefix=/",
            "--libdir=lib",
            "no-shared",
            "no-docs",
            "no-unit-test",
            "no-tests",
        ]
        if self.conf.get("user.package:debug_symbols"):
            if windows:
                replace_in_file(
                    self,
                    Path(self.source_folder) / "Configurations/10-main.conf",
                    '"/Z7"',
                    '"/Zi /FS /Fdopenssl-compile.pdb"',
                    strict=False,
                )
            else:
                args.append("-g")
        if not windows:
            args.append("-fPIC")
        if self.settings.os == "Android":
            args.append(f"-D__ANDROID_API__={self.settings.os.api_level}")
        if self.settings.os == "iOS":
            deployment = self.settings.os.version
            target = "iphoneos"
            if self.settings.os.sdk == "iphonesimulator":
                target = "ios-simulator"
            args.extend(("no-module", f"-m{target}-version-min={deployment}"))
        elif self.settings.os == "Emscripten":
            args.extend(("no-module", "no-asm", "no-async", "no-threads"))
        else:
            args.append("enable-fips")

        configure = (Path(self.source_folder) / "Configure").as_posix()
        self.run(f'perl "{configure}" {" ".join(args)}')
        target = " build_libs" if self.settings.os in ("iOS", "Emscripten") else ""
        self.run(f"{self._make} -j{build_jobs(self)}{target}")
        if windows:
            replace_in_file(self, "Makefile", "INSTALLTOP_dir=\\", "INSTALLTOP_dir=/")

    def package(self):
        package = Path(self.package_folder)
        target = (
            "install_dev" if self.settings.os in ("iOS", "Emscripten") else "install"
        )
        self.run(f'{self._make} {target} -j1 DESTDIR="{package.as_posix()}"')
        if self.settings.os == "Windows":
            rename(self, package / "Program Files/Common Files/SSL", package / "ssl")
        copy(self, "LICENSE.txt", self.source_folder, package / "licenses")
        rmdir(self, package / "lib/pkgconfig")
        rmdir(self, package / "lib/cmake")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenSSL")
        prefix = "lib" if self.settings.os == "Windows" else ""
        crypto = self.cpp_info.components["crypto"]
        crypto.libs = [f"{prefix}crypto"]
        crypto.set_property("cmake_target_name", "OpenSSL::Crypto")
        crypto.set_property("pkg_config_name", "libcrypto")
        ssl = self.cpp_info.components["ssl"]
        ssl.libs = [f"{prefix}ssl"]
        ssl.requires = ["crypto"]
        ssl.set_property("cmake_target_name", "OpenSSL::SSL")
        ssl.set_property("pkg_config_name", "libssl")

        if self.settings.os == "Windows":
            crypto.system_libs = ["crypt32", "ws2_32", "advapi32", "user32", "bcrypt"]
        elif self.settings.os == "Linux":
            crypto.system_libs = ["dl", "rt", "pthread"]
        elif self.settings.os == "Android":
            crypto.system_libs = ["dl"]
        modules = Path(self.package_folder) / "lib/ossl-modules"
        self.runenv_info.define_path("OPENSSL_MODULES", str(modules))
