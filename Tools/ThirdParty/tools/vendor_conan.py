#!/usr/bin/env python3
import re
import shutil
import sys
from pathlib import Path

CONAN_SRC = Path(r"D:\OpenSource\conan\conan")
DEST = Path(__file__).resolve().parent.parent / "src" / "thirdparty" / "_conan"

COPY_FILES = [
    "errors.py",
    "__init__.py",
    "internal/__init__.py",
    "internal/default_settings.py",
    "internal/errors.py",
    "internal/internal_tools.py",
    "internal/methods.py",
    "internal/paths.py",
    "internal/subsystems.py",
    "internal/util/__init__.py",
    "internal/util/config_parser.py",
    "internal/util/dates.py",
    "internal/util/files.py",
    "internal/util/runners.py",
    "internal/graph/__init__.py",
    "internal/graph/graph_error.py",
    "internal/api/detect/detect_vs.py",
]

COPY_DIRS = [
    "api/output.py",
    "api/__init__.py",
    "api/model",
    "internal/model",
    "tools",
]

IMPORT_RE = re.compile(
    r'\b(from\s+|import\s+)(conan)((?:\.[\w]+)+|\s)',
    re.MULTILINE,
)


def rewrite_imports(src: str) -> str:
    def _sub(m: re.Match) -> str:
        return m.group(1) + "thirdparty._conan" + m.group(3)
    return IMPORT_RE.sub(_sub, src)


def copy_item(rel: str) -> None:
    src = CONAN_SRC / rel
    dst = DEST / rel
    if not src.exists():
        print(f"  SKIP (not found): {rel}")
        return
    if src.is_dir():
        for f in src.rglob("*.py"):
            _copy_py(f, DEST / f.relative_to(CONAN_SRC))
    else:
        _copy_py(src, dst)


def _copy_py(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    text = src.read_text(encoding="utf-8")
    text = rewrite_imports(text)
    dst.write_text(text, encoding="utf-8")


def write_stub(rel: str, symbols: list[str], extra: str = "") -> None:
    dst = DEST / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    lines = []
    for sym in symbols:
        if "=" in sym:
            lines.append(sym)
        else:
            lines.append(f"{sym} = None")
    if extra:
        lines.append(extra)
    dst.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    if DEST.exists():
        print(f"Removing existing {DEST}")
        shutil.rmtree(DEST)
    DEST.mkdir(parents=True)

    print("Copying files...")
    for rel in COPY_FILES:
        copy_item(rel)
    for rel in COPY_DIRS:
        copy_item(rel)

    print("Writing stubs...")

    write_stub("internal/graph/graph.py",
               symbols=[],
               extra="""from thirdparty._conan.internal.graph.graph_error import GraphError, GraphConflictError
RECIPE_DOWNLOADED = "Downloaded"
RECIPE_INCACHE = "Cache"
RECIPE_UPDATED = "Updated"
RECIPE_INCACHE_DATE_UPDATED = "Cache (Updated date)"
RECIPE_NEWER = "Newer"
RECIPE_NOT_IN_REMOTE = "Not in remote"
RECIPE_UPDATEABLE = "Update available"
RECIPE_EDITABLE = "Editable"
RECIPE_CONSUMER = "Consumer"
RECIPE_VIRTUAL = "Cli"
RECIPE_PLATFORM = "Platform"
BINARY_CACHE = "Cache"
BINARY_DOWNLOAD = "Download"
BINARY_UPDATE = "Update"
BINARY_BUILD = "Build"
BINARY_MISSING = "Missing"
BINARY_SKIP = "Skip"
BINARY_EDITABLE = "Editable"
BINARY_EDITABLE_BUILD = "EditableBuild"
BINARY_INVALID = "Invalid"
BINARY_PLATFORM = "Platform"
CONTEXT_HOST = "host"
CONTEXT_BUILD = "build"


class Overrides:
    def __init__(self):
        self._overrides = {}

    def __bool__(self):
        return bool(self._overrides)
""")

    write_stub("internal/rest/__init__.py", [])
    write_stub("internal/rest/caching_file_downloader.py", [],
               extra="""import hashlib
import os
import urllib.request


class SourcesCachingDownloader:
    def __init__(self, conanfile):
        self._output = conanfile.output

    def download(self, urls, file_path, retry, retry_wait, verify_ssl, auth, headers, md5, sha1, sha256):
        if isinstance(urls, str):
            urls = [urls]
        last_err = None
        for url in urls:
            try:
                self._output.info(f"Downloading {url}")
                os.makedirs(os.path.dirname(file_path) or ".", exist_ok=True)
                req = urllib.request.Request(url, headers=headers or {})
                with urllib.request.urlopen(req) as resp, open(file_path, "wb") as f:
                    f.write(resp.read())
                if sha256:
                    with open(file_path, "rb") as f:
                        digest = hashlib.sha256(f.read()).hexdigest()
                    if digest != sha256:
                        raise RuntimeError(f"SHA256 mismatch: expected {sha256}, got {digest}")
                return
            except Exception as e:
                last_err = e
                self._output.warning(f"Download failed ({url}): {e}")
        raise RuntimeError(f"All download URLs failed: {last_err}")
""")
    write_stub("internal/rest/file_uploader.py", [],
               extra="""
class FileProgress:
    def __init__(self, *a, **kw): pass
""")

    write_stub("internal/cache/__init__.py", [])
    write_stub("internal/cache/home_paths.py", [],
               extra="""
class HomePaths:
    def __init__(self, *a, **kw): pass
""")

    write_stub("internal/api/__init__.py", [])
    write_stub("internal/api/detect/__init__.py", [])
    write_stub("internal/api/detect/detect_api.py", [],
               extra="""
def default_cppstd(*a, **kw): return "17"
def default_cstd(*a, **kw): return "11"
def detect_api(*a, **kw): return {}
""")
    # detect_vs.py is copied from conan source (see COPY_FILES)
    write_stub("internal/api/install/__init__.py", [])
    write_stub("internal/api/install/generators.py", [],
               extra="""
def relativize_path(path, *a, **kw): return path
""")

    write_stub("internal/runner/__init__.py", [])
    write_stub("internal/loader.py", [],
               extra="""
class ConanFileLoader:
    def __init__(self, *a, **kw): pass
""")
    write_stub("internal/hook_manager.py", [],
               extra="""
class HookManager:
    def __init__(self, *a, **kw): pass
    def call_hook(self, *a, **kw): pass
""")

    print("Ensuring __init__.py files...")
    for d in DEST.rglob("*"):
        if d.is_dir():
            init = d / "__init__.py"
            if not init.exists():
                init.write_text("", encoding="utf-8")

    py_count = sum(1 for _ in DEST.rglob("*.py"))
    print(f"Done. {py_count} .py files in {DEST}")


if __name__ == "__main__":
    main()
