#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import gzip
import hashlib
import io
import json
import lzma
import os
import shutil
import sys
import tarfile
from pathlib import Path
from tempfile import TemporaryDirectory

from package_cps import CPS_DIRECTORY, package_manifest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from hooks.hook_sbom import SBOM_FILENAME, format_sbom, package_sbom

CONAN_METADATA = {"conaninfo.txt", "conanmanifest.txt"}
LEGACY_LINK_MANIFESTS = {".o3de-symlinks.json", ".package-symlinks.json"}
MSVC_RUNTIME_DLLS = {
    "concrt140.dll",
    "msvcp140.dll",
    "msvcp140_1.dll",
    "msvcp140_2.dll",
    "msvcp140_atomic_wait.dll",
    "msvcp140_codecvt_ids.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll",
}


def archive_package(
    folder,
    archive,
    licenses=None,
    *,
    preserve_links=False,
    cps=None,
    sbom=None,
    standalone=False,
    omit_msvc_runtime=False,
):
    root = folder.resolve()
    checksums = []

    def add_bytes(name, data):
        info = tarfile.TarInfo(name)
        info.size = len(data)
        tar.addfile(info, io.BytesIO(data))

    def add(source, relative, parents, boundary):
        if relative.name.casefold() in CONAN_METADATA:
            return
        if omit_msvc_runtime and relative.name.casefold() in MSVC_RUNTIME_DLLS:
            return
        if sbom is not None and relative == Path(SBOM_FILENAME):
            return
        if (
            not preserve_links
            and source.suffix == ".pyc"
            and source.parent.name == "__pycache__"
        ):
            # Imports during a build can leave bytecode with absolute source paths.
            module, tag, _ = source.name.rpartition(".cpython-")
            if tag and (source.parent.parent / f"{module}.py").is_file():
                return
        resolved = source.resolve(strict=True)
        if not resolved.is_relative_to(boundary) or resolved in parents:
            raise ValueError(f"Invalid package link: {source}")
        if not (resolved.is_dir() or resolved.is_file()):
            raise ValueError(f"Unsupported package entry: {source}")
        name = relative.as_posix()
        if any(part.startswith(".wh.") for part in relative.parts) or any(
            char in name for char in "\\:\r\n"
        ):
            raise ValueError(f"Unsupported archive path: {relative}")
        if name in LEGACY_LINK_MANIFESTS:
            raise ValueError("Reserved archive metadata path")
        if preserve_links and source.is_symlink():
            target = os.readlink(source)
            if Path(target).is_absolute():
                raise ValueError(f"Expected a relative link: {source}")
            destination = Path(os.path.normpath(source.parent / target))
            if not destination.is_relative_to(boundary) or any(
                char in target for char in "\\:\r\n"
            ):
                raise ValueError(f"Invalid archive link target: {source}")
            info = tarfile.TarInfo(name)
            info.type = tarfile.SYMTYPE
            info.linkname = target
            info.mode = 0o777
            tar.addfile(info)
            return
        info = tar.gettarinfo(str(resolved), name)
        info.type = tarfile.DIRTYPE if resolved.is_dir() else tarfile.REGTYPE
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = ""
        info.mode = 0o755 if info.isdir() or info.mode & 0o111 else 0o644
        if info.isdir():
            tar.addfile(info)
            for child in sorted(resolved.iterdir()):
                add(child, relative / child.name, parents | {resolved}, boundary)
        else:
            info.linkname = ""
            info.size = resolved.stat().st_size
            with resolved.open("rb") as stream:
                if standalone:
                    digest = hashlib.file_digest(stream, "sha256").hexdigest()
                    checksums.append(f"{digest}  {name}\n")
                    stream.seek(0)
                tar.addfile(info, stream)

    with archive.open("wb") as stream:
        if standalone:
            compressed = lzma.LZMAFile(stream, "wb", preset=3)
        else:
            compressed = gzip.GzipFile(mode="wb", fileobj=stream, mtime=0, filename="")
        with (
            compressed,
            tarfile.open(fileobj=compressed, mode="w|", dereference=True) as tar,
        ):
            for child in sorted(root.iterdir()):
                add(child, Path(child.name), {root}, root)
            if sbom is not None:
                name = SBOM_FILENAME
                data = sbom.encode("utf-8")
                add_bytes(name, data)
                if standalone:
                    checksums.append(f"{hashlib.sha256(data).hexdigest()}  {name}\n")
            if licenses:
                add(
                    licenses,
                    Path("licenses/dependencies"),
                    set(),
                    licenses.resolve(),
                )
            if cps:
                manifest = CPS_DIRECTORY / f"{cps['name']}.cps"
                existing = root / manifest
                if existing.is_dir():
                    raise ValueError(f"CPS manifest path is a directory: {existing}")
                if not existing.is_file():
                    data = (json.dumps(cps, indent=2, sort_keys=True) + "\n").encode()
                    name = manifest.as_posix()
                    add_bytes(name, data)
                    if standalone:
                        checksums.append(
                            f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                        )
    if standalone:
        return "".join(checksums).encode()


