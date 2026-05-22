"""Qt 6 recipe — builds individual Qt submodules from separate tarballs.

Downloads per-module tarballs (e.g. qtbase-everywhere-src-6.10.2.tar.xz) rather
than the monolithic qt-everywhere-src archive (~600 MB) so only the modules we
need are fetched.

Each module is cmake-configured, built, and installed into the shared
package_folder in dependency order (qtbase first, then the rest).

System packages used (instead of Qt bundled):
  - ZLIB      → ZLIB::ZLIB          (zlib recipe)
  - PCRE2     → PCRE2::PCRE2-16     (pcre2 recipe)
  - Freetype  → Freetype::Freetype  (freetype recipe)
  - PNG       → PNG::PNG            (libpng recipe, via native PNGConfig.cmake)
  - WebP      → WebP::webp          (libwebp recipe, via generated WebPConfig.cmake)
  - SQLite3   → SQLite::SQLite3     (sqlite3 recipe)

Qt bundled (target-name complexity or missing recipe):
  - HarfBuzz, double-conversion, libjpeg, TIFF, brotli, bzip2
"""
from __future__ import annotations

import os
from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get


# Insertion order determines both download order and build order.
# qtbase MUST come first — all other modules depend on it.
_VERSION = "6.10.2"
_VERSION_SHORT = "6.10"
_BASE_URL = (
    f"https://download.qt.io/official_releases/qt/{_VERSION_SHORT}/{_VERSION}/submodules"
)

_MODULES: dict[str, str] = {
    "qtbase":         "aeb78d29291a2b5fd53cb55950f8f5065b4978c25fb1d77f627d695ab9adf21e",
    "qt5compat":      "3fa418f0fac02eb9efc5f762fbe25f20647b0ebb7fa92faf07e6de85044161c2",
    "qtimageformats": "8b8f9c718638081e7b3c000e7f31910140b1202a98e98df5d1b496fe6f639d67",
    "qtsvg":          "f07ff80f38caf235187200345392ca7479445ddf49a36c3694cd52a735dad6e1",
    "qttools":        "1e3d2c07c1fd76d2425c6eaeeaa62ffaff5f79210c4e1a5bc2a6a9db668d5b24",
    "qttranslations": "b3b3813bc9d76b545716dc8b6e659fa71b6e2bc14569e9fab6dab8b30650a644",
    "qtwebsockets":   "eccc751bea509ef656d20029693987a0fc03c58e21c38f1351480f3c8eb42ebd",
    "qthttpserver":   "26568d59bee258fd35297823d2f7839ef1337042a009b752769e688703fe4643",
}

# Qt FEATURE_ flags (-DFEATURE_xxx=ON/OFF at cmake configure time).
_FEATURES: dict[str, bool] = {
    "androiddeployqt":     False,
    "appstore_compliant":  False,
    "assistant":           False,
    "batch_test_support":  False,
    "brotli":              False,
    "calendarwidget":      False,
    "cborstreamreader":    True,
    "cborstreamwriter":    True,
    "clang":               False,
    "clangcpp":            False,
    "commandlineparser":   True,
    "concurrent":          True,
    "cups":                False,
    "dbus":                False,
    "designer":            False,
    "eglfs":               False,
    "framework":           False,
    "freetype":            True,
    "gif":                 False,
    "harfbuzz":            True,
    "ico":                 True,
    "imageformat_bmp":     True,
    "imageformat_jpeg":    True,
    "imageformat_png":     True,
    "imageformat_ppm":     True,
    "imageformat_xbm":     True,
    "imageformat_xpm":     True,
    "imageformatplugin":   True,
    "linguist":            False,
    "macdeployqt":         False,
    "opengles3":           False,
    "openssl":             False,
    "openssl_linked":      False,
    "pcre2":               True,
    "pdf":                 False,
    "permissions":         False,
    "pixeltool":           False,
    "printsupport":        False,
    "qdbus":               False,
    "qdoc":                False,
    "qmake":               False,
    "qtdiag":              False,
    "schannel":            False,
    "securetransport":     False,
    "separate_debug_info": False,
    "sql":                 False,
    "ssl":                 False,
    "testlib":             False,
    "tiff":                True,
    "webp":                True,
    "windeployqt":         False,
    "zstd":                False,
    # Disable CPU extensions that may not be present on all target machines.
    "arch_haswell":        False,
    "ssse3":               False,
    "sse4_1":              False,
}

# Qt INPUT_ variables — select "system" (our pre-built package) or "qt" (bundled).
_INPUTS: dict[str, str] = {
    "bzip2":            "qt",       # bundled — no recipe with right cmake target
    "doubleconversion": "qt",       # bundled — no recipe
    "freetype":         "system",   # Freetype::Freetype via freetype-config.cmake
    "harfbuzz":         "qt",       # bundled — no recipe with right cmake target
    "libjpeg":          "qt",       # bundled — libjpeg-turbo target name mismatch
    "libpng":           "system",   # PNG::PNG via native PNGConfig.cmake
    "opengl":           "no",
    "pcre":             "system",   # PCRE2::PCRE2-16 via generated PCRE2Config.cmake
    "sqlite":           "system",   # SQLite::SQLite3 via generated SQLite3Config.cmake
    "tiff":             "qt",       # bundled — avoid libtiff/LERC target complexity
    "webp":             "system",   # WebP::webp via generated WebPConfig.cmake
    "zlib":             "system",   # ZLIB::ZLIB via generated ZLIBConfig.cmake
}


