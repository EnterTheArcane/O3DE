# Qt6 builder — minimal Windows CMake build
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir


# Modules we want to build (everything else is OFF)
_DEFAULT_MODULES = [
    "qtbase",  # Core, Gui, Widgets, Network, OpenGL, etc.
    "qtsvg",  # SVG support
    "qtimageformats",  # Extra image formats
    "qttools",  # Designer, linguist, etc.
    "qtshadertools",  # Shader tools (required by qtquick3d)
    "qtdeclarative",  # QML / Quick
    "qt3d",  # 3D rendering
]

_ALL_MODULES = [
    "qtactiveqt",
    "qtcharts",
    "qtcoap",
    "qtconnectivity",
    "qtdatavis3d",
    "qtdeclarative",
    "qtdeviceutils",
    "qtdoc",
    "qtgrpc",
    "qthttpserver",
    "qtimageformats",
    "qtlanguageserver",
    "qtlocation",
    "qtlottie",
    "qtmqtt",
    "qtmultimedia",
    "qtnetworkauth",
    "qtopcua",
    "qtpositioning",
    "qtquick3d",
    "qtquick3dphysics",
    "qtquicktimeline",
    "qtremoteobjects",
    "qtscxml",
    "qtsensors",
    "qtserialbus",
    "qtserialport",
    "qtshadertools",
    "qtspeech",
    "qtsvg",
    "qttools",
    "qttranslations",
    "qtvirtualkeyboard",
    "qtwayland",
    "qtwebchannel",
    "qtwebengine",
    "qtwebsockets",
    "qtwebview",
    "qt3d",
    "qtactiveqt",
    "qtgraphs",
    "qt5compat",
    "qtquickeffectmaker",
]


class Recipe(RecipeBase):
    name = "qt6"
    version = "6.8.3"
    license = "LGPL-3.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_openssl": [True, False],
        "with_harfbuzz": [True, False],
        "with_freetype": [True, False],
        "with_libpng": [True, False],
        "with_zlib": [True, False],
        "with_pcre2": [True, False],
        "with_sqlite3": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_openssl": True,
        "with_harfbuzz": True,
        "with_freetype": True,
        "with_libpng": True,
        "with_zlib": True,
        "with_pcre2": True,
        "with_sqlite3": True,
    }

    def requirements(self) -> list[str]:
        deps = []
        if self.options.with_zlib:
            deps.append("zlib")
        if self.options.with_openssl:
            deps.append("openssl")
        if self.options.with_pcre2:
            deps.append("pcre2")
        if self.options.with_freetype:
            deps.append("freetype")
        if self.options.with_harfbuzz:
            deps.append("harfbuzz")
        if self.options.with_libpng:
            deps.append("libpng")
        if self.options.with_sqlite3:
            deps.append("sqlite3")
        return deps

    def source(self):
        from pathlib import Path

        src = Path(self.source_folder)
        if src.exists() and any(src.iterdir()):
            print("[thirdparty] Qt6 source already present — skipping download")
            return
        get(
            url="https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz",
            dest=self.source_folder,
            sha256="cdd3a69967208276bb01af7ace7dba0ba53e679f886a4cbe624225c60fb73f2c",
        )

    def generate(self):
        import glob

        # Qt6 requires a single-config generator; Ninja is the default.
        # Explicitly set MSVC cl.exe so LLVM clang (earlier on PATH) is not
        # picked up.  CMake's MSVC variable is only set for cl.exe / clang-cl,
        # and several Qt bundled 3rd-party libs (e.g. gumbo in qlitehtml) use
        # it to enable Windows compat paths.  The vcvarsall env sourced by the
        # cmake tool provides LIB/INCLUDE so kernel32.lib etc. are found.
        cl_paths = glob.glob(
            r"C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe"
        )

        def _msvc_ver(p):
            try:
                return tuple(
                    int(x) for x in p.split("MSVC\\")[1].split("\\")[0].split(".")
                )
            except Exception:
                return (0,)

        cl_paths.sort(key=_msvc_ver)
        cl_exe = cl_paths[-1].replace("\\", "/") if cl_paths else None

        tc = CMakeToolchain(self)
        if cl_exe:
            tc.variables["CMAKE_C_COMPILER"] = cl_exe
            tc.variables["CMAKE_CXX_COMPILER"] = cl_exe

        tc.variables["QT_BUILD_TESTS"] = False
        tc.variables["QT_BUILD_EXAMPLES"] = False
        tc.variables["QT_BUILD_DOCS"] = False
        tc.variables["FEATURE_static_runtime"] = False

        # Enable only the modules we want; disable everything else
        enabled = set(_DEFAULT_MODULES)
        for mod in _ALL_MODULES:
            tc.variables[f"BUILD_{mod}"] = mod in enabled

        # Use Qt's bundled PCRE2, freetype, harfbuzz, libpng, zlib to avoid
        # Qt-specific cmake wrapper target mismatches (Qt needs PCRE2::16BIT,
        # WrapSystemFreetype::WrapSystemFreetype, etc. which our packages don't provide).
        tc.variables["FEATURE_system_pcre2"] = False
        tc.variables["FEATURE_system_freetype"] = False
        tc.variables["FEATURE_system_harfbuzz"] = False
        tc.variables["FEATURE_system_libpng"] = False
        tc.variables["FEATURE_system_zlib"] = False
        tc.variables["FEATURE_system_sqlite"] = False

        # OpenSSL: let Qt auto-detect via CMAKE_PREFIX_PATH (set in toolchain)
        tc.variables["FEATURE_openssl"] = self.options.with_openssl
        tc.variables["FEATURE_openssl_linked"] = self.options.with_openssl

        # Disable features we definitely don't want
        tc.variables["FEATURE_sql_mysql"] = False
        tc.variables["FEATURE_sql_psql"] = False
        tc.variables["FEATURE_sql_odbc"] = False
        tc.variables["FEATURE_libudev"] = False
        tc.variables["FEATURE_dbus"] = False
        tc.variables["INPUT_opengl"] = "desktop"

        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            "LICENSES/*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
