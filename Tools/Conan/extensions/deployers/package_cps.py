#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import re
from pathlib import Path

from conan.errors import ConanException

CPS_DIRECTORY = Path("lib/cps")
COMPONENT_TYPES = {
    "application": "executable",
    "shared-library": "dylib",
    "static-library": "archive",
}


def package_path(root, value):
    path = root / value
    relative = path.relative_to(root)
    if ".." in relative.parts or not path.resolve().is_relative_to(root):
        raise ValueError(f"CPS path is outside the package: {value}")
    return f"@prefix@/{relative.as_posix()}" if relative.parts else "@prefix@"


def binary_component(root, kind, location, link_location=None):
    component = {"type": kind, "location": package_path(root, location)}
    if link_location:
        component["link_location"] = package_path(root, link_location)
    return component


def installed_components(dependency, root):
    # Some SDK recipes provide native CMake configs instead of Conan targets.
    # Describe their binary entry points from the installed package layout.
    name = dependency.ref.name
    sdk = {
        "physx": root / "release",
        "pyside6": root / "pyside6",
        "python": root / "python",
        "qt": root / "qt",
    }.get(name, root)
    libdirs = (
        sdk / "lib",
        sdk / "libs",
        *(root / path for path in dependency.cpp_info.libdirs),
    )
    bindirs = {sdk / "bin", *(root / path for path in dependency.cpp_info.bindirs)}
    if name == "python" and dependency.settings.os == "Windows":
        bindirs.add(sdk)
    files = {
        path
        for directory in (*libdirs, *bindirs)
        for path in directory.glob("*")
        if path.is_file()
    }
    libraries = {}
    executables = {}
    for path in sorted(files):
        if path.suffix in (".a", ".lib"):
            continue
        if path.suffix in (".dll", ".dylib") or ".so" in path.suffixes:
            # Prefer the linker symlink to any versioned filename.
            stem = (
                path.name.partition(".so")[0] if ".so" in path.suffixes else path.stem
            )
            stem = stem.removeprefix("lib")
            if path.suffix == ".dylib":
                stem = re.sub(r"(?:\.\d+)+$", "", stem)
            import_library = next(
                (
                    directory / f"{path.stem}.lib"
                    for directory in libdirs
                    if (directory / f"{path.stem}.lib").is_file()
                ),
                None,
            )
            kind = "module" if path.suffix == ".dll" and not import_library else "dylib"
            libraries.setdefault(
                stem, binary_component(root, kind, path, import_library)
            )
            continue
        if path.parent in bindirs and (
            path.suffix == ".exe" or path.stat().st_mode & 0o111
        ):
            executables[path.stem] = binary_component(root, "executable", path)
    for path in sorted(files):
        if path.suffix in (".a", ".lib"):
            libraries.setdefault(
                path.stem.removeprefix("lib"), binary_component(root, "archive", path)
            )
    for framework in sorted(sdk.glob("*.framework")) + sorted(
        (sdk / "lib").glob("*.framework")
    ):
        location = framework / framework.stem
        if location.is_file():
            kind = "archive" if dependency.package_type == "static-library" else "dylib"
            component = binary_component(root, kind, location)
            if (framework / "Headers").is_dir():
                component["includes"] = [package_path(root, framework / "Headers")]
            libraries[framework.stem] = component
    return libraries, executables


