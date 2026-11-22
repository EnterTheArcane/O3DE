#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from io import StringIO
import mmap
from pathlib import Path
import re
import shlex
import shutil
from subprocess import list2cmdline
import sys

from conan.tools.env import VirtualBuildEnv


# PE RSDS records and COFF LF_TYPESERVER2 records carry a GUID, age and PDB path.
PDB_REFERENCE = re.compile(
    rb"(?:RSDS|\x15\x15).{20}([A-Za-z]:[/\\][^\x00\r\n]{1,4096}?\.pdb)\x00",
    re.DOTALL | re.IGNORECASE,
)

MACHO_MAGIC = {
    bytes.fromhex(magic) for magic in ("feedface", "cefaedfe", "feedfacf", "cffaedfe")
}
MACHO_FAT_MAGIC = {bytes.fromhex(magic) for magic in ("cafebabe", "cafebabf")}


def install_pdbs(binary, roots):
    if binary.suffix.lower() not in (".lib", ".dll", ".exe", ".pyd"):
        return
    if not binary.stat().st_size:
        return
    with binary.open("rb") as stream, mmap.mmap(
        stream.fileno(), 0, access=mmap.ACCESS_READ
    ) as data:
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
