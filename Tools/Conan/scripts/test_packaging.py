#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import hashlib
import json
import os
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, Mock, patch

from conan.internal.model.conf import Conf
from conan.internal.model.cpp_info import CppInfo
from conan.internal.model.pkg_type import PackageType

sys.path.insert(
    0, str(Path(__file__).resolve().parents[1] / "extensions" / "deployers")
)

from archive_deployer import SBOM_FILENAME, archive_package
from archive_deployer import deploy as deploy_archive
from hooks.hook_debug_symbols import pre_generate
from hooks.hook_sbom import format_sbom
from oci_deployer import deploy
from package_cps import package_manifest
from publish import (
    IMAGE_INDEX,
    IMAGE_MANIFEST,
    authenticate_push,
    catalog_references,
    check_manifest,
    prepare,
    publish_catalogs,
    push_manifest,
)

from build import cache_folder


class PackagingTests(unittest.TestCase):
    def test_debug_path_maps_preserve_flags_and_prefer_nested_sources(self):
        root = Path(tempfile.gettempdir()).resolve() / "example path"
        conf = Conf()
        conf.define("user.package:debug_symbols", 3)
        conf.define("tools.build:cflags", ["-g"])
        recipe = SimpleNamespace(
            name="example",
            version="1.2.3",
            source_folder=root / "source",
            build_folder=root,
            conf=conf,
            settings=Mock(get_safe={"compiler": "clang", "os": "Linux"}.get),
            settings_build=Mock(get_safe={"os": "Linux"}.get),
            dependencies={},
        )
        pre_generate(recipe)

        flags = conf.get("user.package:debug_path_flags")
        self.assertEqual(
            flags,
            [
                f"-ffile-prefix-map={root.as_posix()}=/__debug__/build/example/1.2.3",
                f"-ffile-prefix-map={(root / 'source').as_posix()}=/__debug__/source/example/1.2.3",
            ],
        )
        self.assertEqual(conf.get("tools.build:cflags")[0], "-g")

    def test_sbom_uses_package_manager_neutral_references(self):
        package = {
            "name": "example",
            "version": "1.2.3",
            "purl": "pkg:conan/example@1.2.3",
            "bom-ref": "pkg:conan/example@1.2.3?rrev=recipe",
        }
        dependency = {
            "name": "dependency",
            "version": "4.0.0+patch",
            "purl": "pkg:conan/dependency@4.0.0+patch",
            "bom-ref": "pkg:conan/dependency@4.0.0+patch?rrev=other",
        }
        bom = {
            "metadata": {
                "component": package,
                "tools": {"components": [{"name": "Conan-io"}]},
            },
            "components": [dependency],
            "dependencies": [
                {"ref": package["bom-ref"], "dependsOn": [dependency["bom-ref"]]},
                {"ref": dependency["bom-ref"]},
            ],
        }

        text = format_sbom(bom)
        self.assertNotIn("conan", text.casefold())
        result = json.loads(text)
        self.assertEqual(
            result["metadata"]["component"]["purl"], "pkg:generic/example@1.2.3"
        )
        self.assertEqual(
            result["components"][0]["purl"], "pkg:generic/dependency@4.0.0%2Bpatch"
        )
        self.assertEqual(
            result["dependencies"],
            [
                {"ref": package["bom-ref"], "dependsOn": [dependency["bom-ref"]]},
                {"ref": dependency["bom-ref"]},
            ],
        )
        self.assertEqual(format_sbom(result), text)

    def test_cps_describes_import_libraries_and_components(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            for path in (
                "bin/core.dll",
                "include/core.h",
                "lib/core.lib",
                "lib/extra.lib",
            ):
                artifact = root / path
                artifact.parent.mkdir(exist_ok=True)
                artifact.touch()
            dependency = Mock(
                package_folder=str(root),
                languages=None,
                package_type=PackageType.LIBRARY,
            )
            dependency.ref.name = "example"
            dependency.ref.version = "1.2.3"
            dependency.dependencies.host = {}
            dependency.cpp_info = CppInfo()
            headers = dependency.cpp_info.components["headers"]
            headers.includedirs = [str(root / "include")]
            core = dependency.cpp_info.components["core"]
            core.bindirs = [str(root / "bin")]
            core.libdirs = [str(root / "lib")]
            core.libs = ["core", "extra"]
            core.requires = ["headers"]

            manifest = package_manifest(dependency, root, {"example"})
            components = manifest["components"]
            self.assertEqual(components["headers"]["includes"], ["@prefix@/include"])
            self.assertEqual(
                components["core"]["requires"],
                [":headers", ":core.core", ":core.extra"],
            )
            self.assertEqual(
                components["core.core"],
                {
                    "type": "dylib",
                    "location": "@prefix@/bin/core.dll",
                    "link_location": "@prefix@/lib/core.lib",
                },
            )
            self.assertEqual(
                components["core.extra"]["location"], "@prefix@/lib/extra.lib"
            )

    def test_cps_describes_static_frameworks_as_archives(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            artifact = root / "lib/QtCore.framework/QtCore"
            artifact.parent.mkdir(parents=True)
            artifact.touch()
            dependency = Mock(languages=None, package_type="static-library")
            dependency.ref.name = "qt"
            dependency.ref.version = "6.12.0"
            dependency.cpp_info = CppInfo()
            dependency.dependencies.host = {}
            dependency.dependencies.direct_host = {}

            manifest = package_manifest(dependency, root, {"qt"})
            self.assertEqual(manifest["components"]["QtCore"]["type"], "archive")

    def test_cps_describes_flat_pyside6_sdk(self):
        with tempfile.TemporaryDirectory() as folder:
            package = Path(folder)
            for relative in (
                "bin/shiboken6.exe",
                "bin/pyside6.abi3.dll",
                "lib/pyside6.abi3.lib",
                "PySide6/pyside6.abi3.dll",
            ):
                path = package / relative
                path.parent.mkdir(exist_ok=True)
                path.touch()
            dependency = Mock(languages=None, package_type="shared-library")
            dependency.ref.name = "pyside6"
            dependency.ref.version = "6.12.0"
            dependency.cpp_info = CppInfo()
            dependency.dependencies.host = {}
            dependency.dependencies.direct_host = {}

            manifest = package_manifest(dependency, package.resolve(), {"pyside6"})
            components = manifest["components"]
            self.assertEqual(
                components["shiboken6"]["location"], "@prefix@/bin/shiboken6.exe"
            )
            self.assertEqual(
                components["pyside6.abi3"]["location"], "@prefix@/bin/pyside6.abi3.dll"
            )

    def test_apple_deploy_signs_the_staged_package(self):
        for platform in ("Macos", "iOS"):
            with (
                self.subTest(platform=platform),
                tempfile.TemporaryDirectory() as folder,
            ):
                root = Path(folder).resolve()
                package = root / "cache"
                package.mkdir()
                (package / "python").write_bytes(b"cached")
                (package / SBOM_FILENAME).write_bytes(b"{}")
                dependency = Mock(package_folder=package, languages=None)
                dependency.ref.name = "python"
                dependency.ref.version = "3.15.2"
                dependency.cpp_info = CppInfo()
                dependency.dependencies.host = {}
                dependency.dependencies.direct_host = {}
                dependency.settings.os = platform
                dependency.settings.get_safe.return_value = platform
                dependency.pref.repr_notime.return_value = "python/3.15.2"
                graph = Mock(nodes=[])
                consumer = graph.root.conanfile
                consumer.extension_properties = {}
                consumer.dependencies.direct_host = {"python": dependency}
                consumer.dependencies.host = {}
                signing = Mock()
                consumer.python_requires = {"apple-signing": Mock(module=signing)}

                def sign_tree(conanfile, staged, **_):
                    self.assertIs(conanfile, consumer)
                    self.assertNotEqual(staged, package)
                    self.assertEqual((staged / "python").read_bytes(), b"cached")
                    (staged / "python").write_bytes(b"signed")

                signing.sign_tree.side_effect = sign_tree
                output = root / "packages"
                deploy(graph, output)

                signing.sign_tree.assert_called_once()
                options = signing.sign_tree.call_args.kwargs
                self.assertEqual(options["ios"], platform == "iOS")
                if platform == "Macos":
                    self.assertEqual(
                        options["entitlements"].name, "python_runtime.entitlements"
                    )
                    self.assertTrue(options["entitlements"].is_file())
                else:
                    self.assertIsNone(options["entitlements"])
                self.assertEqual((package / "python").read_bytes(), b"cached")
                with tarfile.open(output / "python.tar.gz") as tar:
                    cps = json.load(tar.extractfile("lib/cps/python.cps"))
                    self.assertEqual(cps["cps_path"], "@prefix@/lib/cps")
                    self.assertEqual(cps["version"], "3.15.2")
                    self.assertEqual(tar.extractfile("python").read(), b"signed")

    @unittest.skipIf(sys.platform == "win32", "Apple packages use Unix symlinks")
    def test_framework_links_use_native_tar_entries(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            framework = root / "source/Python.framework"
            (framework / "Versions/3.15").mkdir(parents=True)
            (framework / "Versions/3.15/Python").write_bytes(b"code")
            (framework / "Versions/Current").symlink_to("3.15")
            (framework / "Python").symlink_to("Versions/Current/Python")
            for compression in ("gz", "xz"):
                with self.subTest(compression=compression):
                    archive = root / f"framework.tar.{compression}"
                    archive_package(
                        framework.parent,
                        archive,
                        preserve_links=True,
                        standalone=compression == "xz",
                    )
                    destination = root / compression
                    with tarfile.open(archive) as tar:
                        self.assertNotIn(".o3de-symlinks.json", tar.getnames())
                        self.assertNotIn(".package-symlinks.json", tar.getnames())
                        tar.extractall(destination, filter="data")
                    extracted = destination / "Python.framework"
                    self.assertEqual(
                        (extracted / "Versions/Current").readlink(), Path("3.15")
                    )
                    self.assertEqual(
                        (extracted / "Python").readlink(),
                        Path("Versions/Current/Python"),
                    )
                    self.assertEqual((extracted / "Python").read_bytes(), b"code")

    def test_oci_and_standalone_deploy_share_package_root(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            package = root / "cache"
            (package / "bin").mkdir(parents=True)
            (package / "bin" / "tool").write_bytes(b"tool")
            (package / "bin" / "MSVCP140.DLL").write_bytes(b"system runtime")
            (package / "bin" / "tool.dll").write_bytes(b"package library")
            (package / "conaninfo.txt").write_bytes(b"package settings")
            (package / "conanmanifest.txt").write_bytes(b"cache checksums")
            (package / "bin/tool.py").write_bytes(b"print('tool')")
            (package / "bin/__pycache__").mkdir()
            (package / "bin/__pycache__/tool.cpython-315.pyc").write_bytes(
                b"build path"
            )
            (package / "licenses").mkdir()
            (package / "licenses" / "LICENSE").write_bytes(b"license")
            dependency = Mock(
                package_folder=package, languages=None, package_type="application"
            )
            dependency.ref.name = "example"
            dependency.ref.version = "1.2.3"
            dependency.ref.revision = "recipe"
            dependency.ref.user = dependency.ref.channel = None
            dependency.name = "example"
            dependency.context = "host"
            dependency.license = "mit"
            dependency.author = dependency.description = dependency.homepage = None
            dependency.settings.get_safe.return_value = "Windows"
            dependency.pref.repr_notime.return_value = "example/1.2.3#recipe:id#package"
            dependency.cpp_info = CppInfo()
            dependency.dependencies.host = {}
            dependency.dependencies.direct_host = {}
            node = Mock(
                ref=dependency.ref,
                conanfile=dependency,
                context="host",
                test=False,
                edges=[],
            )
            node.name = "example"
            dependency.subgraph = Mock(root=node, nodes=[node])
            graph = Mock(nodes=[node])
            graph.root.conanfile.extension_properties = {}
            graph.root.conanfile.dependencies.direct_host = {"example": dependency}
            graph.root.conanfile.dependencies.host = {}
            output = root / "packages"

            deploy(graph, output)
            deploy_archive(graph, output)
            standalone = output / "archives"
            manifest = json.loads((standalone / "packages.json").read_text())
            oci = json.loads((output / "packages.json").read_text())
            self.assertEqual(
                manifest["example"]["identity"], oci["example"]["identity"]
            )
            archive = standalone / manifest["example"]["archive"]
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            self.assertEqual(archive.name, f"example-1.2.3-{digest}.tar.xz")
            self.assertEqual(
                archive.with_suffix(".xz.SHA256SUMS").read_text().splitlines(),
                [f"{digest}  {archive.name}"],
            )
            with (
                tarfile.open(output / "example.tar.gz") as layer,
                tarfile.open(archive, "r:xz") as tar,
            ):
                files = [member.name for member in layer if member.isfile()]
                self.assertNotIn("conaninfo.txt", files)
                self.assertNotIn("conanmanifest.txt", files)
                self.assertNotIn("bin/MSVCP140.DLL", files)
                self.assertIn("bin/tool.dll", files)
                self.assertNotIn("bin/__pycache__/tool.cpython-315.pyc", files)
                self.assertIn("lib/cps/example.cps", files)
                bom = json.load(layer.extractfile(SBOM_FILENAME))
                self.assertEqual(bom["specVersion"], "1.6")
                self.assertNotIn("conan", json.dumps(bom).casefold())
                self.assertEqual(
                    bom["metadata"]["component"]["licenses"],
                    [{"license": {"id": "MIT", "acknowledgement": "declared"}}],
                )
                self.assertEqual(bom["components"], [])
                self.assertFalse((package / SBOM_FILENAME).exists())
                self.assertEqual(
                    [(entry.name, entry.type) for entry in tar],
                    [(entry.name, entry.type) for entry in layer],
                )
                checksums = []
                for name in files:
                    contents = tar.extractfile(name).read()
                    self.assertEqual(contents, layer.extractfile(name).read())
                    checksums.append(
                        f"{hashlib.sha256(contents).hexdigest()}  {name}\n"
                    )
                content = "".join(checksums).encode()
                self.assertEqual(
                    archive.with_suffix(".xz.content.SHA256SUMS").read_bytes(), content
                )

    def test_deploy_skips_build_only_and_engine_provided_packages(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            package = root / "cache"
            package.mkdir()
            (package / "libOpenImageIO.a").write_bytes(b"archive")
            (package / SBOM_FILENAME).write_bytes(b"{}")
            (package / "licenses").mkdir()
            (package / "licenses/LICENSE").write_bytes(b"license")
            imaging = Mock(
                package_folder=package, languages=None, package_type="static-library"
            )
            imaging.ref.name = "openimageio"
            imaging.ref.version = "1.2.3"
            imaging.settings.os = "Emscripten"
            imaging.settings.get_safe.return_value = "Emscripten"
            imaging.pref.repr_notime.return_value = "openimageio/1.2.3"
            imaging.cpp_info = CppInfo()
            tool = Mock(
                conan_data={"publish": False}, package_folder=None, cpp_info=CppInfo()
            )
            tool.ref.name = "tool"
            engine = Mock(package_type="static-library", package_folder=None)
            engine.ref.name = "zstd"
            engine.ref.version = "1.5.7"
            link = Mock(
                libs=True, headers=True, transitive_headers=False, transitive_libs=False
            )
            imaging.dependencies.host = {link: engine}
            imaging.dependencies.direct_host = {link: engine}
            node = Mock(context="host", ref=imaging.ref, conanfile=imaging, edges=[])
            graph = Mock(nodes=[node])
            consumer = graph.root.conanfile
            consumer.extension_properties = {"engine_provided": ("zstd",)}
            consumer.dependencies.direct_host = {"openimageio": imaging, "tool": tool}
            consumer.dependencies.host = {link: engine}
            output = root / "packages"

            deploy(graph, output)
            deploy_archive(graph, output)

            for index in (output / "packages.json", output / "archives/packages.json"):
                self.assertEqual(set(json.loads(index.read_text())), {"openimageio"})
            with tarfile.open(output / "openimageio.tar.gz") as archive:
                cps = json.load(archive.extractfile("lib/cps/openimageio.cps"))
                self.assertEqual(cps["requires"]["zstd"]["version"], "1.5.7")

    def test_deploy_includes_transitive_sdk_dependencies(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            package = root / "cache"
            package.mkdir()
            (package / "header.h").touch()
            (package / SBOM_FILENAME).write_bytes(b"{}")

            def dependency(name):
                result = Mock(package_folder=package, languages=None, conan_data={})
                result.ref.name = name
                result.ref.version = "1.0"
                result.settings.get_safe.return_value = "Linux"
                result.pref.repr_notime.return_value = f"{name}/1.0"
                result.cpp_info = CppInfo()
                result.dependencies.host = {}
                result.dependencies.direct_host = {}
                return result

            sdk = dependency("sdk")
            headers = dependency("headers")
            component = dependency("component")
            static = dependency("static")
            private = dependency("private")
            sdk.cpp_info.components["core"].requires = ["component::component"]
            sdk.dependencies.host = {"component": component}
            graph = Mock(nodes=[])
            consumer = graph.root.conanfile
            consumer.extension_properties = {}
            consumer.dependencies.direct_host = {"sdk": sdk}
            consumer.dependencies.host = {
                Mock(headers=True, libs=False): headers,
                Mock(headers=False, libs=False): component,
                Mock(headers=False, libs=True): static,
                Mock(headers=False, libs=False): private,
            }
            for platform in ("Linux", "Emscripten"):
                with self.subTest(platform=platform):
                    consumer.settings.os = platform
                    output = root / platform
                    deploy(graph, output)
                    deploy_archive(graph, output)
                    expected = {"sdk", "headers", "component"}
                    if platform == "Emscripten":
                        expected.add("static")
                    for index in (
                        output / "packages.json",
                        output / "archives/packages.json",
                    ):
                        self.assertEqual(set(json.loads(index.read_text())), expected)

    def test_ownership_change_uses_a_new_cache(self):
        exclusions = ["~ispc/*", "~ispc-texture-compressor/*"]
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            cache = root / "cache"
            cache.mkdir()
            (cache / "remotes.json").write_text(
                json.dumps(
                    {
                        "remotes": [
                            {"name": "conancenter", "allowed_packages": exclusions}
                        ]
                    }
                ),
                encoding="utf-8",
            )
            self.assertEqual(cache_folder(root, exclusions), cache)
            self.assertNotEqual(cache_folder(root, [*exclusions, "~qt/*"]), cache)

    def test_archive_and_cache_match(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            stage = root / "release" / "packages"
            stage.mkdir(parents=True)
            source = root / "source"
            (source / "bin").mkdir(parents=True)
            (source / "bin" / "tool").write_bytes(b"tool")
            (source / "bin" / "tool").chmod(0o755)
            archive = stage / "expat.tar.gz"
            archive_package(source, archive)
            repeat = root / "repeat.tar.gz"
            archive_package(source, repeat)
            self.assertEqual(archive.read_bytes(), repeat.read_bytes())
            with tarfile.open(archive) as tar:
                self.assertEqual(tar.extractfile("bin/tool").read(), b"tool")
            (stage / "packages.json").write_text(
                json.dumps({"expat": {"archive": archive.name}}), encoding="utf-8"
            )
            cache = root / "cache" / "oci"
            prepare("linux-x64", root, "ghcr.io/test/test", cache)
            catalog = json.loads((root / "linux-x64.json").read_text(encoding="utf-8"))
            digest = catalog["linux-x64"]["release"]["expat"].split("@sha256:")[1]
            manifest = (cache / "manifests" / f"{digest}.json").read_bytes()
            self.assertEqual(hashlib.sha256(manifest).hexdigest(), digest)
            layer = json.loads(manifest)["layers"][0]["digest"].removeprefix("sha256:")
            self.assertEqual(
                (cache / f"{layer}.tar").read_bytes(), archive.read_bytes()
            )

            layout = root / "oci"
            index = json.loads((layout / "index.json").read_bytes())
            self.assertEqual(index["manifests"][0]["digest"], f"sha256:{digest}")
            layer_blob = layout / "blobs" / "sha256" / layer
            self.assertEqual(layer_blob.read_bytes(), archive.read_bytes())
            mode = layer_blob.stat().st_mode
            layer_blob.chmod(mode & ~0o222)
            try:
                prepare("linux-x64", root, "ghcr.io/test/test", cache)
            finally:
                layer_blob.chmod(mode)
            self.assertEqual(
                json.loads((root / "linux-x64.json").read_bytes()), catalog
            )

            client = Mock(prefix="https")
            client.get_container.return_value.manifest_url.return_value = (
                "ghcr.io/v2/test/test/expat/manifests/test"
            )
            client.do_request.side_effect = [
                Mock(status_code=201),
                Mock(status_code=404),
                Mock(status_code=200, content=manifest),
            ]
            with patch("publish.sleep") as wait:
                push_manifest(
                    client,
                    layout,
                    "ghcr.io/test/test/expat",
                    f"sha256:{digest}",
                )
            wait.assert_called_once_with(1)
            client.get_container.assert_called_with(
                f"ghcr.io/test/test/expat@sha256:{digest}"
            )
            self.assertEqual(client.upload_blob.call_count, 2)
            upload = client.do_request.call_args_list[0]
            self.assertEqual(upload.args[1], "PUT")
            self.assertEqual(upload.kwargs["data"], manifest)
            self.assertEqual(upload.kwargs["headers"], {"Content-Type": IMAGE_MANIFEST})

            client.do_request.side_effect = None
            client.do_request.return_value = Mock(
                status_code=200, content=manifest + b"\n"
            )
            with self.assertRaisesRegex(ValueError, "Expected sha256:"):
                check_manifest(client, "ghcr.io/test/test/expat", f"sha256:{digest}")

    def test_version_index_preserves_other_builds_on_partial_publish(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            registry = "ghcr.io/test/3rdparty"
            repo = f"{registry}/qt"
            metadata = {"qt": {"identity": "qt/6.12.1#recipe:package#revision"}}

            def candidate(profile, configs, digit):
                output = root / profile
                digest = "sha256:" + digit * 64
                entries = {"qt": f"{repo}@{digest}"}
                output.mkdir(exist_ok=True)
                (output / f"{profile}.json").write_text(
                    json.dumps({profile: dict.fromkeys(configs, entries)}),
                    encoding="utf-8",
                )
                for config in configs:
                    packages = output / config / "packages"
                    packages.mkdir(parents=True, exist_ok=True)
                    (packages / "packages.json").write_text(
                        json.dumps(metadata), encoding="utf-8"
                    )
                layout_index = output / "oci/index.json"
                layout_index.parent.mkdir(exist_ok=True)
                layout_index.write_text(
                    json.dumps(
                        {
                            "manifests": [
                                {
                                    "mediaType": IMAGE_MANIFEST,
                                    "digest": digest,
                                    "size": 123,
                                    "annotations": {
                                        "org.opencontainers.image.ref.name": digit
                                    },
                                }
                            ]
                        }
                    ),
                    encoding="utf-8",
                )
                return profile, output

            windows = candidate("windows-x64", ("release", "debug"), "a")
            ios = candidate("ios-a64", ("release",), "b")
            simulator = candidate("ios-simulator-a64", ("release",), "c")
            macos = candidate("macos-a64", ("release",), "e")
            self.assertEqual(
                catalog_references(*windows, registry, push=False),
                [(repo, "sha256:" + "a" * 64, None, None)],
            )

            client = MagicMock(prefix="https")
            client.get_container.return_value.manifest_url.return_value = (
                "ghcr.io/v2/test/3rdparty/qt/manifests/6.12.1"
            )
            client.do_request.side_effect = [
                Mock(status_code=404),
                Mock(status_code=201),
            ]
            with (
                patch.dict(os.environ, {"OCI_USERNAME": "", "OCI_PASSWORD": ""}),
                patch("oras.client.OrasClient", return_value=client),
                patch(
                    "publish.push_manifest",
                    side_effect=lambda client, layout, repo, digest: {
                        "mediaType": IMAGE_MANIFEST,
                        "digest": digest,
                        "size": 123,
                    },
                ) as upload,
            ):
                for build in (windows, ios, simulator, macos):
                    publish_catalogs([build], registry, command="upload")
                client.get_container.assert_not_called()
                self.assertEqual(upload.call_count, 4)

                publish_catalogs(
                    [windows, ios, simulator, macos], registry, command="index"
                )
                self.assertEqual(upload.call_count, 4)
                first = json.loads(client.do_request.call_args.kwargs["data"])
                self.assertEqual(first["mediaType"], IMAGE_INDEX)
                self.assertEqual(len(first["manifests"]), 5)
                self.assertEqual(
                    [
                        entry["annotations"]["build.type"]
                        for entry in first["manifests"]
                    ],
                    ["Release", "Release", "Release", "Release", "Debug"],
                )
                client.do_request.side_effect = [
                    Mock(status_code=200, json=lambda: first),
                    Mock(status_code=201),
                ]
                updated = candidate("ios-a64", ("release",), "d")
                publish_catalogs([updated], registry, command="push")
                self.assertEqual(upload.call_count, 5)

            self.assertEqual(client.get_container.call_count, 2)
            client.get_container.assert_called_with(f"{repo}:6.12.1")
            updated_index = json.loads(client.do_request.call_args.kwargs["data"])
            entries = updated_index["manifests"]
            self.assertEqual(
                [entry["digest"] for entry in entries],
                ["sha256:" + digit * 64 for digit in "dceaa"],
            )
            self.assertEqual(
                [entry["platform"] for entry in entries[:3]],
                [
                    {"os": os_name, "architecture": "arm64"}
                    for os_name in ("ios", "ios-simulator", "macos")
                ],
            )
            self.assertEqual(entries[1]["annotations"]["platform.os"], "ios-simulator")
            self.assertEqual(entries[1]["annotations"]["platform.architecture"], "a64")

    def test_push_token_has_pull_and_push_scope(self):
        client = Mock(prefix="https")
        client.session.get.return_value.status_code = 401
        client.session.get.return_value.headers = {
            "WWW-Authenticate": 'Bearer realm="https://ghcr.io/token",service="ghcr.io"',
        }
        client.auth.request_token.return_value = "test-token"
        authenticate_push(client, "ghcr.io/test/expat", "test-user", "test-password")
        challenge = client.auth.request_token.call_args.args[0]
        self.assertEqual(challenge.scope, "repository:test/expat:pull,push")
        client.auth.set_token_auth.assert_called_once_with("test-token")

    def test_link_escape(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            payload = root / "payload"
            payload.mkdir()
            outside = root / "outside"
            outside.touch()
            try:
                (payload / "escape").symlink_to(outside)
            except OSError as error:
                self.skipTest(str(error))
            with self.assertRaisesRegex(ValueError, "Invalid package link"):
                archive_package(payload, root / "test.tar.gz")


if __name__ == "__main__":
    unittest.main()
