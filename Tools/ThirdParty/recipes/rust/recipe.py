import os
import re
import urllib.request

from thirdparty import RecipeBase
from thirdparty.errors import RecipeException, RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version


_VERSION = "1.96.0"
_CHANNEL_MANIFEST_URL = "https://static.rust-lang.org/dist/channel-rust-stable.toml"

_COMPONENTS = ("cargo", "rustc", "rust-std")

_HOST_TRIPLES = {
    "Windows": {
        "X64": "x86_64-pc-windows-msvc",
        "ARM": "aarch64-pc-windows-msvc",
    },
    "Linux": {
        "X64": "x86_64-unknown-linux-gnu",
        "ARM": "aarch64-unknown-linux-gnu",
    },
    "Mac": {
        "X64": "x86_64-apple-darwin",
        "ARM": "aarch64-apple-darwin",
    },
}

_SOURCES = {
    "x86_64-pc-windows-msvc": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/cargo-1.96.0-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "a1ba24b0a35d1c3ce86dd24cf8633ad0d0b0eb1e5eaa1d01644417dea28351ee",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rustc-1.96.0-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "cfba935dbd6d0deaf0a02339f9b6c0573b30f41697919ba7532b60ef2e6306b4",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rust-std-1.96.0-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "48b22b9237d5d43cbec4f7076e816a4d0b1b682932dfa82afd2573434d43fc53",
        },
    },
    "aarch64-pc-windows-msvc": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/cargo-1.96.0-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "26a37960a64570fd30d40d100c6a9b8a598584dff97e78f64c7cebab733ea9e6",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rustc-1.96.0-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "438408d6a8917401b2f816d87befc5783cc015af59c6f479f3649214cafd081c",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rust-std-1.96.0-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "53c11671dbd91e634b92e304eb8163b2a38658c42e0152122709e2129298d754",
        },
    },
    "x86_64-unknown-linux-gnu": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/cargo-1.96.0-x86_64-unknown-linux-gnu.tar.xz",
            "sha256": "dee75c3c8f9f600ad75bc0c93249e767d3047845a4dd668327ce43ab039ba266",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rustc-1.96.0-x86_64-unknown-linux-gnu.tar.xz",
            "sha256": "7d7fa1d0cfb0fab71a956bb78f41107202c17f30ab56c45288e869a37fd9633d",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rust-std-1.96.0-x86_64-unknown-linux-gnu.tar.xz",
            "sha256": "c09c7c646248f14f473f5f7a029af15ee57c3a9f9bc93dfa72d9621938586b82",
        },
    },
    "aarch64-unknown-linux-gnu": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/cargo-1.96.0-aarch64-unknown-linux-gnu.tar.xz",
            "sha256": "09ea03e74aa94e07db7bc00bd2ec1ad86d90a7348c89fde3909a8922543b949f",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rustc-1.96.0-aarch64-unknown-linux-gnu.tar.xz",
            "sha256": "76b1a6e8dd1636e364d4bbba685485ff44eee5ff6434add089bab4c703c7e19d",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rust-std-1.96.0-aarch64-unknown-linux-gnu.tar.xz",
            "sha256": "538e85452709687797d990579a491ff9b02f8bffba4a5d54cfa945e28868053e",
        },
    },
    "x86_64-apple-darwin": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/cargo-1.96.0-x86_64-apple-darwin.tar.xz",
            "sha256": "23390ad69f74f3464774f17058f803e19cf45a9a11ee725b7a37e96c549f1243",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rustc-1.96.0-x86_64-apple-darwin.tar.xz",
            "sha256": "f503815fe9e8cf6d654f751532932b6a9b13b8615a40fc6dfb9760a18cf595a1",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rust-std-1.96.0-x86_64-apple-darwin.tar.xz",
            "sha256": "afabf23aff5bf6d27dba9608a7c7bec349bf9fda9c3e37983dd5cc44c9afbcca",
        },
    },
    "aarch64-apple-darwin": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/cargo-1.96.0-aarch64-apple-darwin.tar.xz",
            "sha256": "c042858192b7b6d66fe59b3bbbbd0f6e3cac6e8a478dc4cc091cde9eddea3c8b",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rustc-1.96.0-aarch64-apple-darwin.tar.xz",
            "sha256": "1bb7b0bad1d2a42fc4173ede6dd460de2774fc1858a8369329d3e081e4e3426c",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-05-28/rust-std-1.96.0-aarch64-apple-darwin.tar.xz",
            "sha256": "439c4f71060b913e00db3a2e01340b2da0aa49978b843e36871f3250267c63f8",
        },
    },
}


class Recipe(RecipeBase):
    name = "rust"
    version = _VERSION
    license = "MIT OR Apache-2.0"

    def latest_version(self):
        with urllib.request.urlopen(_CHANNEL_MANIFEST_URL, timeout=30) as response:
            manifest = response.read().decode("utf-8")

        match = re.search(r"rustc-([0-9]+\.[0-9]+\.[0-9]+)-src\.tar", manifest)
        if not match:
            match = re.search(r"rustc-([0-9]+\.[0-9]+\.[0-9]+)-", manifest)
        if not match:
            raise RecipeException("Could not find Rust stable version in channel manifest")
        return Version(match.group(1))

    def validate(self):
        self._target_triple

    def build(self):
        triple = self._target_triple
        for component in _COMPONENTS:
            entry = _SOURCES[triple][component]
            component_folder = self.folders.build / component
            os.makedirs(component_folder, exist_ok=True)
            get(
                self,
                url=entry["url"],
                sha256=entry["sha256"],
                destination=component_folder,
                strip_root=True,
            )

    def package(self):
        triple = self._target_triple
        os.makedirs(self.folders.package / ".cargo", exist_ok=True)

        for component in _COMPONENTS:
            component_folder = self.folders.build / component
            payload_name = f"rust-std-{triple}" if component == "rust-std" else component
            payload_folder = component_folder / payload_name
            if not os.path.isdir(payload_folder):
                raise RecipeException(f"Could not find Rust component payload: {payload_folder}")

            copy(self, "*", src=payload_folder, dst=self.folders.package)
            copy(
                self,
                "LICENSE*",
                src=component_folder,
                dst=self.folders.package / "licenses" / component,
                keep_path=False,
            )

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []

        bin_dir = self.folders.package / "bin"
        cargo = bin_dir / f"cargo{self._exe_suffix}"
        rustc = bin_dir / f"rustc{self._exe_suffix}"

        self.info.buildenv.prepend_path("PATH", bin_dir)
        self.info.buildenv.define("RUSTUP_TOOLCHAIN", "stable")
        self.info.buildenv.define_path("CARGO_HOME", self.folders.package / ".cargo")
        self.info.buildenv.define_path("CARGO", cargo)
        self.info.buildenv.define_path("RUSTC", rustc)
        self.info.conf.define("tools.rust:dir", self.folders.package)

    @property
    def _target_triple(self):
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        try:
            return _HOST_TRIPLES[os_name][arch]
        except KeyError as exc:
            raise RecipeInvalidConfiguration(
                f"{self.name} has no prebuilt toolchain for {os_name}/{arch}"
            ) from exc

    @property
    def _exe_suffix(self):
        return ".exe" if self.settings.os == "Windows" else ""
