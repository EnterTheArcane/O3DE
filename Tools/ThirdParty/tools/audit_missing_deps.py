#!/usr/bin/env python3
"""
audit_missing_deps.py — Find transitive dependencies our recipes will pull in
from conan-center-index that we don't already ship under ``recipes/``.

This is a static, AST-only analysis: nothing is imported and no network is
touched. We walk each existing recipe's matching CCI recipe, extract every
``self.requires(...)``, ``self.tool_requires(...)``, and
``self.test_requires(...)`` call, climb the enclosing ``if`` chain to capture
the gating expression, and then recurse into any unfamiliar dependency by
looking up *its* CCI recipe.

Each missing dependency is tagged with the conditions under which it would be
pulled in, so platform-only or option-gated deps stand out from unconditional
ones.

Usage:
    python tools/audit_missing_deps.py
    python tools/audit_missing_deps.py --json > missing.json
    python tools/audit_missing_deps.py --include-host-tools
    python tools/audit_missing_deps.py --cci-root D:/OpenSource/conan-center-index

The CCI root defaults to ``../../OpenSource/conan-center-index`` relative to
this script (matching the workspace layout).
"""
from __future__ import annotations

import argparse
import ast
import json
import sys
from collections import defaultdict, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
RECIPES_DIR = REPO_ROOT / "recipes"


def _default_cci_root() -> Path:
    """Find a conan-center-index checkout near the workspace, fall back to D:."""
    candidates = [
        Path(r"D:/OpenSource/conan-center-index"),
        REPO_ROOT.parent / "conan-center-index",
        Path.home() / "OpenSource" / "conan-center-index",
    ]
    for c in candidates:
        if (c / "recipes").is_dir():
            return c
    return candidates[0]


DEFAULT_CCI_ROOT = _default_cci_root()

# Build tools that come from the host PATH, not from packages we build.
# tool_requires() entries matching these are dropped silently when the
# --include-host-tools flag is not set. Matches by exact package name.
HOST_TOOLS_ALLOWLIST = frozenset({
    "cmake",
    "ninja",
    "meson",
    "pkgconf",
    "pkg-config",
    "autoconf",
    "autoconf-archive",
    "automake",
    "libtool",
    "m4",
    "msys2",
    "mingw-w64",
    "gnu-config",
    "gettext",
    "bison",
    "flex",
    "gperf",
    "nasm",
    "yasm",
    "perl",
    "strawberryperl",
    "ruby",
    "ruby_installer",
    "make",
    "patch",
    "sed",
    "grep",
    "diff",
    "tar",
    "zip",
    "unzip",
    "doxygen",
    "swig",
    "wayland",  # protocol scanner only — wayland-scanner; runtime wayland is host package
    "b2",
    "premake",
    "scons",
    "ply",
    "jom",
    "qbs",
})


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class Edge:
    """A single requires() / tool_requires() / test_requires() call."""

    from_pkg: str
    to_pkg: str
    context: str          # "host" | "build" | "test"
    conditions: list[str] = field(default_factory=list)


@dataclass
class MissingPkg:
    name: str
    edges: list[Edge] = field(default_factory=list)

    def tags(self) -> list[str]:
        """Reduce the conditions across all edges into a small set of tags."""
        all_conds = [c for e in self.edges for c in e.conditions]
        contexts = {e.context for e in self.edges}
        tags: list[str] = []
        if not all_conds and "host" in contexts:
            tags.append("unconditional")
        if "build" in contexts and "host" not in contexts:
            tags.append("build-tool-only")
        if "test" in contexts and contexts <= {"test"}:
            tags.append("test-only")

        os_tags = _gating_os(all_conds)
        if os_tags:
            tags.append("os=" + "|".join(sorted(os_tags)))

        option_tags = _gating_options(all_conds)
        if option_tags:
            tags.append("opt=" + ",".join(sorted(option_tags)))

        return tags or ["conditional"]


# ---------------------------------------------------------------------------
# AST helpers
# ---------------------------------------------------------------------------

def _build_parent_map(tree: ast.AST) -> dict[ast.AST, ast.AST]:
    parents: dict[ast.AST, ast.AST] = {}
    for node in ast.walk(tree):
        for child in ast.iter_child_nodes(node):
            parents[child] = node
    return parents


def _enclosing_conditions(node: ast.AST, parents: dict[ast.AST, ast.AST]) -> list[str]:
    """Walk up from *node* collecting `if <test>:` / `elif`/`else` branches."""
    conds: list[str] = []
    current: Optional[ast.AST] = parents.get(node)
    child: ast.AST = node
    while current is not None:
        if isinstance(current, ast.If):
            test_src = _safe_unparse(current.test)
            # Determine whether the child sits in body or orelse.
            in_body = any(child is n or _contains(n, child) for n in current.body)
            if in_body:
                conds.append(test_src)
            else:
                conds.append(f"not ({test_src})")
        child = current
        current = parents.get(current)
    conds.reverse()
    return conds


def _contains(haystack: ast.AST, needle: ast.AST) -> bool:
    for n in ast.walk(haystack):
        if n is needle:
            return True
    return False


