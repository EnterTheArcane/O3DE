#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

"""
Sign and verify all Mach-O code below a package directory.

The default identity (``-``) creates ad-hoc signatures, which are required for locally built code on Apple silicon.
Set APPLE_CODE_SIGN_IDENTITY to use a signing identity from the keychain.
"""

from __future__ import annotations

import os
from io import StringIO
import pathlib
import re
import shlex
import sys

from conan import ConanFile


class AppleSigning(ConanFile):
    name = "apple-signing"
    version = "1.0"
    package_type = "python-require"


MACHO_MAGICS = {
    b"\xfe\xed\xfa\xce",  # MH_MAGIC
    b"\xce\xfa\xed\xfe",  # MH_CIGAM
    b"\xfe\xed\xfa\xcf",  # MH_MAGIC_64
    b"\xcf\xfa\xed\xfe",  # MH_CIGAM_64
    b"\xca\xfe\xba\xbe",  # FAT_MAGIC
    b"\xbe\xba\xfe\xca",  # FAT_CIGAM
    b"\xca\xfe\xba\xbf",  # FAT_MAGIC_64
    b"\xbf\xba\xfe\xca",  # FAT_CIGAM_64
}


def is_macho(path: pathlib.Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    try:
        with path.open("rb") as source:
            return source.read(4) in MACHO_MAGICS
    except OSError:
        return False


def is_executable_macho(conanfile: ConanFile, path: pathlib.Path) -> bool:
    output = StringIO()
    conanfile.run(
        f"/usr/bin/otool -hv {shlex.quote(str(path))}",
        stdout=output,
        quiet=True,
        env=None,
    )
    return re.search(r"\bEXECUTE\b", output.getvalue()) is not None


def code_paths(root: pathlib.Path) -> tuple[list[pathlib.Path], list[pathlib.Path]]:
    paths = [
        path
        for path in root.rglob("*")
        if all(not part.endswith(".dSYM") for part in path.relative_to(root).parts)
        and not path.is_symlink()
    ]
    macho_files = sorted(
        (path for path in paths if is_macho(path)),
        key=lambda path: (len(path.parts), str(path)),
        reverse=True,
    )
    bundles = sorted(
        (
            path
            for path in paths
            if path.is_dir() and path.suffix in {".app", ".bundle", ".framework"}
        ),
        key=lambda path: (len(path.parts), str(path)),
        reverse=True,
    )
    if root.is_dir() and root.suffix in {".app", ".bundle", ".framework"}:
        bundles.append(root)
    return macho_files, bundles


def sign_path(
    conanfile: ConanFile,
    path: pathlib.Path,
    identity: str,
    entitlements: pathlib.Path | None,
    executable: bool,
    ios: bool,
):
    command = ["/usr/bin/codesign", "--force", "--sign", identity]
    keychain = os.environ.get("APPLE_CODE_SIGN_KEYCHAIN")
    if identity != "-" and keychain:
        command.extend(["--keychain", keychain])
    if identity != "-" and not ios:
        command.extend(["--timestamp", "--options", "runtime"])
        if executable and entitlements:
            command.extend(["--entitlements", str(entitlements)])
    command.append(str(path))
    conanfile.run(shlex.join(command), env=None)


def verify_path(conanfile: ConanFile, path: pathlib.Path, deep: bool = False):
    command = ["/usr/bin/codesign", "--verify", "--strict", "--verbose=2"]
    if deep:
        command.append("--deep")
    command.append(str(path))
    conanfile.run(shlex.join(command), env=None)


def sign_tree(
    conanfile: ConanFile,
    root: pathlib.Path,
    *,
    identity: str | None = None,
    entitlements: pathlib.Path | None = None,
    ios: bool = False,
) -> None:
    if sys.platform != "darwin":
        raise RuntimeError("Apple code signing must run on macOS")

    if identity is None:
        identity = os.environ.get("APPLE_CODE_SIGN_IDENTITY") or "-"
    root = root.resolve(strict=True)
    if entitlements is not None:
        entitlements = entitlements.resolve(strict=True)
    macho_files, bundles = code_paths(root)
    for path in macho_files:
        executable = is_executable_macho(conanfile, path)
        sign_path(conanfile, path, identity, entitlements, executable, ios)
    for path in bundles:
        sign_path(conanfile, path, identity, entitlements, path.suffix == ".app", ios)

    for path in macho_files:
        verify_path(conanfile, path)
    for path in bundles:
        verify_path(conanfile, path, deep=True)

    conanfile.output.info(
        f"Signed and verified {len(macho_files)} Mach-O files below {root}"
    )
