#!/usr/bin/env python3
"""_cleanup_v1_dead_code.py — Strip dead Conan v1 compatibility shims from recipes.

Removes five categories of confirmed no-ops from all recipes/*/recipe.py:

  A. .names["cmake_find_package*"] / .filenames[...] / .build_modules["cmake_find_package*"]
     subscript assignments  (MockInfoProperty — silently discarded)

  B. self.env_info.* assignments  (MockInfoProperty — silently discarded)

  C. _settings_build property definition + usages
     (always equals self.settings; build.py sets settings_build = settings)

  D. no_copy_source = True  class attribute  (not read by the build runner)

  E. Standalone TODO comment lines referencing conan v2 / cmake_find_package*

Usage::

    python tools/_cleanup_v1_dead_code.py [--dry-run] [recipe ...]

    --dry-run    Print what would change without writing files.
    recipe ...   Optional list of recipe names to restrict processing.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Patterns: lines to drop entirely
# ---------------------------------------------------------------------------

# A — MockInfoProperty subscript-assignment lines (NOT set_property)
_DROP_PATTERNS: list[re.Pattern] = [
    # .names["cmake_find_package*"] = ...   (any cpp_info or component)
    re.compile(r'\.names\[["\'](cmake_find_package|cmake_find_package_multi|pkg_config)["\']'),
    # .filenames["cmake_find_package*"] = ...
    re.compile(r'\.filenames\[["\'](cmake_find_package|cmake_find_package_multi)["\']'),
    # .build_modules["cmake_find_package*"] = ...
    re.compile(r'\.build_modules\[["\'](cmake_find_package|cmake_find_package_multi)["\']'),

    # B — env_info assignments
    re.compile(r'\bself\.env_info\.'),

    # D — no_copy_source class attribute
    re.compile(r'^\s*no_copy_source\s*=\s*True\s*$'),

    # E — standalone TODO comments about conan v2 / cmake_find_package* / global scope
    re.compile(
        r'^\s*#\s*TODO:.*'
        r'(?:conan v2|cmake_find_package|global scope|Remove for Conan)',
        re.IGNORECASE,
    ),
]

# ---------------------------------------------------------------------------
# _settings_build property block (2 or 3 lines)
# ---------------------------------------------------------------------------

# Matches an optional inline TODO comment, then the @property + def + return block.
# The return line may contain extra spaces.
# Group 1: optional indentation of the @property line (for alignment check).
_SETTINGS_BUILD_BLOCK_RE = re.compile(
    r'(?:[ \t]*#[^\n]*Remove for Conan[^\n]*\n)?'   # optional preceding TODO comment
    r'([ \t]*)@property\n'
    r'\1[ \t]+def _settings_build\(self\):?\n'
    r'(?:\1[ \t]+#[^\n]*\n)?'                        # optional inline comment (e.g. autoconf)
    r'\1[ \t]+return getattr\(self,\s*["\']settings_build["\'],\s*self\.settings\)\n?',
    re.MULTILINE,
)

# C — replace usages
_SETTINGS_BUILD_USAGE_RE = re.compile(r'\bself\._settings_build\b')

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _transform(source: str) -> tuple[str, list[str]]:
    """Apply all transformations to *source*; return (new_source, description_lines)."""
    changes: list[str] = []

    # --- Pass 1: Remove _settings_build property blocks ---
    def _remove_block(m: re.Match) -> str:
        changes.append("  removed _settings_build property block")
        return ""
    source = _SETTINGS_BUILD_BLOCK_RE.sub(_remove_block, source)

    # --- Pass 2: Replace _settings_build usages ---
    count = len(_SETTINGS_BUILD_USAGE_RE.findall(source))
    if count:
        source = _SETTINGS_BUILD_USAGE_RE.sub("self.settings", source)
        changes.append(f"  replaced {count} self._settings_build -> self.settings")

    # --- Pass 3: Drop dead lines ---
    lines = source.splitlines(keepends=True)
    kept: list[str] = []
    dropped = 0
    for line in lines:
        if any(p.search(line) for p in _DROP_PATTERNS):
            dropped += 1
        else:
            kept.append(line)
    if dropped:
        changes.append(f"  dropped {dropped} dead line(s)")
    source = "".join(kept)

    # --- Pass 4: Fix empty compound-statement bodies (add `pass`) ---
    fixed_source, n_fixed = _fix_empty_blocks(source)
    if n_fixed:
        source = fixed_source
        changes.append(f"  added pass to {n_fixed} empty block(s)")

    # --- Pass 5: Collapse 3+ consecutive blank lines to 2 ---
    source = re.sub(r'\n{3,}', '\n\n', source)

    return source, changes


# Compound statement headers whose bodies may become empty after dead-line removal.
_COMPOUND_HDR_RE = re.compile(
    r'^(?P<indent>\s+)'
    r'(?:if|elif|else|for|while|with|try|except|finally)\b'
    r'.*:\s*(?:#.*)?$'
)


def _fix_empty_blocks(source: str) -> tuple[str, int]:
    """Insert `pass` wherever a compound-statement body was emptied.

    Scans line-by-line; when a compound header is followed by a line at the
    same or lesser indent (skipping blanks/comments), the block is empty and
    needs a `pass` statement.
    """
    lines = source.splitlines(keepends=True)
    result: list[str] = []
    n_fixed = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        m = _COMPOUND_HDR_RE.match(line)
        if m:
            header_indent = len(m.group("indent"))
            # Find next non-blank, non-comment line
            j = i + 1
            while j < len(lines):
                s = lines[j].strip()
                if s and not s.startswith('#'):
                    break
                j += 1
            # Check if the block body is missing (next real line ≤ header indent)
            if j >= len(lines) or (len(lines[j]) - len(lines[j].lstrip())) <= header_indent:
                result.append(line)
                result.append(' ' * (header_indent + 4) + 'pass\n')
                n_fixed += 1
                i += 1
                continue
        result.append(line)
        i += 1
    return ''.join(result), n_fixed


def _process_recipe(path: Path, dry_run: bool) -> bool:
    """Return True if the file was (or would be) changed."""
    original = path.read_text(encoding="utf-8")
    new_source, changes = _transform(original)

    if new_source == original:
        return False

    print(f"{'[DRY] ' if dry_run else ''}  {path.relative_to(path.parents[2])}")
    for c in changes:
        print(c)

    if not dry_run:
        path.write_text(new_source, encoding="utf-8")
    return True


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview changes without writing files")
    parser.add_argument("recipes", nargs="*", metavar="recipe",
                        help="Recipe names to process (default: all)")
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parents[1]
    recipes_root = repo_root / "recipes"

    if not recipes_root.is_dir():
        print(f"ERROR: recipes/ not found at {recipes_root}", file=sys.stderr)
        sys.exit(1)

    if args.recipes:
        targets = [recipes_root / name / "recipe.py" for name in args.recipes]
        missing = [p for p in targets if not p.exists()]
        if missing:
            for m in missing:
                print(f"ERROR: {m} not found", file=sys.stderr)
            sys.exit(1)
    else:
        targets = sorted(recipes_root.glob("*/recipe.py"))

    if args.dry_run:
        print(f"DRY RUN — {len(targets)} recipe(s) to inspect")
    else:
        print(f"Processing {len(targets)} recipe(s)")

    changed = 0
    for path in targets:
        if _process_recipe(path, dry_run=args.dry_run):
            changed += 1

    action = "would change" if args.dry_run else "changed"
    print(f"\n{changed}/{len(targets)} file(s) {action}.")


if __name__ == "__main__":
    main()
