#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path
import stat
from zipfile import ZipFile

from conan import ConanFile
from conan.errors import ConanException, ConanInvalidConfiguration
from conan.tools.files import copy, download, rm, unzip
from conan.tools.layout import basic_layout


HOST_OS = {"Linux": "linux", "Macos": "darwin", "Windows": "windows"}


class Recipe(ConanFile):
    name = "android-ndk"
    license = "Apache-2.0"
    homepage = "https://developer.android.com/ndk/"
    package_type = "application"
    settings = "os", "arch"
    no_copy_source = True

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    @property
    def _archive_arch(self):
        if self.settings.os == "Macos" and self.settings.arch in ("armv8", "x86_64"):
            return "x86_64"
        return str(self.settings.arch)

    def validate(self):
        archives = self.conan_data["sources"][self.version]
        if self._archive_arch not in archives.get(str(self.settings.os), {}):
            raise ConanInvalidConfiguration(
                f"No Android NDK r30 archive for {self.settings.os} {self.settings.arch}"
            )

    def package_id(self):
        if self.info.settings.os == "Macos":
            self.info.settings.arch = "universal:armv8/x86_64"

    def layout(self):
        basic_layout(self, src_folder="src")

    def build(self):
        root = Path(self.build_folder).resolve() / "ndk"
        archive = root.with_suffix(".zip")
        data = self.conan_data["sources"][self.version][str(self.settings.os)]
        download(self, **data[self._archive_arch], filename=str(archive))
        unzip(
            self, str(archive), destination=root, strip_root=True, keep_permissions=True
        )
        # Conan preserves ZIP modes but extracts symlink entries as regular files.
        with ZipFile(archive) as source:
            for entry in source.infolist():
                if not stat.S_ISLNK(entry.external_attr >> 16):
                    continue
                path = root / Path(entry.filename).relative_to("android-ndk-r30")
                target = Path(source.read(entry).decode())
                if (
                    not path.resolve().is_relative_to(root)
                    or target.is_absolute()
                    or not (path.parent / target).resolve().is_relative_to(root)
                ):
                    raise ConanException(
                        f"Invalid NDK archive symlink: {entry.filename}"
                    )
                path.unlink()
                path.symlink_to(target)
        rm(self, archive.name, archive.parent)

    def package(self):
        sdk = Path(self.build_folder) / "ndk"
        copy(self, "*", sdk, self.package_folder)
        copy(self, "*NOTICE*", sdk, Path(self.package_folder) / "licenses")

    def package_info(self):
        root = Path(self.package_folder)
        host = f"{HOST_OS[str(self.settings.os)]}-{self._archive_arch}"
        bin_dir = f"toolchains/llvm/prebuilt/{host}/bin"
        tools = root / bin_dir
        self.cpp_info.bindirs = [bin_dir]
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.conf_info.define("tools.android:ndk_path", root.as_posix())
        self.buildenv_info.define_path("ANDROID_NDK_ROOT", root.as_posix())
        self.buildenv_info.define_path("ANDROID_NDK_HOME", root.as_posix())

        windows = self.settings.os == "Windows"
        suffix = ".exe" if windows else ""
        for tool in (
            "addr2line",
            "ar",
            "nm",
            "objcopy",
            "objdump",
            "ranlib",
            "readelf",
            "strip",
        ):
            self.buildenv_info.define_path(
                tool.upper(), (tools / f"llvm-{tool}{suffix}").as_posix()
            )

        target = self.settings_target
        if target is None or target.get_safe("os") != "Android":
            return
        abi, triplet = {
            "armv7": ("armeabi-v7a", "armv7a-linux-androideabi"),
            "armv8": ("arm64-v8a", "aarch64-linux-android"),
            "x86": ("x86", "i686-linux-android"),
            "x86_64": ("x86_64", "x86_64-linux-android"),
        }[str(target.arch)]
        api_level = str(target.os.api_level)
        clang = (tools / f"{triplet}{api_level}-clang").as_posix()
        suffix = ".cmd" if windows else ""
        compilers = {"c": f"{clang}{suffix}", "cpp": f"{clang}++{suffix}"}
        self.conf_info.define("tools.build:compiler_executables", compilers)
        self.buildenv_info.define_path("CC", compilers["c"])
        self.buildenv_info.define_path("CXX", compilers["cpp"])

        toolchain = tools.parent
        sysroot = (toolchain / "sysroot").as_posix()
        self.conf_info.define("tools.build:sysroot", sysroot)
        self.buildenv_info.define_path("SYSROOT", sysroot)
        self.buildenv_info.define_path("NDK_ROOT", toolchain.as_posix())
        self.buildenv_info.define("CHOST", triplet.replace("armv7a-", "arm-", 1))
        self.buildenv_info.define("ANDROID_ABI", abi)
        self.buildenv_info.define("ANDROID_NATIVE_API_LEVEL", api_level)
        self.buildenv_info.define("ANDROID_PLATFORM", f"android-{api_level}")
        self.buildenv_info.define("ANDROID_TOOLCHAIN", "clang")
        stl = target.get_safe("compiler.libcxx")
        self.buildenv_info.define("ANDROID_STL", stl or "c++_shared")
