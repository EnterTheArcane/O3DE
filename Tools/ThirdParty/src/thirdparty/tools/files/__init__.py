from __future__ import annotations

import contextlib
import hashlib
import os
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Any, Protocol, cast


class _RecipeLike(Protocol):
    """Structural protocol matching the attributes used by file helpers."""
    version: str
    recipe_folder: str
    source_folder: str
    package_folder: str
    thirdparty_data: dict[str, Any]


def get(
    url: str,
    dest: str,
    sha256: str | None = None,
    strip_root: bool = True,
    filename: str | None = None,
) -> None:
    """Download *url*, verify sha256, and extract the archive into *dest*.

    If *strip_root* is True (default) and the archive contains a single
    top-level directory, its contents are moved directly into *dest* so
    that ``<dest>/CMakeLists.txt`` exists rather than
    ``<dest>/<archive-name>/CMakeLists.txt``.
    """
    dest_path = Path(dest)
    dest_path.mkdir(parents=True, exist_ok=True)

    filename = filename or url.split("/")[-1].split("?")[0]
    print(f"[thirdparty] Downloading {filename} ...")

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        archive_path = tmp_path / filename

        # URLs originate from developer-controlled data.yml, not user input.
        urllib.request.urlretrieve(url, archive_path)  # noqa: S310

        if sha256 is not None:
            _verify_sha256(archive_path, sha256)

        print(f"[thirdparty] Extracting {filename} ...")
        extract_dir = tmp_path / "_extracted"
        extract_dir.mkdir()
        _extract(archive_path, extract_dir, filename)

        src_dir = extract_dir
        if strip_root:
            children = list(extract_dir.iterdir())
            if len(children) == 1 and children[0].is_dir():
                src_dir = children[0]

        for item in src_dir.iterdir():
            target = dest_path / item.name
            if target.exists():
                shutil.rmtree(target) if target.is_dir() else target.unlink()
            shutil.move(str(item), dest_path)


@contextlib.contextmanager
def chdir(path: str):
    """Context manager to temporarily change the working directory."""
    old = os.getcwd()
    try:
        os.chdir(path)
        yield
    finally:
        os.chdir(old)


def copy(pattern: str, src: str, dst: str, *, keep_path: bool = True) -> None:
    """Copy files matching *pattern* from *src* into *dst*.

    If *keep_path* is True (default) sub-directory structure is preserved.
    If *keep_path* is False all matched files are placed flat inside *dst*.
    """
    src_path = Path(src)
    dst_path = Path(dst)
    for match in src_path.glob(pattern):
        if match.is_file():
            if keep_path:
                rel = match.relative_to(src_path)
                target = dst_path / rel
            else:
                target = dst_path / match.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(match, target)


def rmdir(path: str) -> None:
    """Remove a directory tree (no-op if it does not exist)."""
    p = Path(path)
    if p.exists():
        shutil.rmtree(p)


def mkdir(recipe_or_path, path: str | None = None) -> None:
    """Create a directory (and parents). Accepts both ``mkdir(path)`` and
    Conan-style ``mkdir(self, path)`` calls."""
    target = path if path is not None else str(recipe_or_path)
    Path(target).mkdir(parents=True, exist_ok=True)


def download(recipe_or_url, url_or_filename: str, filename: str | None = None, **kwargs) -> None:
    """Download a single file without extracting.

    Accepts both ``download(url, filename)`` and Conan-style
    ``download(self, url, filename)`` calls.
    """
    if filename is not None:
        url = str(url_or_filename)
        dest_name = filename
    else:
        url = str(recipe_or_url)
        dest_name = str(url_or_filename)
    print(f"[thirdparty] download: {url} -> {dest_name}")
    urllib.request.urlretrieve(url, dest_name)


def rm(pattern: str, path: str, recursive: bool = False) -> None:
    """Remove files matching *pattern* inside *path*."""
    p = Path(path)
    glob_fn = p.rglob if recursive else p.glob
    for match in glob_fn(pattern):
        if match.is_file():
            match.unlink()


def load(path: str) -> str:
    """Return the text content of *path*."""
    return Path(path).read_text(encoding="utf-8")


def replace_in_file(*args, strict: bool = False, **kwargs) -> None:
    """Replace every occurrence of *search* with *replace* in a text file.

    Accepts both ``replace_in_file(path, search, replace)`` and the
    Conan-style ``replace_in_file(recipe, path, search, replace)`` forms.
    The *strict* keyword argument is accepted but ignored (for compatibility).
    """
    if len(args) == 4:
        _, path, search, replace = args
    elif len(args) == 3:
        path, search, replace = args
    else:
        raise TypeError(f"replace_in_file() takes 3 or 4 positional arguments but {len(args)} were given")
    p = Path(path)
    p.write_text(p.read_text(encoding="utf-8").replace(search, replace), encoding="utf-8")


