#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import argparse
import gzip
import hashlib
import json
import os
import re
import shutil
from functools import partial
from pathlib import Path
from time import sleep
from urllib.parse import urlsplit

from build import PROFILES

LAYER = "application/vnd.oci.image.layer.v1.tar+gzip"
IMAGE_CONFIG = "application/vnd.oci.image.config.v1+json"
IMAGE_MANIFEST = "application/vnd.oci.image.manifest.v1+json"
IMAGE_INDEX = "application/vnd.oci.image.index.v1+json"
VARIANT_KEYS = ("platform.os", "platform.architecture", "build.type")


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def blob_path(layout, digest):
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        raise ValueError(f"Invalid OCI digest: {digest}")
    return layout / "blobs" / "sha256" / digest.removeprefix("sha256:")


def layout_json_blob(layout, media_type, data):
    payload = json.dumps(data, separators=(",", ":")).encode("utf-8")
    digest = f"sha256:{hashlib.sha256(payload).hexdigest()}"
    path = blob_path(layout, digest)
    if not path.exists() or path.read_bytes() != payload:
        path.write_bytes(payload)
    return {"mediaType": media_type, "digest": digest, "size": len(payload)}


def catalog_references(profile, output, registry, *, push):
    packages = json.loads((output / f"{profile}.json").read_text(encoding="utf-8"))[
        profile
    ]
    references = set()
    for config, entries in packages.items():
        if push:
            metadata = json.loads(
                (output / config / "packages" / "packages.json").read_text(
                    encoding="utf-8"
                )
            )
        for name, reference in entries.items():
            repo, _, digest = reference.rpartition("@")
            if not repo.startswith(f"{registry}/") or not re.fullmatch(
                r"sha256:[0-9a-f]{64}", digest
            ):
                raise ValueError(f"Unexpected destination: {reference}")
            version = None
            if push:
                identity = metadata[name]["identity"]
                recipe = re.split(r"[#@]", identity, maxsplit=1)[0]
                package, _, version = recipe.partition("/")
                if package != name or not re.fullmatch(
                    r"[a-zA-Z0-9_][a-zA-Z0-9._-]{0,127}", version
                ):
                    raise ValueError(f"Invalid OCI version tag for {identity}")
            references.add((repo, digest, version, config if push else None))
    return sorted(references)


def check_manifest(client, repo, digest, *, retry_not_found=False):
    container = client.get_container(f"{repo}@{digest}")
    delays = (0, 1, 2, 4, 8, 16) if retry_not_found else (0,)
    for delay in delays:
        if delay:
            sleep(delay)
        response = client.do_request(
            f"{client.prefix}://{container.manifest_url()}",
            headers={"Accept": IMAGE_MANIFEST},
        )
        if response.status_code != 404:
            break
    response.raise_for_status()
    if f"sha256:{hashlib.sha256(response.content).hexdigest()}" != digest:
        raise ValueError(f"Expected {digest} in {repo}")


def push_manifest(client, layout, repo, digest):
    payload = blob_path(layout, digest).read_bytes()
    if f"sha256:{hashlib.sha256(payload).hexdigest()}" != digest:
        raise ValueError(f"Local manifest digest mismatch: {digest}")
    manifest = json.loads(payload)
    if manifest.get("mediaType") != IMAGE_MANIFEST:
        raise ValueError(f"Expected an OCI image manifest: {digest}")

    container = client.get_container(f"{repo}@{digest}")
    for descriptor in (manifest["config"], *manifest["layers"]):
        path = blob_path(layout, descriptor["digest"])
        if path.stat().st_size != descriptor["size"]:
            raise ValueError(f"Local blob size mismatch: {descriptor['digest']}")
        client.upload_blob(str(path), container, descriptor).raise_for_status()

    # Preserve the exact bytes associated with the catalog digest.
    client.do_request(
        f"{client.prefix}://{container.manifest_url()}",
        "PUT",
        data=payload,
        headers={"Content-Type": IMAGE_MANIFEST},
    ).raise_for_status()
    # GHCR can return 404 by digest briefly after accepting the manifest.
    check_manifest(client, repo, digest, retry_not_found=True)
    return {"mediaType": IMAGE_MANIFEST, "digest": digest, "size": len(payload)}


def oci_platform(profile):
    os_name, arch = profile.rsplit("-", 1)
    return {
        "os": "js" if os_name == "emscripten" else os_name,
        "architecture": {
            "a64": "arm64",
            "x64": "amd64",
            "w32": "wasm",
            "w64": "wasm64",
        }[arch],
    }


