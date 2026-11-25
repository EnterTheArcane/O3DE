#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import os
import re
import shlex
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory

from conan.tools.files import rename, rmdir


def relocate(conanfile, framework, version, is_macho):
    def otool(option, path):
        output = StringIO()
        conanfile.run(
            f"/usr/bin/otool {option} {shlex.quote(str(path))}",
            stdout=output,
            quiet=True,
            env=None,
        )
        return output.getvalue()

    sdk = framework / "Versions" / version
    library = sdk / "Python"
    name = otool("-D", library).splitlines()[1]
    installed = Path(name).parents[2]
    if not installed.is_absolute() or installed.name != framework.name:
        raise ValueError(f"Unexpected Python framework install name {name}")
    prefix = f"{installed}/"

    for path in sorted(sdk.rglob("*")):
        if not is_macho(path):
            continue
        name_lines = otool("-D", path).splitlines()
        names = name_lines[1:]
        command = []
        if names and names[0].startswith(prefix):
            relative = path.relative_to(sdk)
            command.extend(("-id", f"@rpath/Python.framework/{relative}"))
        dependencies = otool("-L", path)
        for line in dependencies.splitlines()[1 + len(names) :]:
            dependency = line.strip().partition(" (compatibility version ")[0]
            if dependency.startswith(prefix):
                target = framework / Path(dependency).relative_to(installed)
                if not target.is_file():
                    raise FileNotFoundError(target)
                relative = os.path.relpath(target, path.parent)
                command.extend(("-change", dependency, f"@loader_path/{relative}"))
        load_commands = otool("-l", path)
        for rpath in re.findall(
            r"cmd LC_RPATH\s+cmdsize \d+\s+path (.+) \(offset \d+\)", load_commands
        ):
            if rpath == str(installed) or rpath.startswith(prefix):
                target = framework / Path(rpath).relative_to(installed)
                command.extend(
                    (
                        "-rpath",
                        rpath,
                        f"@loader_path/{os.path.relpath(target, path.parent)}",
                    )
                )
        if command:
            conanfile.run(
                shlex.join(["/usr/bin/install_name_tool", *command, str(path)]),
                env=None,
            )

    for script in (sdk / "bin").iterdir():
        if script.is_symlink() or not script.is_file():
            continue
        with script.open("rb") as stream:
            shebang = stream.readline(1024)
            if not shebang.startswith(f"#!{prefix}".encode()):
                continue
            target = framework / Path(shebang[2:].strip().decode()).relative_to(
                installed
            )
            interpreter = os.path.relpath(target, script.parent)
            if interpreter == f"python{version}":
                interpreter = "python3"
            source = stream.read()
        launcher = f"""#!/bin/sh
'''exec' "$(dirname "$0")/{interpreter}" "$0" "$@"
' '''
"""
        # Keep the original script body in its existing encoding.
        script.write_bytes(launcher.encode() + source)

    # Move the version's contents out before discarding the framework aliases.
    with TemporaryDirectory(dir=framework.parent) as staging:
        flat = Path(staging) / framework.name
        rename(conanfile, sdk, flat)
        rmdir(conanfile, framework)
        rename(conanfile, flat, framework)

    bindir = framework / "bin"
    (bindir / f"pip{version}").unlink(missing_ok=True)
    dsym = bindir / f"python{version}.dSYM"
    if dsym.is_dir():
        dwarf = dsym / "Contents/Resources/DWARF"
        rename(conanfile, dwarf / f"python{version}", dwarf / "python3")
        rename(conanfile, dsym, bindir / "python3.dSYM")

    headers = framework / "Headers"
    original = headers.resolve(strict=True)
    headers.unlink()
    rename(conanfile, original, headers)
    original.parent.rmdir()

    libdir = framework / "lib"
    stdlib = libdir / "python"
    rename(conanfile, libdir / f"python{version}", stdlib)
    config = stdlib / "config-darwin"
    rename(conanfile, stdlib / f"config-{version}-darwin", config)
    (libdir / f"libpython{version}.dylib").unlink()
    for extension in ("a", "dylib"):
        (config / f"libpython{version}.{extension}").unlink()
    # The curses extensions use the real dylib install names.
    for name in ("curses", "form", "menu", "ncurses", "panel"):
        alias = libdir / f"lib{name}.dylib"
        if alias.is_symlink():
            alias.unlink()
    docs = framework / "share/doc"
    rename(conanfile, docs / f"python{version}", docs / "python")