def deploy_packages(graph, output_folder, *, standalone=False):
    output = Path(output_folder)
    output.mkdir(parents=True, exist_ok=True)
    packages = {}
    consumer = graph.root.conanfile
    host_nodes = {
        node.conanfile.pref: node.conanfile
        for node in graph.nodes
        if node.context == "host" and node.ref
    }
    properties = getattr(consumer, "extension_properties", {})
    engine_provided = set(properties.get("engine_provided", ()))
    bundle_msvc_runtime = properties.get("bundle_msvc_runtime", ())
    dependencies = {
        dependency.ref.name: dependency
        for dependency in consumer.dependencies.direct_host.values()
    }
    component_dependencies = {
        reference.partition("::")[0]
        for dependency in dependencies.values()
        for info in (dependency.cpp_info, *dependency.cpp_info.components.values())
        for reference in info.requires
        if "::" in reference
    }
    wasm = consumer.settings.os == "Emscripten"
    for requirement, dependency in consumer.dependencies.host.items():
        name = dependency.ref.name
        if (
            requirement.headers
            or (wasm and requirement.libs)
            or name in component_dependencies
        ):
            dependencies.setdefault(name, dependency)

    imaging = ("openimageio", "opencolorio")
    for name in imaging:
        package = dependencies.get(name)
        if package and package.settings.os == "Emscripten":
            # Consumers link the dependencies of static imaging SDKs themselves.
            for requirement, dependency in package.dependencies.host.items():
                if requirement.headers or requirement.libs:
                    dependencies.setdefault(dependency.ref.name, dependency)
    available = dependencies.keys() | engine_provided
    with TemporaryDirectory(dir=output) as directory:
        staging = Path(directory)
        licenses = staging / "licenses"
        # Shared imaging libraries contain statically linked and header-only
        # dependencies that are absent from the public CMake dependency graph.
        pending = [
            node
            for node in graph.nodes
            if node.context == "host" and node.ref and node.ref.name in imaging
        ]
        collected = set()
        while pending:
            node = pending.pop()
            name = node.ref.name
            if name in collected:
                continue
            collected.add(name)
            pending.extend(
                edge.dst for edge in node.edges if edge.dst.context == "host"
            )
            if node.conanfile.package_folder:
                source = Path(node.conanfile.package_folder) / "licenses"
                if source.is_dir():
                    shutil.copytree(source, licenses / name, symlinks=True)

        for dependency in dependencies.values():
            name = dependency.ref.name
            if (
                name in engine_provided
                or (dependency.conan_data or {}).get("publish") is False
            ):
                continue
            if not dependency.package_folder or not dependency.pref:
                raise ValueError(f"Expected a built host artifact for {dependency.ref}")
            folder = Path(dependency.package_folder)
            cps = package_manifest(dependency, folder, available)
            cached_sbom = folder / SBOM_FILENAME
            if cached_sbom.is_file():
                sbom = format_sbom(json.loads(cached_sbom.read_bytes()))
            else:
                sbom = package_sbom(host_nodes[dependency.pref])
            notices = None
            if name in imaging:
                notices = licenses
            platform = dependency.settings.get_safe("os") or consumer.settings.get_safe(
                "os"
            )
            if platform in ("Macos", "iOS"):
                signed = staging / name
                shutil.copytree(folder, signed, symlinks=True)
                entitlements = None
                if platform == "Macos" and name == "python":
                    entitlements = (
                        Path(__file__).resolve().parents[2]
                        / "recipes/python/all/platform/macos/python_runtime.entitlements"
                    )
                consumer.python_requires["apple-signing"].module.sign_tree(
                    consumer, signed, ios=platform == "iOS", entitlements=entitlements
                )
                folder = signed
            archive = f"{name}.tar.gz"
            target = staging / "package.tar.xz" if standalone else output / archive
            content = archive_package(
                folder,
                target,
                notices,
                preserve_links=platform in ("Macos", "iOS"),
                cps=cps,
                sbom=sbom,
                standalone=standalone,
                omit_msvc_runtime=platform == "Windows"
                and name not in bundle_msvc_runtime,
            )
            if standalone:
                with target.open("rb") as stream:
                    digest = hashlib.file_digest(stream, "sha256").hexdigest()
                archive = f"{name}-{dependency.ref.version}-{digest}.tar.xz"
                target.replace(output / archive)
                for suffix, data in (
                    (".SHA256SUMS", f"{digest}  {archive}\n".encode()),
                    (".content.SHA256SUMS", content),
                ):
                    sidecar = staging / f"{archive}{suffix}"
                    sidecar.write_bytes(data)
                    sidecar.replace(output / sidecar.name)
            packages[name] = {
                "identity": dependency.pref.repr_notime(),
                "archive": archive,
            }
        index = staging / "packages.json"
        index.write_text(
            json.dumps(packages, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        index.replace(output / index.name)


def deploy(graph, output_folder, **_):
    deploy_packages(graph, Path(output_folder) / "archives", standalone=True)