def variant_key(descriptor):
    annotations = descriptor.get("annotations", {})
    key = tuple(annotations.get(name) for name in VARIANT_KEYS)
    if not all(isinstance(value, str) and value for value in key):
        raise ValueError(f"Missing package variant metadata for {descriptor['digest']}")
    return key


def push_index(client, repo, version, descriptors):
    container = client.get_container(f"{repo}:{version}")
    url = f"{client.prefix}://{container.manifest_url()}"
    response = client.do_request(url, headers={"Accept": IMAGE_INDEX})
    if response.status_code == 404:
        index = {"schemaVersion": 2, "mediaType": IMAGE_INDEX, "manifests": []}
    else:
        response.raise_for_status()
        index = response.json()
        if index.get("mediaType") != IMAGE_INDEX or index.get("schemaVersion") != 2:
            raise ValueError(f"Expected a package image index at {repo}:{version}")

    variants = {variant_key(entry): entry for entry in index["manifests"]}
    if len(variants) != len(index["manifests"]):
        raise ValueError(f"Duplicate package variants in {repo}:{version}")
    variants.update((variant_key(entry), entry) for entry in descriptors)
    # Generic platform selection takes the first match when Debug also exists.
    index["manifests"] = [
        variants[key]
        for key in sorted(variants, key=lambda key: (key[2] != "Release", key))
    ]
    payload = json.dumps(index, separators=(",", ":")).encode("utf-8")
    client.do_request(
        url, "PUT", data=payload, headers={"Content-Type": IMAGE_INDEX}
    ).raise_for_status()


def authenticate_push(client, repo, username, password):
    from oras.auth import get_auth_backend
    from oras.auth.utils import parse_auth_header

    host, _, repository = repo.partition("/")
    response = client.session.get(f"{client.prefix}://{host}/v2/", timeout=30)
    if response.status_code == 401:
        challenge = response.headers.get("WWW-Authenticate", "")
        scheme = challenge.partition(" ")[0].lower()
        if scheme == "basic":
            client.auth = get_auth_backend("basic", client.session)
        elif scheme == "bearer":
            auth = parse_auth_header(challenge)
            if not auth.realm or urlsplit(auth.realm).scheme != "https":
                raise ValueError(f"Expected an HTTPS token endpoint for {host}")
            # Authorize uploads before the SDK probes for existing blobs with HEAD.
            auth.scope = f"repository:{repository}:pull,push"
            client.auth.set_basic_auth(username, password)
            token = client.auth.request_token(auth)
            if not token:
                raise ValueError(f"Could not authorize a push to {repo}")
            client.auth.set_token_auth(token)
            return
        else:
            raise ValueError(
                f"Unsupported registry authentication challenge from {host}"
            )
    else:
        response.raise_for_status()
    client.auth.set_basic_auth(username, password)


def publish_catalogs(candidates, registry, command):
    from oras.client import OrasClient

    writing = command != "verify"
    username = os.environ.get("OCI_USERNAME") if writing else None
    password = os.environ.get("OCI_PASSWORD") if writing else None
    if bool(username) != bool(password):
        raise ValueError(
            "Set both OCI_USERNAME and OCI_PASSWORD for authenticated publishing"
        )

    groups = {}
    layout_descriptors = {}
    for profile, output in candidates:
        if command == "index":
            layout = json.loads((output / "oci/index.json").read_text(encoding="utf-8"))
            layout_descriptors[output] = {
                manifest["digest"]: {
                    key: manifest[key] for key in ("mediaType", "digest", "size")
                }
                for manifest in layout["manifests"]
            }
        for repo, digest, version, config in catalog_references(
            profile, output, registry, push=writing
        ):
            groups.setdefault((repo, version), []).append(
                (profile, config, digest, output)
            )

    for (repo, version), builds in groups.items():
        # Registry authorization tokens are scoped to a repository.
        client = OrasClient()
        # Keep Requests from taking registry credentials from .netrc.
        client.session.auth = lambda request: request
        client.session.request = partial(client.session.request, timeout=(10, 120))
        with client.session:
            if not writing:
                for digest in sorted({build[2] for build in builds}):
                    check_manifest(client, repo, digest)
                    print(f"Verified public access to {repo}@{digest}")
                continue
            if username:
                authenticate_push(client, repo, username, password)
            manifests = {}
            descriptors = []
            for profile, config, digest, output in builds:
                if digest not in manifests:
                    if command == "index":
                        manifests[digest] = layout_descriptors[output][digest]
                    else:
                        manifests[digest] = push_manifest(
                            client, output / "oci", repo, digest
                        )
                        print(f"Uploaded {repo}@{digest}")
                if command == "upload":
                    continue
                os_name, arch = profile.rsplit("-", 1)
                descriptors.append(
                    {
                        **manifests[digest],
                        "platform": oci_platform(profile),
                        "annotations": {
                            "platform.os": os_name,
                            "platform.architecture": arch,
                            "build.type": config.capitalize(),
                        },
                    }
                )
            if descriptors:
                push_index(client, repo, version, descriptors)
                print(f"Published {repo}:{version}")


