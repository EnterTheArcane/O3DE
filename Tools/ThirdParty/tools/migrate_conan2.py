#!/usr/bin/env python3
"""migrate_conan2.py — Execute the conan2 → thirdparty migration.

Phase 1: Delete non-internal deletable modules from src/conan2/
Phase 2: Move remaining non-internal modules to src/thirdparty/ with import rewriting
Phase 3: Update src/conan2/internal/ imports to point at thirdparty.*
Phase 4: Update remaining src/thirdparty/ files that still reference conan2.{errors,api,tools}
Phase 5: Merge thirdparty-specific extras back into merged __init__ files
Phase 6: Clean up moved source files from src/conan2/
Phase 7: Simplify src/conan2/__init__.py to a minimal package marker

After this script, src/conan2/ contains only __init__.py + internal/.
"""
from __future__ import annotations

import re
import shutil
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parent.parent
CONAN2 = ROOT / "src" / "conan2"
THIRDPARTY = ROOT / "src" / "thirdparty"

# ---------------------------------------------------------------------------
# Import rewriting
# ---------------------------------------------------------------------------

# Replace  conan2.<errors|api|tools>  →  thirdparty.<errors|api|tools>
# Must NOT touch conan2.internal (which stays in place)
_IMPORT_RE = re.compile(r"\bconan2\.(errors|api|tools)\b")


def rewrite_imports(text: str) -> str:
    return _IMPORT_RE.sub(lambda m: f"thirdparty.{m.group(1)}", text)