def _safe_unparse(node: ast.AST) -> str:
    try:
        return ast.unparse(node)
    except Exception:  # pragma: no cover — defensive
        return "<unparseable>"


def _string_from(node: ast.AST) -> Optional[str]:
    """Extract a literal string from a Call's first positional argument."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.JoinedStr):
        # f-string — try the leading literal segment only.
        for v in node.values:
            if isinstance(v, ast.Constant) and isinstance(v.value, str):
                return v.value
            return None
    return None


def _pkg_name_from_ref(ref: str) -> Optional[str]:
    """Given a conan reference like 'fmt/[>=10]', return the package name."""
    ref = ref.strip()
    if not ref:
        return None
    # Cut off any version specifier.
    pkg = ref.split("/", 1)[0]
    # Strip any inline conditional remainder.
    pkg = pkg.strip()
    if not pkg or any(c in pkg for c in "{}<>$"):
        return None
    return pkg


# ---------------------------------------------------------------------------
# Conanfile parsing
# ---------------------------------------------------------------------------

REQ_METHODS = {
    "requires": "host",
    "tool_requires": "build",
    "test_requires": "test",
}


def extract_edges(pkg: str, recipe: Path) -> list[Edge]:
    """Return every requires/tool_requires/test_requires edge in *recipe*."""
    try:
        tree = ast.parse(recipe.read_text(encoding="utf-8"))
    except (OSError, SyntaxError):
        return []

    parents = _build_parent_map(tree)
    edges: list[Edge] = []

    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        func = node.func
        if not isinstance(func, ast.Attribute):
            continue
        method = func.attr
        if method not in REQ_METHODS:
            continue
        # Require `self.<method>(...)`
        if not (isinstance(func.value, ast.Name) and func.value.id == "self"):
            continue
        if not node.args:
            continue

        ref_str = _string_from(node.args[0])
        if ref_str is None:
            continue
        dep_pkg = _pkg_name_from_ref(ref_str)
        if dep_pkg is None:
            continue

        edges.append(Edge(
            from_pkg=pkg,
            to_pkg=dep_pkg,
            context=REQ_METHODS[method],
            conditions=_enclosing_conditions(node, parents),
        ))

    # Class-level `requires = "a/1.0"` or tuple form.
    for cls in (n for n in tree.body if isinstance(n, ast.ClassDef)):
        for item in cls.body:
            if not isinstance(item, ast.Assign):
                continue
            for target in item.targets:
                if not isinstance(target, ast.Name):
                    continue
                attr = target.id
                if attr not in {"requires", "tool_requires", "test_requires"}:
                    continue
                context = REQ_METHODS[attr]
                values: list[ast.AST] = []
                if isinstance(item.value, (ast.Tuple, ast.List)):
                    values = list(item.value.elts)
                else:
                    values = [item.value]
                for v in values:
                    ref_str = _string_from(v)
                    if ref_str is None:
                        continue
                    dep_pkg = _pkg_name_from_ref(ref_str)
                    if dep_pkg is None:
                        continue
                    edges.append(Edge(
                        from_pkg=pkg,
                        to_pkg=dep_pkg,
                        context=context,
                    ))

    return edges


# ---------------------------------------------------------------------------
# CCI lookup
# ---------------------------------------------------------------------------

def find_cci_recipe(cci_root: Path, pkg: str) -> Optional[Path]:
    """Best-effort: locate the most reasonable recipe.py for *pkg*."""
    pkg_dir = cci_root / "recipes" / pkg
    if not pkg_dir.is_dir():
        return None

    # Prefer .../all/recipe.py
    all_cf = pkg_dir / "all" / "recipe.py"
    if all_cf.is_file():
        return all_cf

    # Otherwise pick the lexicographically largest sub-folder containing one.
    candidates = sorted(
        (p / "recipe.py" for p in pkg_dir.iterdir() if p.is_dir()),
        key=lambda p: p.parent.name,
        reverse=True,
    )
    for cf in candidates:
        if cf.is_file():
            return cf
    return None


# ---------------------------------------------------------------------------
# Condition tag distillation
# ---------------------------------------------------------------------------

def _gating_os(conditions: Iterable[str]) -> set[str]:
    """Pick up obvious OS gating like ``self.settings.os == "Windows"``."""
    seen: set[str] = set()
    for cond in conditions:
        for os_name in ("Windows", "Linux", "Macos", "iOS", "Android",
                        "FreeBSD", "tvOS", "watchOS", "visionOS"):
            if f'"{os_name}"' in cond or f"'{os_name}'" in cond:
                seen.add(os_name)
        if "is_apple_os" in cond:
            seen.add("Apple")
        if "self.settings.os in" in cond:
            # Capture lists like `self.settings.os in ["Linux", "FreeBSD"]`
            for os_name in ("Linux", "Windows", "Macos", "FreeBSD", "Android"):
                if os_name in cond:
                    seen.add(os_name)
    return seen


def _gating_options(conditions: Iterable[str]) -> set[str]:
    """Pick up option gating like ``self.options.with_foo``."""
    import re
    seen: set[str] = set()
    pattern = re.compile(r"self\.options\.(get_safe\([\"']([^\"']+)[\"']\)|([A-Za-z_][A-Za-z0-9_]*))")
    for cond in conditions:
        for m in pattern.finditer(cond):
            opt = m.group(2) or m.group(3)
            if opt and opt not in {"get_safe"}:
                seen.add(opt)
    return seen


# ---------------------------------------------------------------------------
# Walker
# ---------------------------------------------------------------------------

def audit(
    have: set[str],
    cci_root: Path,
    include_host_tools: bool,
    full_transitive: bool = False,
) -> dict[str, MissingPkg]:
    """Walk requirements from the existing set; return missing packages."""
    missing: dict[str, MissingPkg] = {}
    queue: deque[str] = deque(have)
    visited: set[str] = set()

    while queue:
        pkg = queue.popleft()
        if pkg in visited:
            continue
        visited.add(pkg)

        recipe = find_cci_recipe(cci_root, pkg)
        if recipe is None:
            continue

        for edge in extract_edges(pkg, recipe):
            dep = edge.to_pkg
            if dep in have:
                # Already shipped — keep walking through it.
                if dep not in visited:
                    queue.append(dep)
                continue
            if (
                not include_host_tools
                and edge.context == "build"
                and dep in HOST_TOOLS_ALLOWLIST
            ):
                continue
            entry = missing.setdefault(dep, MissingPkg(name=dep))
            entry.edges.append(edge)
            if full_transitive and dep not in visited:
                queue.append(dep)

    return missing


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def render_text(missing: dict[str, MissingPkg], label: str = "") -> str:
    if not missing:
        header = f"=== {label} ==="  if label else ""
        return f"{header}\nNone."

    rows: list[tuple[str, str, str]] = []
    for name in sorted(missing):
        entry = missing[name]
        tags = ",".join(entry.tags())
        required_by = ", ".join(sorted({e.from_pkg for e in entry.edges}))
        rows.append((name, tags, required_by))

    width_name = max(len(r[0]) for r in rows) + 2
    width_tags = max(len(r[1]) for r in rows) + 2
    lines: list[str] = []
    if label:
        lines.append(f"=== {label} ===")
    lines.append(f"{'package'.ljust(width_name)}{'tags'.ljust(width_tags)}required-by")
    lines.append("-" * (width_name + width_tags + 12))
    for name, tags, required_by in rows:
        lines.append(f"{name.ljust(width_name)}{tags.ljust(width_tags)}{required_by}")
    lines.append("")
    lines.append(f"Total: {len(missing)}")
    return "\n".join(lines)


def render_json(missing: dict[str, MissingPkg]) -> str:
    payload = {}
    for name, entry in sorted(missing.items()):
        payload[name] = {
            "tags": entry.tags(),
            "edges": [
                {
                    "from": e.from_pkg,
                    "context": e.context,
                    "conditions": e.conditions,
                }
                for e in entry.edges
            ],
        }
    return json.dumps(payload, indent=2)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    parser.add_argument(
        "--cci-root",
        type=Path,
        default=DEFAULT_CCI_ROOT,
        help="Path to conan-center-index checkout (default: %(default)s).",
    )
    parser.add_argument(
        "--include-host-tools",
        action="store_true",
        help="Report tool_requires() on host build tools (cmake, ninja, ...).",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit JSON instead of a text table.",
    )
    parser.add_argument(
        "--full-transitive",
        action="store_true",
        help="Recurse into missing deps too (shows the complete closure, not just 1-step).",
    )
    args = parser.parse_args(argv)

    cci_root: Path = args.cci_root
    if not (cci_root / "recipes").is_dir():
        print(f"error: --cci-root {cci_root} does not contain a 'recipes/' directory",
              file=sys.stderr)
        return 2

    have = {p.name for p in RECIPES_DIR.iterdir() if p.is_dir()}
    if not have:
        print(f"error: no recipes found under {RECIPES_DIR}", file=sys.stderr)
        return 2

    # Sanity: warn about local recipes with no CCI counterpart.
    no_cci = sorted(p for p in have if find_cci_recipe(cci_root, p) is None)
    if no_cci and not args.json:
        print(f"# {len(no_cci)} local recipes have no CCI counterpart "
              f"(skipped during walk):", file=sys.stderr)
        for p in no_cci:
            print(f"#   {p}", file=sys.stderr)
        print("", file=sys.stderr)

    missing = audit(have, cci_root, args.include_host_tools,
                    full_transitive=args.full_transitive)

    # Partition into buildable (has CCI recipe) vs system-only.
    buildable = {k: v for k, v in missing.items()
                 if find_cci_recipe(cci_root, k) is not None}
    system_only = {k: v for k, v in missing.items()
                   if find_cci_recipe(cci_root, k) is None}

    if args.json:
        print(render_json(missing))
    else:
        print(render_text(buildable, label="BUILDABLE (has CCI recipe)"))
        if system_only:
            print()
            print(render_text(system_only, label="SYSTEM / NO CCI RECIPE (skip or handle manually)"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
