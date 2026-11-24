#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import mmap
import os
import re
import shlex
import shutil
import sys
from io import StringIO
from pathlib import Path
from subprocess import list2cmdline

from conan.tools.env import VirtualBuildEnv
from conan.tools.files import save

# PE RSDS records and COFF LF_TYPESERVER2 records carry a GUID, age and PDB path.
PDB_REFERENCE = re.compile(
    rb"(?:RSDS|\x15\x15).{20}([A-Za-z]:[/\\][^\x00\r\n]{1,4096}?\.pdb)\x00",
    re.DOTALL | re.IGNORECASE,
)

MACHO_MAGIC = {
    bytes.fromhex(magic) for magic in ("feedface", "cefaedfe", "feedfacf", "cffaedfe")
}
MACHO_FAT_MAGIC = {bytes.fromhex(magic) for magic in ("cafebabe", "cafebabf")}


def pre_generate(conanfile):
    if (
        not conanfile.conf.get("user.package:debug_symbols")
        or not conanfile.source_folder
    ):
        return
    compiler = conanfile.settings.get_safe("compiler")
    if compiler not in ("clang", "apple-clang", "gcc", "emcc", "msvc"):
        return
    target_os = conanfile.settings.get_safe("os")
    windows = target_os == "Windows"
    root = "C:/__debug__" if windows else "/__debug__"
    identity = f"{conanfile.name}/{conanfile.version}"
    paths = {}
    if target_os == "Emscripten":
        # Emscripten generates its sysroot outside the SDK package.
        env = VirtualBuildEnv(conanfile, auto_generate=True).vars()
        cache = (
            env.get("EM_CACHE")
            or os.getenv("EM_CACHE")
            or conanfile.conf.get("user.toolchain:emscripten_cache")
        )
        if cache:
            paths[Path(cache).expanduser().resolve().as_posix()] = f"{root}/emscripten"
    for dependency in conanfile.dependencies.values():
        if dependency.package_folder:
            paths[Path(dependency.package_folder).resolve().as_posix()] = (
                f"{root}/packages/{dependency.ref.name}/{dependency.ref.version}"
            )
    if conanfile.build_folder:
        paths[Path(conanfile.build_folder).resolve().as_posix()] = (
            f"{root}/build/{identity}"
        )
    paths[Path(conanfile.source_folder).resolve().as_posix()] = (
        f"{root}/source/{identity}"
    )
    # The source can be nested in the build tree. Give the narrower prefix priority.
    mappings = sorted(paths.items(), key=lambda mapping: len(mapping[0]))
    if compiler == "msvc":
        flags = ["-experimental:deterministic"]
        flags.extend(f"-pathmap:{path}={mapped}" for path, mapped in mappings)
    else:
        flags = [f"-ffile-prefix-map={path}={mapped}" for path, mapped in mappings]
        if windows and compiler == "clang":
            flags.append("-gno-codeview-command-line")
            flags = [f"-clang:{flag}" for flag in flags]

    conanfile.conf.define("user.package:debug_path_flags", flags)
    if conanfile.settings_build.get_safe("os") == "Windows" and not conanfile.win_bash:
        # CMakeToolchain writes these flags inside a double-quoted CMake string.
        toolchain_flags = [list2cmdline([flag]).replace('"', r"\"") for flag in flags]
    else:
        toolchain_flags = [shlex.quote(flag) for flag in flags]
    for language in ("c", "cxx"):
        conanfile.conf.append(f"tools.build:{language}flags", toolchain_flags)
    build_type = conanfile.settings.get_safe("build_type")
    if build_type:
        # Conan scopes its CMake flags to the selected build type with multi-config generators.
        options = "\n".join(
            f"    [==[$<$<AND:$<NOT:$<CONFIG:{build_type}>>,$<COMPILE_LANGUAGE:C,CXX>>:{flag}>]==]"
            for flag in flags
        )
        cmake = Path(conanfile.generators_folder) / "debug_paths.cmake"
        save(
            conanfile,
            cmake,
            "include_guard(GLOBAL)\n"
            "get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)\n"
            "if(multi_config)\n"
            f"  add_compile_options(\n{options}\n  )\n"
            "endif()\n",
        )
        conanfile.conf.append(
            "tools.cmake.cmaketoolchain:user_toolchain", cmake.as_posix()
        )
    if target_os in ("Macos", "iOS"):
        for variable in ("OBJCFLAGS", "OBJCXXFLAGS"):
            conanfile.buildenv.append(variable, ["-g", *toolchain_flags])