class Recipe(RecipeBase):
    name = "qt6"
    version = _VERSION
    license = "LGPL-3.0"

    def requirements(self) -> list[str]:
        return [
            "zlib",
            "pcre2",
            "freetype",
            "libpng",
            "libwebp",
            "sqlite3",
        ]

    # ------------------------------------------------------------------
    # Source
    # ------------------------------------------------------------------

    def source(self) -> None:
        for module_name, sha256 in _MODULES.items():
            module_dir = os.path.join(self.source_folder, module_name)
            if Path(module_dir).is_dir() and any(Path(module_dir).iterdir()):
                print(f"[thirdparty] Qt6 {module_name} source already present — skipping")
                continue
            get(
                url=f"{_BASE_URL}/{module_name}-everywhere-src-{_VERSION}.tar.xz",
                dest=module_dir,
                sha256=sha256,
            )

    # ------------------------------------------------------------------
    # Generate — no-op; per-module toolchains are written inside build().
    # ------------------------------------------------------------------

    def generate(self) -> None:
        pass

    # ------------------------------------------------------------------
    # Build
    # ------------------------------------------------------------------

    def build(self) -> None:
        import glob as _glob

        # Find newest MSVC cl.exe so Ninja picks up the correct compiler.
        cl_paths = _glob.glob(
            r"C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe"
        )

        def _msvc_ver(p: str) -> tuple[int, ...]:
            try:
                return tuple(int(x) for x in p.split("MSVC\\")[1].split("\\")[0].split("."))
            except Exception:
                return (0,)

        cl_paths.sort(key=_msvc_ver)
        cl_exe = cl_paths[-1].replace("\\", "/") if cl_paths else None

        first_module = next(iter(_MODULES))
        orig_build = self.build_folder
        orig_source = self.source_folder
        orig_dep_paths = self.dep_package_paths

        for module_name in _MODULES:
            module_build = os.path.join(orig_build, module_name)
            module_src = os.path.join(orig_source, module_name)
            os.makedirs(module_build, exist_ok=True)

            # Skip modules that were already installed in a previous run.
            marker = os.path.join(module_build, ".qt_installed")
            if os.path.exists(marker):
                print(f"[thirdparty] Qt6 {module_name} already installed — skipping")
                continue

            # Temporarily redirect recipe paths to this module's directories.
            self.source_folder = module_src
            self.build_folder = module_build
            # Subsequent modules must find the Qt install prefix (qtbase etc.)
            # via CMAKE_PREFIX_PATH in addition to the other system deps.
            if module_name == first_module:
                self.dep_package_paths = list(orig_dep_paths)
            else:
                self.dep_package_paths = [self.package_folder] + list(orig_dep_paths)

            try:
                self._configure_module(module_name, cl_exe)
                cmake = CMake(self)
                cmake.configure()
                cmake.build()
                cmake.install()
                Path(marker).write_text("installed\n", encoding="utf-8")
            finally:
                self.source_folder = orig_source
                self.build_folder = orig_build
                self.dep_package_paths = orig_dep_paths

    def _configure_module(self, module_name: str, cl_exe: str | None) -> None:
        """Write the cmake toolchain + dep wrappers for one Qt module."""
        tc = CMakeToolchain(self)
        if cl_exe:
            tc.variables["CMAKE_C_COMPILER"] = cl_exe
            tc.variables["CMAKE_CXX_COMPILER"] = cl_exe

        tc.variables["BUILD_SHARED_LIBS"] = True
        tc.variables["QT_BUILD_TESTS"] = False
        tc.variables["QT_BUILD_EXAMPLES"] = False
        tc.variables["QT_BUILD_DOCS"] = False
        tc.variables["QT_EMBED_TOOLCHAIN_COMPILER"] = False
        tc.variables["QT_GENERATE_WRAPPER_SCRIPTS_FOR_ALL_HOSTS"] = False
        tc.variables["CMAKE_SUPPRESS_DEVELOPER_WARNINGS"] = True
        tc.variables["PCRE2_USE_STATIC_LIBS"] = True

        for feature, enabled in _FEATURES.items():
            tc.variables[f"FEATURE_{feature}"] = enabled
        for input_name, value in _INPUTS.items():
            tc.variables[f"INPUT_{input_name}"] = value

        tc.generate()

        # Generate cmake config wrappers for our system deps (ZLIB, PCRE2, …).
        deps = CMakeDeps(self)
        deps.generate()

        # Fix PCRE2Config.cmake for Qt compatibility.
        # Qt's FindWrapSystemPCRE2.cmake calls:
        #   find_package(PCRE2 COMPONENTS 16BIT)
        # and expects the target PCRE2::16BIT plus PCRE2_16BIT_FOUND=TRUE.
        # The standard CMakeDeps-generated config defines PCRE2::PCRE2-16 but
        # never sets the 16BIT component variable, causing find_package to fail.
        self._patch_pcre2_config()

        # CMakeDeps appends CMAKE_FIND_PACKAGE_PREFER_CONFIG=ON to the toolchain.
        # Qt's cmake works better in MODULE mode for packages it manages itself
        # (harfbuzz, doubleconversion, …); our _DIR hints ensure Config mode is
        # still used for the specific packages we wrap.
        toolchain_path = os.path.join(self.build_folder, "thirdparty_toolchain.cmake")
        with open(toolchain_path, "a", encoding="utf-8") as fh:
            fh.write('\nset(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF CACHE BOOL "" FORCE)\n')

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _patch_pcre2_config(self) -> None:
        """Overwrite the CMakeDeps-generated PCRE2Config.cmake with one that is
        compatible with Qt's FindWrapSystemPCRE2.cmake.

        Qt calls ``find_package(PCRE2 COMPONENTS 16BIT)`` and checks for the
        target ``PCRE2::16BIT`` plus ``PCRE2_16BIT_FOUND=TRUE``.  The standard
        CMakeDeps output creates ``PCRE2::PCRE2-16`` and never sets that
        component variable, so the find fails and Qt refuses to use the system
        PCRE2 library.
        """
        import glob as _glob

        pcre2_dep = self.dependencies.get("pcre2")
        if pcre2_dep is None:
            return

        lib_matches = _glob.glob(
            os.path.join(pcre2_dep.package_folder, "lib", "pcre2-16*.lib")
        )
        if not lib_matches:
            return

        lib_path = lib_matches[0].replace("\\", "/")
        inc_path = os.path.join(pcre2_dep.package_folder, "include").replace("\\", "/")

        cfg_dir = os.path.join(self.build_folder, "cmake_deps", "lib", "cmake", "PCRE2")
        cfg_file = os.path.join(cfg_dir, "PCRE2Config.cmake")
        with open(cfg_file, "w", encoding="utf-8") as f:
            f.write(
                "# Qt-compatible PCRE2 config — generated by qt6 recipe\n"
                'set(PCRE2_VERSION "10.44")\n'
                "set(PCRE2_FOUND TRUE)\n"
                "# Component variable required by find_package(PCRE2 COMPONENTS 16BIT)\n"
                "set(PCRE2_16BIT_FOUND TRUE)\n"
                f'set(PCRE2_INCLUDE_DIR "{inc_path}")\n'
                f'set(PCRE2_INCLUDE_DIRS "{inc_path}")\n'
                f'set(PCRE2_LIBRARY "{lib_path}")\n'
                f'set(PCRE2_LIBRARIES "{lib_path}")\n'
                "if(NOT TARGET PCRE2::16BIT)\n"
                "  add_library(PCRE2::16BIT STATIC IMPORTED GLOBAL)\n"
                "  set_target_properties(PCRE2::16BIT PROPERTIES\n"
                f'    IMPORTED_LOCATION "{lib_path}"\n'
                f'    INTERFACE_INCLUDE_DIRECTORIES "{inc_path}"\n'
                # Tell consumers to compile with PCRE2_STATIC so the PCRE2 headers
                # emit normal (non-dllimport) function declarations on Windows.
                '    INTERFACE_COMPILE_DEFINITIONS "PCRE2_STATIC"\n'
                "  )\n"
                "endif()\n"
                "# Alias for non-Qt consumers that expect PCRE2::PCRE2-16\n"
                "if(NOT TARGET PCRE2::PCRE2-16)\n"
                "  add_library(PCRE2::PCRE2-16 INTERFACE IMPORTED GLOBAL)\n"
                "  target_link_libraries(PCRE2::PCRE2-16 INTERFACE PCRE2::16BIT)\n"
                "endif()\n"
            )

    # ------------------------------------------------------------------
    # Package
    # ------------------------------------------------------------------

    def package(self) -> None:
        # All cmake.install() calls happen inside build(); by this point every
        # module has already been installed into package_folder.
        # Copy licenses from the qtbase source tree.
        qtbase_licenses = os.path.join(self.source_folder, "qtbase", "LICENSES")
        if os.path.isdir(qtbase_licenses):
            copy(
                "**/*",
                src=qtbase_licenses,
                dst=os.path.join(self.package_folder, "licenses"),
            )

    def package_info(self) -> None:
        # Expose the Qt6 cmake config installed by qtbase so downstream recipes
        # can find_package(Qt6 COMPONENTS Core Gui …) via our CMakeDeps wrapper.
        self.cpp_info.set_property("cmake_file_name", "Qt6")
        self.cpp_info.set_property("cmake_package_file", "lib/cmake/Qt6/Qt6Config.cmake")
