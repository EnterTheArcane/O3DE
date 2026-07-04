import os
import re
import urllib.request

from thirdparty import RecipeBase
from thirdparty.errors import RecipeException, RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version


_VERSION = "1.96.1"
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
            "url": "https://static.rust-lang.org/dist/2026-06-30/cargo-1.96.1-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "e2c271f65ae10a2b40aebe483a2e7c0c566557f6bab8ab718be32ac9383a5081",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rustc-1.96.1-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "d226a2e142b4cd796df9db527f4f3ff79bc9ce4118b36dcd7c82b7eca557d0b8",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rust-std-1.96.1-x86_64-pc-windows-msvc.tar.xz",
            "sha256": "f77bef11e2c032f8aafcdc60b4e50d21becf06d05c027fa87d7f45bf9bd146bb",
        },
    },
    "aarch64-pc-windows-msvc": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/cargo-1.96.1-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "6b9b9ba76ed946afc03a904013561a6e07f7de161362ea54fda5aa27f08bc6d7",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rustc-1.96.1-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "16d7acac79b065c27b3b4d3f3bcffb8d30a407475c2a0cd2b7f0232f9b96bfbc",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rust-std-1.96.1-aarch64-pc-windows-msvc.tar.xz",
            "sha256": "1e3dbf6283206c6390a38312b41f26d88437c539a074ddb480e4aa503254a86f",
        },
    },
    "x86_64-unknown-linux-gnu": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/cargo-1.96.1-x86_64-unknown-linux-gnu.tar.xz",
            "sha256": "ecc53a3c49fab5ab8c9301b3bbc8fb1dff9be6c65287add3f57a0fe8fddfea9e",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rustc-1.96.1-x86_64-unknown-linux-gnu.tar.xz",
            "sha256": "3545a0efad2355ecb0a3b9ac02efee96e27f1f9d24b7ce2fc3f279b2efb0d923",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rust-std-1.96.1-x86_64-unknown-linux-gnu.tar.xz",
            "sha256": "1bf4fde5048cca33e6ea00c7471281ed96d792f6923141e3db45072743a1afae",
        },
    },
    "aarch64-unknown-linux-gnu": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/cargo-1.96.1-aarch64-unknown-linux-gnu.tar.xz",
            "sha256": "02a4d7bf424ea28d574fe5b5d29e7ae99b0bc11a5920d8f28fa7408aa6a37992",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rustc-1.96.1-aarch64-unknown-linux-gnu.tar.xz",
            "sha256": "4bc079af433c730af16aa90c96777985b86d40c4670670380160bd61626a577f",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rust-std-1.96.1-aarch64-unknown-linux-gnu.tar.xz",
            "sha256": "24bd362ff484421cae8be8c4d326fde143a086782e59e21fc7f353a5d04cd630",
        },
    },
    "x86_64-apple-darwin": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/cargo-1.96.1-x86_64-apple-darwin.tar.xz",
            "sha256": "128041aa757a6bbc60664079ff294281cdbabd51cc4b7c1f7d40cd1b1ea955f5",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rustc-1.96.1-x86_64-apple-darwin.tar.xz",
            "sha256": "814e1ed2d19952cf9771aeff5e0d4e0b77eefa76a689809f2c1bf769187ab051",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rust-std-1.96.1-x86_64-apple-darwin.tar.xz",
            "sha256": "a094fc4d30985f48aff70629d7a6c1f16d314a63f3cb141927095b6d506edd35",
        },
    },
    "aarch64-apple-darwin": {
        "cargo": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/cargo-1.96.1-aarch64-apple-darwin.tar.xz",
            "sha256": "2f43d75e9ad3febae5022c6f295cf93b74131cfdb1293a83e291f878ea9585a0",
        },
        "rustc": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rustc-1.96.1-aarch64-apple-darwin.tar.xz",
            "sha256": "9b548f0665f85f3c7fd45165611e3dea79f048c69d163be193986310d204fc2c",
        },
        "rust-std": {
            "url": "https://static.rust-lang.org/dist/2026-06-30/rust-std-1.96.1-aarch64-apple-darwin.tar.xz",
            "sha256": "0d433a74c303febc915f8fa1091ef166445706461d0c96984ecb7303aa8208f5",
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
                strip_root=True)

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
        self.info.conf.tools.rust.dir = self.folders.package

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