def prepare(profile, output, registry, cache):
    layout = output / "oci"
    (layout / "blobs" / "sha256").mkdir(parents=True, exist_ok=True)
    manifests = {}
    result = {}
    platform = oci_platform(profile)
    configs = ["release", "debug"] if profile.startswith("windows-") else ["release"]
    for config in configs:
        stage = output / config / "packages"
        packages = json.loads((stage / "packages.json").read_text(encoding="utf-8"))
        result[config] = {}
        for name, package in packages.items():
            archive = stage / package["archive"]
            with archive.open("rb") as stream:
                layer_hash = hashlib.file_digest(stream, "sha256").hexdigest()
            with gzip.open(archive, "rb") as stream:
                tar_hash = hashlib.file_digest(stream, "sha256").hexdigest()
            layer_digest = f"sha256:{layer_hash}"
            layer_blob = blob_path(layout, layer_digest)
            if layer_blob.exists():
                # Existing OCI layouts can contain read-only, content-addressed blobs.
                with layer_blob.open("rb") as stream:
                    if hashlib.file_digest(stream, "sha256").hexdigest() != layer_hash:
                        raise ValueError(f"Corrupted OCI layout blob: {layer_digest}")
            else:
                shutil.copyfile(archive, layer_blob)
            image_config = layout_json_blob(
                layout,
                IMAGE_CONFIG,
                {
                    "architecture": platform["architecture"],
                    "os": platform["os"],
                    "rootfs": {"type": "layers", "diff_ids": [f"sha256:{tar_hash}"]},
                },
            )
            descriptor = layout_json_blob(
                layout,
                IMAGE_MANIFEST,
                {
                    "schemaVersion": 2,
                    "mediaType": IMAGE_MANIFEST,
                    "config": image_config,
                    "layers": [
                        {
                            "mediaType": LAYER,
                            "digest": layer_digest,
                            "size": archive.stat().st_size,
                            "annotations": {
                                "org.opencontainers.image.title": archive.name
                            },
                        }
                    ],
                },
            )
            digest = descriptor["digest"]
            descriptor["annotations"] = {
                "org.opencontainers.image.ref.name": digest.replace(":", "-")
            }
            manifests[digest] = descriptor
            repo = f"{registry}/{name}"
            if cache:
                cache.mkdir(parents=True, exist_ok=True)
                (cache / "manifests").mkdir(exist_ok=True)
                sha = digest.removeprefix("sha256:")
                shutil.copyfile(
                    blob_path(layout, digest), cache / "manifests" / f"{sha}.json"
                )
                shutil.copyfile(archive, cache / f"{layer_hash}.tar")
            result[config][name] = f"{repo}@{digest}"
    write_json(layout / "oci-layout", {"imageLayoutVersion": "1.0.0"})
    write_json(
        layout / "index.json",
        {
            "schemaVersion": 2,
            "mediaType": IMAGE_INDEX,
            "manifests": [manifests[digest] for digest in sorted(manifests)],
        },
    )
    write_json(output / f"{profile}.json", {profile: result})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "command", choices=("prepare", "upload", "index", "push", "verify")
    )
    parser.add_argument("profile", choices=PROFILES, nargs="?")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--registry", required=True, help="Registry and repository prefix"
    )
    parser.add_argument(
        "--seed-cache",
        type=Path,
        help="Seed a scratch O3DE OCI cache for a local round trip",
    )
    args = parser.parse_args()
    registry = args.registry.lower().rstrip("/")
    output = args.output.resolve()
    if args.profile is None:
        if args.command not in ("push", "index"):
            parser.error(f"{args.command} requires a profile")
        candidates = [
            (profile, output / f"oci-{profile}")
            for profile in PROFILES
            if (output / f"oci-{profile}").is_dir()
        ]
        if not candidates:
            parser.error(
                f"{args.command} requires a profile or a directory of oci-<profile> artifacts"
            )
        publish_catalogs(candidates, registry, command=args.command)
    elif args.command == "prepare":
        prepare(args.profile, output, registry, args.seed_cache)
    else:
        publish_catalogs([(args.profile, output)], registry, command=args.command)


if __name__ == "__main__":
    main()