def cpp_component(dependency, root, info, name, extra_requires):
    component = {"type": "interface"}
    includes = [
        package_path(root, path) for path in info.includedirs if (root / path).is_dir()
    ]
    if includes:
        component["includes"] = includes
    if info.defines:
        definitions = {}
        for define in info.defines:
            key, operator, value = define.partition("=")
            definitions[key] = value if operator else None
        component["definitions"] = {"*": definitions}
    if info.cflags or info.cxxflags:
        component["compile_flags"] = {"c": info.cflags, "cpp": info.cxxflags}
    if info.sharedlinkflags or info.exelinkflags:
        component["link_flags"] = list(
            dict.fromkeys(info.sharedlinkflags + info.exelinkflags)
        )
    if info.system_libs:
        component["link_libraries"] = info.system_libs
    languages = info.languages or dependency.languages or []
    if "C++" in languages:
        component["link_languages"] = ["cpp"]
    for requirement in dict.fromkeys([*info.requires, *extra_requires]):
        target = (
            requirement.replace("::", ":") if "::" in requirement else f":{requirement}"
        )
        component.setdefault("requires", []).append(target)

    libraries = {}
    for library in info.libs or ([name] if info.location or info.exe else []):
        entry = info.clone()
        if info.libs:
            entry.libs = [library]
            if dependency.ref.name in ("gz-math", "gz-utils", "sdformat"):
                entry.libs = [
                    f"{library}{str(dependency.ref.version).partition('.')[0]}"
                ]
        try:
            entry.deduce_locations(dependency, library)
        except ConanException:
            if entry.libs == [library]:
                raise
            # Some builds retain an unversioned Gazebo library name.
            entry.libs = [library]
            entry.deduce_locations(dependency, library)
        if not entry.location:
            raise ConanException(
                f"Missing CPS location for {dependency.ref} component {name}"
            )
        libraries[library] = binary_component(
            root, COMPONENT_TYPES[str(entry.type)], entry.location, entry.link_location
        )
    if len(libraries) == 1:
        component = {**component, **next(iter(libraries.values()))}
        libraries.clear()
    return component, libraries


def package_manifest(dependency, folder, available):
    root = folder.resolve()
    name = dependency.ref.name
    info = dependency.cpp_info
    dependencies = {dep.ref.name: dep for dep in dependency.dependencies.host.values()}
    components = {}
    declared = info.components or {name: info}
    defaults = list(info.default_components or declared)
    extra_requires = []
    if not info.components:
        for requirement, dep in dependency.dependencies.direct_host.items():
            if (
                requirement.transitive_headers
                or requirement.transitive_libs
                or (
                    requirement.libs
                    and str(dependency.package_type) == "static-library"
                    and str(dep.package_type) != "build-scripts"
                    and dep.ref.name in available
                )
            ):
                extra_requires.append(f"{dep.ref.name}::{dep.ref.name}")
    for component_name, metadata in declared.items():
        component, libraries = cpp_component(
            dependency, root, metadata, component_name, extra_requires
        )
        if libraries:
            for library, binary in libraries.items():
                key = f"{component_name}.{library}"
                components[key] = binary
                component.setdefault("requires", []).append(f":{key}")
        components[component_name] = component

    if name not in components:
        components[name] = {
            "type": "interface",
            "requires": [f":{component}" for component in defaults],
        }
    if not info.components and not info.libs and not info.location:
        libraries, executables = installed_components(dependency, root)
        if name == "python":
            version = str(dependency.ref.version).rsplit(".", 1)[0]
            for key in (
                "Python",
                f"python{version}",
                f"python{version.replace('.', '')}",
            ):
                if key in libraries:
                    components[name] = {**components[name], **libraries.pop(key)}
                    break
            if dependency.settings.os == "Windows":
                debug = libraries.pop(f"python{version.replace('.', '')}_d", None)
                if debug and debug["type"] == components[name]["type"]:
                    components[name]["configurations"] = {
                        "debug": {
                            attr: debug[attr]
                            for attr in ("location", "link_location")
                            if attr in debug
                        }
                    }
        for binaries in (libraries, executables):
            for key, binary in sorted(binaries.items()):
                if key in components:
                    key = f"{key}.{binary['type']}"
                components[key] = binary

    manifest = {
        "cps_version": "0.13.0",
        "name": name,
        "version": str(dependency.ref.version),
        "cps_path": f"@prefix@/{CPS_DIRECTORY.as_posix()}",
        "default_components": defaults,
        "components": components,
    }
    requirements = {}
    for component in components.values():
        for reference in component.get("requires", []):
            package, _, target = reference.partition(":")
            if package:
                required = requirements.setdefault(
                    package,
                    {
                        "version": str(dependencies[package].ref.version),
                        "components": [],
                    },
                )
                if target not in required["components"]:
                    required["components"].append(target)
    if requirements:
        manifest["requires"] = requirements
    for field in ("license", "description"):
        value = getattr(dependency, field)
        if isinstance(value, str) and value:
            manifest[field] = value
    return manifest