def save(path: str, content: str, append: bool = False) -> None:
    """Write *content* to *path* (UTF-8). Creates parent directories as needed."""
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    mode = "a" if append else "w"
    with open(p, mode, encoding="utf-8") as f:
        f.write(content)


def rename(src: str, dst: str) -> None:
    """Rename/move *src* to *dst*, creating parent directories as needed."""
    src_p = Path(src)
    dst_p = Path(dst)
    dst_p.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(src_p), dst_p)


def apply_patches(recipe: "_RecipeLike") -> None:
    """Apply patches listed in ``data.yml`` ``patches`` section to ``source_folder``.

    The data.yml patches section is expected in the form::

        patches:
          "1.2.3":
            - patches/fix-something.patch
    """
    import patch_ng  # type: ignore[import-untyped]

    patches_raw: Any = recipe.thirdparty_data.get("patches", {})
    if not isinstance(patches_raw, dict):
        return
    patches_by_ver = cast("dict[str, Any]", patches_raw)

    ver_patches_raw: Any = patches_by_ver.get(recipe.version, [])
    if not isinstance(ver_patches_raw, list):
        return
    version_patches = cast("list[Any]", ver_patches_raw)

    recipe_dir = Path(recipe.recipe_folder)
    src_dir = Path(recipe.source_folder)

    for entry in version_patches:
        patch_path = recipe_dir / str(entry)
        if not patch_path.exists():
            raise RuntimeError(f"Patch file not found: {patch_path}")
        print(f"[thirdparty] Applying patch: {patch_path.name}")
        pset = patch_ng.fromfile(str(patch_path))  # type: ignore[no-untyped-call]
        if not pset:
            raise RuntimeError(f"Failed to parse patch: {patch_path}")
        if not pset.apply(strip=0, root=str(src_dir)):  # type: ignore[no-untyped-call]
            raise RuntimeError(f"Failed to apply patch: {patch_path.name}")


def collect_libs(recipe: "_RecipeLike", folder: str = "lib") -> list[str]:
    """Return a list of library names found in ``<package_folder>/<folder>``.

    Strips the leading ``lib`` prefix on non-Windows platforms and drops
    file extensions (.a, .so, .dylib, .lib).
    """
    lib_dir = Path(recipe.package_folder) / folder
    if not lib_dir.is_dir():
        return []
    libs: list[str] = []
    for f in sorted(lib_dir.iterdir()):
        if not f.is_file():
            continue
        if f.suffix not in (".a", ".so", ".dylib", ".lib"):
            continue
        stem = f.stem
        if not sys.platform.startswith("win") and stem.startswith("lib"):
            stem = stem[3:]
        libs.append(stem)
    return libs


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _verify_sha256(path: Path, expected: str) -> None:
    if expected.startswith("sha256:"):
        expected = expected[7:]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest.lower() != expected.lower():
        raise RuntimeError(
            f"SHA-256 mismatch for {path.name}:\n"
            f"  expected: {expected}\n"
            f"  actual:   {digest}"
        )


def _detect_archive_type(path: Path) -> str | None:
    """Return a normalised extension based on magic bytes, or None."""
    header = path.read_bytes()[:8]
    if header[:4] == b"PK\x03\x04":
        return ".zip"
    if header[:2] == b"\x1f\x8b":
        return ".tar.gz"
    if header[:3] == b"BZh":
        return ".tar.bz2"
    if header[:6] == b"\xfd7zXZ\x00":
        return ".tar.xz"
    return None


def _extract(archive: Path, dest: Path, filename: str) -> None:
    if filename.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
    elif any(
        filename.endswith(ext)
        for ext in (".tar.gz", ".tgz", ".tar.bz2", ".tar.xz", ".tar.zst")
    ):
        with tarfile.open(archive) as tf:
            if sys.version_info >= (3, 12):
                tf.extractall(dest, filter="data")  # type: ignore[call-arg]
            else:
                tf.extractall(dest)
    else:
        # Fall back to magic-byte detection for files with unhelpful names
        detected = _detect_archive_type(archive)
        if detected == ".zip":
            with zipfile.ZipFile(archive) as zf:
                zf.extractall(dest)
        elif detected in (".tar.gz", ".tar.bz2", ".tar.xz"):
            with tarfile.open(archive) as tf:
                if sys.version_info >= (3, 12):
                    tf.extractall(dest, filter="data")  # type: ignore[call-arg]
                else:
                    tf.extractall(dest)
        else:
            raise RuntimeError(f"Unsupported archive format: {filename}")