def install_pdbs(binary, roots):
    if binary.suffix.lower() not in (".lib", ".dll", ".exe", ".pyd"):
        return
    if not binary.stat().st_size:
        return
    with (
        binary.open("rb") as stream,
        mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data,
    ):
        paths = {match.group(1) for match in PDB_REFERENCE.finditer(data)}
    for path in paths:
        pdb = Path(path.decode("utf-8", errors="replace")).resolve()
        if not any(pdb.is_relative_to(root) for root in roots) or not pdb.is_file():
            continue
        destination = binary.with_name(pdb.name)
        if not destination.exists():
            shutil.copyfile(pdb, destination)


def separate_elf_symbols(conanfile, binary, readelf, objcopy):
    command_line = shlex.join
    if sys.platform == "win32" and not conanfile.win_bash:
        command_line = list2cmdline
    sections = StringIO()
    conanfile.run(
        command_line([readelf, "-SW", binary.as_posix()]),
        stdout=sections,
        quiet=True,
    )
    table = sections.getvalue()
    if ".gnu_debuglink" in table or not re.search(r"\s\.(?:z?debug)_info\s", table):
        return
    debug = binary.parent / ".debug" / f"{binary.name}.debug"
    debug.parent.mkdir(exist_ok=True)
    conanfile.run(
        command_line(
            [objcopy, "--only-keep-debug", binary.as_posix(), debug.as_posix()]
        ),
        quiet=True,
    )
    # The OpenSSL FIPS configuration authenticates the original module bytes.
    if conanfile.name == "openssl" and binary.name == "fips.so":
        return
    conanfile.run(
        command_line(
            [
                objcopy,
                "--strip-debug",
                f"--add-gnu-debuglink={debug.as_posix()}",
                binary.as_posix(),
            ]
        ),
        quiet=True,
    )


def install_dsym(conanfile, binary, package):
    debug_map = StringIO()
    conanfile.run(
        shlex.join(["xcrun", "dsymutil", "--dump-debug-map", str(binary)]),
        stdout=debug_map,
        quiet=True,
    )
    if "filename:" not in debug_map.getvalue():
        return
    bundle = package / "symbols" / binary.relative_to(package)
    bundle = bundle.with_name(f"{bundle.name}.dSYM")
    bundle.parent.mkdir(parents=True, exist_ok=True)
    conanfile.run(
        shlex.join(["xcrun", "dsymutil", str(binary), "-o", str(bundle)]), quiet=True
    )


def install_wasm_debug(binary, build):
    if binary.suffix != ".wasm" or binary.name.endswith(".debug.wasm"):
        return
    name = f"{binary.name}.debug.wasm"
    target = binary.with_name(name)
    if not target.exists():
        candidates = list(build.rglob(name))
        if len(candidates) == 1:
            shutil.copyfile(candidates[0], target)


def post_package(conanfile):
    if not conanfile.conf.get("user.package:debug_symbols"):
        return
    os_name = conanfile.settings.get_safe("os")
    package = Path(conanfile.package_folder)
    build = Path(conanfile.build_folder).resolve()
    roots = (build, Path(conanfile.source_folder).resolve())
    cross_linux = os_name == "Linux" and conanfile.settings.arch == "armv8"
    if os_name == "Android":
        env = VirtualBuildEnv(conanfile, auto_generate=True).vars()
        readelf = env.get("READELF")
        objcopy = env.get("OBJCOPY")
        if not readelf or not objcopy:
            raise ValueError("Android debug symbols require NDK READELF and OBJCOPY")
    elif os_name == "Linux":
        prefix = "aarch64-linux-gnu-" if cross_linux else ""
        readelf, objcopy = f"{prefix}readelf", f"{prefix}objcopy"

    for binary in list(package.rglob("*")):
        relative = binary.relative_to(package)
        if (
            binary.is_symlink()
            or not binary.is_file()
            or any(
                part == ".debug" or part.endswith(".dSYM") for part in relative.parts
            )
        ):
            continue
        if os_name == "Windows":
            install_pdbs(binary, roots)
            continue
        if os_name == "Emscripten":
            install_wasm_debug(binary, build)
            continue
        with binary.open("rb") as stream:
            header = stream.read(20)
        if (
            os_name in ("Linux", "Android")
            and len(header) >= 18
            and header.startswith(b"\x7fELF")
        ):
            endian = "little" if header[5] == 1 else "big"
            if int.from_bytes(header[16:18], endian) in (2, 3):
                separate_elf_symbols(conanfile, binary, readelf, objcopy)
        if os_name in ("Macos", "iOS"):
            if header[:4] in MACHO_MAGIC:
                endian = "little" if header[0] in (0xCE, 0xCF) else "big"
                if int.from_bytes(header[12:16], endian) in (2, 6, 8):
                    install_dsym(conanfile, binary, package)
            elif header[:4] in MACHO_FAT_MAGIC and header[4:7] == b"\0\0\0":
                install_dsym(conanfile, binary, package)
