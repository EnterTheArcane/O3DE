#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import os
from io import StringIO
from pathlib import Path
import re
import shlex


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

    versions = framework / "Versions"
    current = versions / "Current"
    if not current.exists():
        current.symlink_to(version)
    sdk = versions / version
    library = sdk / "Python"
    name = otool("-D", library).splitlines()[1]
    installed = Path(name).parents[2]
    if not installed.is_absolute() or installed.name != framework.name:
        raise ValueError(f"Unexpected Python framework install name {name}")
    prefix = f"{installed}/"

    for path in sorted(framework.rglob("*")):
        if not is_macho(path):
            continue
        name_lines = otool("-D", path).splitlines()
        names = name_lines[1:]
        command = []
        if names and names[0].startswith(prefix):
            relative = path.relative_to(sdk)
            command.extend(
                ("-id", f"@rpath/Python.framework/Versions/Current/{relative}")
            )
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
            source = stream.read()
        launcher = f"""#!/bin/sh
'''exec' "$(dirname "$0")/{interpreter}" "$0" "$@"
' '''
"""
        # Keep the original script body in its existing encoding.
        script.write_bytes(launcher.encode() + source)