def process_file(path: Path) -> bool:
    """Rewrite imports in *path* in-place.  Returns True if the file changed."""
    text = path.read_text(encoding="utf-8", errors="replace")
    new_text = rewrite_imports(text)
    if new_text != text:
        path.write_text(new_text, encoding="utf-8")
        return True
    return False


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def _delete(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
        print(f"  [del dir ] {path.relative_to(ROOT)}")
    elif path.is_file():
        path.unlink()
        print(f"  [del file] {path.relative_to(ROOT)}")


def _copy_py(src: Path, dst: Path) -> None:
    """Copy src → dst, rewriting imports.  Creates parent dirs as needed."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    text = src.read_text(encoding="utf-8", errors="replace")
    text = rewrite_imports(text)
    dst.write_text(text, encoding="utf-8")


def _append_if_missing(path: Path, lines: list[str]) -> None:
    """Append *lines* to *path* if their key symbol isn't already present."""
    text = path.read_text(encoding="utf-8", errors="replace")
    additions = [ln for ln in lines if ln.split(" import ")[-1].strip() not in text]
    if additions:
        path.write_text(text.rstrip() + "\n" + "\n".join(additions) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
# Phase 1 — Delete non-internal deletable directories and files
# ---------------------------------------------------------------------------

print("\n=== Phase 1: Deletions ===")

_DELETE_DIRS = [
    "cli",
    "test",
    "cps",
    "api/subapi",
    "tools/android",
    "tools/ros",
    "tools/qbs",
    "tools/sbom",
    "tools/scons",
    "tools/system",
    "tools/layout",
    "tools/cps",
]

_DELETE_FILES = [
    "api/conan_api.py",
    "api/input.py",
]

for rel in _DELETE_DIRS:
    _delete(CONAN2 / rel)

for rel in _DELETE_FILES:
    _delete(CONAN2 / rel)

# ---------------------------------------------------------------------------
# Phase 2 — Move non-internal KEEP files to thirdparty
# ---------------------------------------------------------------------------

print("\n=== Phase 2: Move to thirdparty ===")


def _should_move(path: Path) -> bool:
    """True for non-internal .py files in src/conan2/ (excluding the root __init__.py)."""
    if "__pycache__" in path.parts:
        return False
    if path.suffix != ".py":
        return False
    rel = path.relative_to(CONAN2)
    if rel == Path("__init__.py"):
        return False  # keep as package marker
    if "internal" in rel.parts:
        return False  # internal stays
    return True


# Save thirdparty-specific extras from thin-wrapper __init__ files
# before they are overwritten by the moved conan2 source.
_THIRDPARTY_EXTRAS: dict[str, list[str]] = {}

_WRAPPER_FILES = {
    "tools/files/__init__.py": [
        "from thirdparty._host.patches import apply_patches, export_conandata_patches",
    ],
    "tools/gnu/__init__.py": [
        "from thirdparty.tools.scm.gnu import GnuFtp",
    ],
    "tools/scm/__init__.py": [
        "from thirdparty.tools.scm.github import GithubRepository",
        "from thirdparty.tools.scm.gitlab import GitlabRepository",
        "from thirdparty.tools.scm.google import GoogleSourceRepository",
        "from thirdparty.tools.scm.bitbucket import BitbucketRepository",
        "from thirdparty.tools.scm.gnu import GnuFtp",
    ],
}

# Capture the alias block from thirdparty/errors.py before it's overwritten
_errors_tp = THIRDPARTY / "errors.py"
_errors_alias_lines: list[str] = []
if _errors_tp.exists():
    for line in _errors_tp.read_text(encoding="utf-8").splitlines():
        # Keep lines that aren't the thin-wrapper import
        if not line.startswith("from conan2.errors import") and not line.startswith("import conan2.errors"):
            _errors_alias_lines.append(line)
    # Strip leading blank lines from the saved block
    while _errors_alias_lines and not _errors_alias_lines[0].strip():
        _errors_alias_lines.pop(0)

moved_count = 0
for src_file in sorted(CONAN2.rglob("*.py")):
    if not _should_move(src_file):
        continue
    rel = src_file.relative_to(CONAN2)
    dst_file = THIRDPARTY / rel
    _copy_py(src_file, dst_file)
    moved_count += 1

print(f"  {moved_count} files copied to src/thirdparty/")

# ---------------------------------------------------------------------------
# Phase 3 — Rewrite imports in src/conan2/internal/
# ---------------------------------------------------------------------------

print("\n=== Phase 3: Update conan2.internal imports ===")

internal_changed = 0
for py_file in sorted((CONAN2 / "internal").rglob("*.py")):
    if "__pycache__" in py_file.parts:
        continue
    if process_file(py_file):
        internal_changed += 1

print(f"  {internal_changed} conan2/internal files updated")

# ---------------------------------------------------------------------------
# Phase 4 — Rewrite remaining conan2.{errors,api,tools} refs in thirdparty
# ---------------------------------------------------------------------------

print("\n=== Phase 4: Update remaining thirdparty imports ===")

tp_changed = 0
for py_file in sorted(THIRDPARTY.rglob("*.py")):
    if "__pycache__" in py_file.parts:
        continue
    if "_conan" in py_file.parts:
        continue  # vendored copy — skip
    if process_file(py_file):
        tp_changed += 1

print(f"  {tp_changed} thirdparty files updated")

# ---------------------------------------------------------------------------
# Phase 5 — Merge thirdparty-specific extras
# ---------------------------------------------------------------------------

print("\n=== Phase 5: Merge thirdparty extras ===")

# errors.py — append alias definitions preserved from old thin wrapper
_errors_tp_new = THIRDPARTY / "errors.py"
if _errors_alias_lines and _errors_tp_new.exists():
    current = _errors_tp_new.read_text(encoding="utf-8")
    # Only append lines not already present
    to_add = [ln for ln in _errors_alias_lines
              if ln.strip() and ln.strip() not in current]
    if to_add:
        _errors_tp_new.write_text(
            current.rstrip() + "\n\n" + "\n".join(_errors_alias_lines).rstrip() + "\n",
            encoding="utf-8",
        )
        print(f"  merged alias block into thirdparty/errors.py")

# tools/files/__init__.py, tools/gnu/__init__.py, tools/scm/__init__.py
for rel_path, extra_lines in _WRAPPER_FILES.items():
    target = THIRDPARTY / rel_path
    if target.exists():
        _append_if_missing(target, extra_lines)
        print(f"  merged extras into thirdparty/{rel_path}")

# ---------------------------------------------------------------------------
# Phase 6 — Remove moved source files from src/conan2/
# ---------------------------------------------------------------------------

print("\n=== Phase 6: Clean up src/conan2 sources ===")

removed_count = 0
for src_file in sorted(CONAN2.rglob("*.py")):
    if not _should_move(src_file):
        continue
    src_file.unlink()
    removed_count += 1

print(f"  {removed_count} source files removed from src/conan2/")

# Remove now-empty directories within src/conan2/ (excluding internal/)
for d in sorted(CONAN2.rglob("*"), reverse=True):
    if not d.is_dir():
        continue
    if "internal" in d.relative_to(CONAN2).parts:
        continue
    if d == CONAN2:
        continue
    if "__pycache__" in d.parts:
        shutil.rmtree(d)
        continue
    try:
        d.rmdir()  # succeeds only if empty
        print(f"  removed empty dir {d.relative_to(ROOT)}")
    except OSError:
        pass

# ---------------------------------------------------------------------------
# Phase 7 — Simplify src/conan2/__init__.py
# ---------------------------------------------------------------------------

print("\n=== Phase 7: Simplify conan2/__init__.py ===")

(CONAN2 / "__init__.py").write_text(
    "# conan2 package — retained as the namespace package for conan2.internal.\n"
    "# All public API has moved to thirdparty.*\n",
    encoding="utf-8",
)
print("  src/conan2/__init__.py simplified")

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

print("\n=== Migration complete ===")
remaining = list(CONAN2.rglob("*.py"))
non_pycache = [p for p in remaining if "__pycache__" not in p.parts]
print(f"  src/conan2/ now contains {len(non_pycache)} .py files")
print(f"  (should be 1 __init__.py + all of internal/)")
