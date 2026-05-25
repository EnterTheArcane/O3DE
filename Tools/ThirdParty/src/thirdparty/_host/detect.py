import os
import platform
import re
import subprocess
import sys
from multiprocessing import cpu_count

from thirdparty._conan.internal.default_settings import default_settings_yml
from thirdparty._conan.internal.model.settings import Settings
from thirdparty._conan.internal.model.conf import Conf


def _detect_msvc_version():
    vswhere = os.path.join(
        os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
        "Microsoft Visual Studio", "Installer", "vswhere.exe",
    )
    if not os.path.exists(vswhere):
        return None
    try:
        install_path = subprocess.check_output(
            [vswhere, "-latest",
             "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
             "-property", "installationPath"],
            text=True, stderr=subprocess.DEVNULL,
        ).strip()
        if not install_path:
            return None
        ver_file = os.path.join(
            install_path, "VC", "Auxiliary", "Build",
            "Microsoft.VCToolsVersion.default.txt",
        )
        if not os.path.exists(ver_file):
            return None
        with open(ver_file) as f:
            full_ver = f.read().strip()
        parts = full_ver.split(".")
        minor = int(parts[1])
        # VCTools 14.Nx.x → conan msvc "19N" (14.3x=193, 14.4x=194, …)
        return str(190 + minor // 10)
    except Exception:
        return None


def _detect_apple_clang_version():
    for cmd in (["xcrun", "clang", "--version"], ["clang", "--version"]):
        try:
            out = subprocess.check_output(cmd, text=True, stderr=subprocess.STDOUT)
            m = re.search(r"Apple clang version (\d+)", out, re.IGNORECASE)
            if m:
                return m.group(1)
        except Exception:
            continue
    return None


def _detect_linux_compiler():
    for exe in ("gcc", "clang", "cc"):
        try:
            out = subprocess.check_output(
                [exe, "--version"], text=True, stderr=subprocess.STDOUT,
            )
            if "clang" in out.lower():
                m = re.search(r"clang version (\d+)", out)
                if m:
                    return "clang", m.group(1)
            else:
                m = re.search(r"(\d+)\.\d+", out.splitlines()[0])
                if m:
                    return "gcc", m.group(1)
        except Exception:
            continue
    return None, None


def detect_settings(build_type="Release"):
    settings = Settings.loads(default_settings_yml)

    the_os = platform.system()
    if the_os == "Darwin":
        the_os = "Macos"

    machine = platform.machine().lower()
    if "arm64" in machine or "aarch64" in machine:
        arch = "armv8"
    else:
        arch = "x86_64"

    settings.update_values([
        ("os", the_os),
        ("arch", arch),
        ("build_type", build_type),
    ], raise_undefined=False)

    if the_os == "Windows":
        msvc_ver = _detect_msvc_version()
        if msvc_ver:
            settings.update_values([
                ("compiler", "msvc"),
                ("compiler.version", msvc_ver),
                ("compiler.runtime", "dynamic"),
                ("compiler.cppstd", "17"),
            ], raise_undefined=False)
    elif the_os == "Macos":
        ver = _detect_apple_clang_version()
        if ver:
            settings.update_values([
                ("compiler", "apple-clang"),
                ("compiler.version", ver + ".0"),
                ("compiler.libcxx", "libc++"),
                ("compiler.cppstd", "17"),
            ], raise_undefined=False)
        # Set os.version so that CMakeToolchain emits CMAKE_OSX_DEPLOYMENT_TARGET.
        # platform.mac_ver() returns e.g. "15.4.0" or "26.5.0"; strip the patch component.
        _raw_ver = platform.mac_ver()[0]  # e.g. "26.5.0"
        if _raw_ver:
            _parts = _raw_ver.split(".")
            # Use major.minor (e.g. "26.5"), falling back to just major if needed.
            _os_version = ".".join(_parts[:2]) if len(_parts) >= 2 else _parts[0]
            settings.update_values([("os.version", _os_version)], raise_undefined=False)
    else:
        compiler, ver = _detect_linux_compiler()
        if compiler == "gcc":
            settings.update_values([
                ("compiler", "gcc"),
                ("compiler.version", ver),
                ("compiler.libcxx", "libstdc++11"),
                ("compiler.cppstd", "17"),
            ], raise_undefined=False)
        elif compiler == "clang":
            settings.update_values([
                ("compiler", "clang"),
                ("compiler.version", ver),
                ("compiler.libcxx", "libc++"),
                ("compiler.cppstd", "17"),
            ], raise_undefined=False)

    return settings


def make_conf(jobs=None):
    conf = Conf()
    conf.define("tools.cmake.cmaketoolchain:generator", "Ninja")
    conf.define("tools.meson.mesontoolchain:backend", "ninja")
    conf.define("tools.build:jobs", jobs if jobs is not None else cpu_count())
    conf.define("tools.cmake:configure_args", ["-DCMAKE_POLICY_VERSION_MINIMUM=3.5"])
    conf.define("user.openssl:windows_use_jom", True)
    return conf
