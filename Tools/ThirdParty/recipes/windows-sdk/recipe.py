from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get, save


# Keyed by NuGet package id. The flat-container API lowercases ids in the URL, so ids must be
# lowercase here (they double as the download filename). Both arch lib packages are listed, but a
# build only pulls the one matching the target arch.
_SHA256 = {
    "microsoft.windows.sdk.cpp": "0a4887a64d1b17128f9ffd80032f20debabf943f34eea927aac00cc46cb32879",
    "microsoft.windows.sdk.cpp.x64": "72076d00b16c3882bf9eab0c80e350f33d8bc65e5f29e2f0e9b66a3f2569ccb1",
    "microsoft.windows.sdk.cpp.arm64": "543c2b31dfa77e00cc63860a587178c5c90837f64c05b05f73cb18b1c33f549a",
    "microsoft.windows.sdk.buildtools": "d939fa052f9c80f878b2a28b7071a6f2c9a51029018bb87a835ebda6e535a002",
}

_ARCH = {"X64": "x64", "ARM": "arm64"}
_INCLUDE_SUBDIRS = ("ucrt", "shared", "um", "winrt", "cppwinrt")
_LIB_APIS = ("ucrt", "um")
_CMAKE_MODULE = "cmake/recipe-official-windows-sdk-variables.cmake"


class Recipe(RecipeBase):
    name = "windows-sdk"
    version = "10.0.28000.2270"
    license = "Microsoft Windows SDK License"

    def build(self) -> None:
        # Headers/sources, the target-arch libs only, and the tools -- not both arches.
        cpp_arch = f"microsoft.windows.sdk.cpp.{_ARCH[self.settings.arch]}"
        for pkg_id in ("microsoft.windows.sdk.cpp", cpp_arch, "microsoft.windows.sdk.buildtools"):
            filename = f"{pkg_id}.{self.version}.nupkg"
            get(
                self,
                url=f"https://api.nuget.org/v3-flatcontainer/{pkg_id}/{self.version}/{filename}",
                sha256=_SHA256[pkg_id],
                destination=self.folders.build,
                filename=filename)

    def package(self) -> None:
        build = self.folders.build
        pkg = self.folders.package
        sdk_ver = self._only_child(build / "c" / "Include")
        bin_ver = self._only_child(build / "bin")
        arch = _ARCH[self.settings.arch]
        host = _ARCH[self.settings_build.arch]

        copy(self, "*", src=build / "c" / "Include" / sdk_ver, dst=pkg / "include" / sdk_ver)
        copy(self, "*", src=build / "c" / "Source" / sdk_ver, dst=pkg / "src" / sdk_ver)
        for api in _LIB_APIS:
            copy(self, "*", src=build / "c" / api / arch, dst=pkg / "lib" / sdk_ver / api / arch)
        copy(self, "*", src=build / "bin" / bin_ver / host, dst=pkg / "bin")

        self._write_license()
        self._write_cmake_module(sdk_ver, arch)

    def package_info(self) -> None:
        root = self.folders.package
        sdk_ver = self._only_child(root / "include")
        arch = _ARCH[self.settings.arch]

        self.info.includedirs = [f"include/{sdk_ver}/{s}" for s in _INCLUDE_SUBDIRS]
        self.info.libdirs = [f"lib/{sdk_ver}/{api}/{arch}" for api in _LIB_APIS]
        self.info.bindirs = ["bin"]

        self.info.set_property("cmake_file_name", "WindowsSDK")
        self.info.set_property("cmake_target_name", "WindowsSDK::WindowsSDK")
        self.info.set_property("cmake_build_modules", [_CMAKE_MODULE])

        self.info.buildenv.define_path("WindowsSdkDir", root)
        self.info.buildenv.define("WindowsSDKVersion", f"{sdk_ver}\\")
        self.info.buildenv.define("UCRTVersion", sdk_ver)
        for d in self.info.includedirs:
            self.info.buildenv.append_path("INCLUDE", root / d)
        for d in self.info.libdirs:
            self.info.buildenv.append_path("LIB", root / d)
        self.info.buildenv.prepend_path("PATH", root / "bin")

    @staticmethod
    def _only_child(folder: Path) -> str:
        return next(p.name for p in sorted(folder.iterdir()) if p.is_dir())

    def _write_license(self) -> None:
        # The SDK license is URL-only (nuspec licenseUrl); record it. cppwinrt ships its own
        # LICENSE.txt alongside its headers under include/<ver>/cppwinrt.
        save(
            self,
            self.folders.package / "licenses" / "NOTICE.txt",
            f"Microsoft Windows SDK ({self.version})\n"
            "(c) Microsoft Corporation. All rights reserved.\n"
            "Licensed under the Microsoft Software License Terms for the Windows SDK.\n"
            "https://aka.ms/WinSDKLicenseURL\n")

    def _write_cmake_module(self, sdk_ver: str, arch: str) -> None:
        includes = "\n".join(f'    "${{_root}}/include/{sdk_ver}/{s}"' for s in _INCLUDE_SUBDIRS)
        libs = "\n".join(f'    "${{_root}}/lib/{sdk_ver}/{api}/{arch}"' for api in _LIB_APIS)
        content = (
            'get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)\n'
            f'set(WindowsSDK_VERSION "{sdk_ver}")\n'
            'set(WindowsSDK_ROOT "${_root}")\n'
            f'set(WindowsSDK_INCLUDE_DIRS\n{includes})\n'
            f'set(WindowsSDK_LIBRARY_DIRS\n{libs})\n'
            'set(WindowsSDK_BIN_DIR "${_root}/bin")\n'
            'unset(_root)\n')
        save(self, self.folders.package / _CMAKE_MODULE, content)
