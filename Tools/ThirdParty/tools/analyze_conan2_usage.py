#!/usr/bin/env python3
"""analyze_conan2_usage.py — Static reachability analysis for conan2 modules.

Determines which conan2 modules are actually used (directly or transitively)
from src/thirdparty/, then cross-references with recipe imports to list which
symbols are actually needed from wildcard-imported modules.

Outputs a report showing:
  KEEP    — conan2 modules reachable from thirdparty
  DELETE  — conan2 modules with no path from thirdparty
  WILDCARD SYMBOLS — per wildcard-imported module, symbols actually used in recipes

Usage::

    python tools/analyze_conan2_usage.py
    python tools/analyze_conan2_usage.py --json
    python tools/analyze_conan2_usage.py --json --output report.json
    python tools/analyze_conan2_usage.py --by-subpackage
"""
from __future__ import annotations

import ast
import json
import sys
import textwrap
from collections import defaultdict, deque
from pathlib import Path
from typing import TypeAlias

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parent.parent
CONAN2_SRC = ROOT / "src" / "conan2"
THIRDPARTY_SRC = ROOT / "src" / "thirdparty"
RECIPES_DIR = ROOT / "recipes"

# Type alias for readability
ModuleName: TypeAlias = str


# ---------------------------------------------------------------------------
# Helpers: file ↔ module name
# ---------------------------------------------------------------------------

def file_to_module(py_file: Path, src_root: Path) -> ModuleName:
    """Convert an absolute .py path to a dotted module name relative to src_root."""
    rel = py_file.relative_to(src_root)
    parts = list(rel.with_suffix("").parts)
    if parts[-1] == "__init__":
        parts = parts[:-1]
    return ".".join(parts)


def module_to_file(module: ModuleName, src_root: Path) -> Path | None:
    """Find the .py file for a dotted module name (package init or plain module)."""
    rel = Path(*module.split("."))
    candidate_pkg = src_root / rel / "__init__.py"
    if candidate_pkg.exists():
        return candidate_pkg
    candidate_mod = src_root / rel.with_suffix(".py")
    if candidate_mod.exists():
        return candidate_mod
    return None


def is_init_file(py_file: Path) -> bool:
    return py_file.name == "__init__.py"


def package_of(py_file: Path, src_root: Path) -> ModuleName:
    """Return the package (containing directory as module name) for a .py file."""
    if is_init_file(py_file):
        return file_to_module(py_file, src_root)
    parent_init = py_file.parent / "__init__.py"
    if parent_init.exists():
        return file_to_module(parent_init, src_root)
    # Fallback: treat the file's directory as a namespace package
    rel = py_file.relative_to(src_root)
    parts = list(rel.parts[:-1])  # drop the filename
    return ".".join(parts) if parts else ""


def resolve_relative_import(
    package: ModuleName,
    level: int,
    from_module: str | None,
) -> ModuleName:
    """Resolve a relative import to an absolute module name.

    package   — module name of the package containing the importing file
    level     — number of leading dots  (1 = current package, 2 = parent, …)
    from_module — the string after the dots, or None for ``from . import X``
    """
    parts = package.split(".") if package else []
    # level 1 → stay in package; level 2 → parent of package; etc.
    base_parts = parts[: max(0, len(parts) - (level - 1))]
    if from_module:
        base_parts.extend(from_module.split("."))
    return ".".join(base_parts)


# ---------------------------------------------------------------------------
# AST import extraction
# ---------------------------------------------------------------------------

ImportEntry = tuple[ModuleName, list[str]]  # (resolved_module, [symbols_or_"*"])


def extract_conan2_imports(
    py_file: Path,
    src_root: Path,
    filter_prefix: str = "conan2",
) -> list[ImportEntry]:
    """Parse *py_file* and return all imports whose resolved module starts with
    *filter_prefix*.  Each entry is (module_name, list_of_symbols) where a
    wildcard ``import *`` is represented as the single element ``"*"``.
    """
    try:
        source = py_file.read_text(encoding="utf-8", errors="replace")
        tree = ast.parse(source, filename=str(py_file))
    except SyntaxError:
        return []

    pkg = package_of(py_file, src_root)
    results: list[ImportEntry] = []

    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom):
            if node.level == 0:
                # Absolute import
                if node.module and node.module.startswith(filter_prefix):
                    symbols = [alias.name for alias in node.names]
                    results.append((node.module, symbols))
            else:
                # Relative import
                mod_part = node.module  # may be None for ``from . import X``
                if mod_part is None:
                    # ``from . import foo, bar`` — each name may be a submodule
                    for alias in node.names:
                        resolved = resolve_relative_import(pkg, node.level, alias.name)
                        if resolved.startswith(filter_prefix):
                            results.append((resolved, ["*"]))
                else:
                    resolved = resolve_relative_import(pkg, node.level, mod_part)
                    if resolved.startswith(filter_prefix):
                        symbols = [alias.name for alias in node.names]
                        results.append((resolved, symbols))

        elif isinstance(node, ast.Import):
            for alias in node.names:
                if alias.name.startswith(filter_prefix):
                    # ``import conan2.tools.cmake`` — treat the whole module as used
                    results.append((alias.name, ["*"]))

    return results


# ---------------------------------------------------------------------------
# Step 1: Enumerate all conan2 modules
# ---------------------------------------------------------------------------

def build_all_conan2_modules() -> dict[ModuleName, Path]:
    """Map every conan2 module name → its .py file."""
    modules: dict[ModuleName, Path] = {}
    for py_file in sorted(CONAN2_SRC.rglob("*.py")):
        if "__pycache__" in py_file.parts:
            continue
        mod = file_to_module(py_file, ROOT / "src")
        modules[mod] = py_file
    return modules


# ---------------------------------------------------------------------------
# Step 2: Build conan2 internal import graph
# ---------------------------------------------------------------------------

def build_conan2_import_graph(
    all_modules: dict[ModuleName, Path],
) -> dict[ModuleName, set[ModuleName]]:
    """Return {module → set of conan2 modules it imports}."""
    graph: dict[ModuleName, set[ModuleName]] = defaultdict(set)
    for module_name, py_file in all_modules.items():
        for imported_module, _symbols in extract_conan2_imports(py_file, ROOT / "src"):
            if imported_module != module_name:
                graph[module_name].add(imported_module)
    return graph


# ---------------------------------------------------------------------------
# Step 3: Collect thirdparty → conan2 entry points
# ---------------------------------------------------------------------------

def collect_entry_points() -> dict[ModuleName, set[str]]:
    """Return {conan2_module → set of symbols} imported directly in thirdparty.

    A wildcard import is represented as {"*"}.
    """
    entry_points: dict[ModuleName, set[str]] = defaultdict(set)
    for py_file in sorted(THIRDPARTY_SRC.rglob("*.py")):
        if "__pycache__" in py_file.parts:
            continue
        if "_conan" in py_file.parts:
            # Vendored copy — skip (it's a copy of conan2 with rewritten imports)
            continue
        for imported_module, symbols in extract_conan2_imports(py_file, ROOT / "src"):
            entry_points[imported_module].update(symbols)
    return entry_points


# ---------------------------------------------------------------------------
# Step 4: BFS reachability
# ---------------------------------------------------------------------------

def compute_reachability(
    entry_points: dict[ModuleName, set[str]],
    import_graph: dict[ModuleName, set[ModuleName]],
    all_modules: dict[ModuleName, Path],
) -> tuple[set[ModuleName], set[ModuleName]]:
    """Return (reachable, deletable) sets of module names."""
    reachable: set[ModuleName] = set()
    queue: deque[ModuleName] = deque(entry_points.keys())

    while queue:
        module = queue.popleft()
        if module in reachable:
            continue
        reachable.add(module)
        for dep in import_graph.get(module, set()):
            if dep not in reachable:
                queue.append(dep)

    # Modules that exist as files but are never reached
    deletable = set(all_modules.keys()) - reachable
    return reachable, deletable


# ---------------------------------------------------------------------------
# Step 5: Recipe symbol audit
# ---------------------------------------------------------------------------

def build_thirdparty_wildcard_map() -> dict[str, ModuleName]:
    """Return {thirdparty_module → conan2_module} for wildcard re-exports.

    E.g. 'thirdparty.tools.cmake' → 'conan2.tools.cmake'
    """
    wildcard_map: dict[str, ModuleName] = {}
    for py_file in sorted(THIRDPARTY_SRC.rglob("*.py")):
        if "__pycache__" in py_file.parts:
            continue
        if "_conan" in py_file.parts:
            continue
        tp_module = file_to_module(py_file, ROOT / "src")
        for imported_module, symbols in extract_conan2_imports(py_file, ROOT / "src"):
            if "*" in symbols:
                wildcard_map[tp_module] = imported_module
    return wildcard_map


def audit_recipe_symbols(
    wildcard_map: dict[str, ModuleName],
) -> dict[ModuleName, set[str]]:
    """Return {conan2_module → set of symbols actually imported by recipes}.

    Only covers symbols imported via thirdparty wildcard re-exports.
    """
    # conan2_module → symbols actually used in recipes
    used_symbols: dict[ModuleName, set[str]] = defaultdict(set)

    for recipe_file in sorted(RECIPES_DIR.rglob("recipe.py")):
        # Recipes live outside src/; use RECIPES_DIR as the root so that
        # relative imports resolve correctly (recipes don't use them, but the
        # function needs a valid src_root for the package_of() call).
        for tp_module, symbols in extract_conan2_imports(
            recipe_file, RECIPES_DIR, filter_prefix="thirdparty"
        ):
            # Map the thirdparty module back to its conan2 source
            conan2_mod = wildcard_map.get(tp_module)
            if conan2_mod is None:
                continue
            if "*" in symbols:
                used_symbols[conan2_mod].add("*")
            else:
                used_symbols[conan2_mod].update(symbols)

    return used_symbols


def get_conan2_exported_symbols(module: ModuleName, all_modules: dict[ModuleName, Path]) -> set[str]:
    """Parse a conan2 module's __init__ or module file and collect its public names."""
    py_file = all_modules.get(module)
    if py_file is None:
        # Try to find the __init__.py for a package
        pkg_init = module + ".__init__" if not module.endswith("__init__") else module
        py_file = all_modules.get(pkg_init)
    if py_file is None:
        return set()

    try:
        source = py_file.read_text(encoding="utf-8", errors="replace")
        tree = ast.parse(source, filename=str(py_file))
    except SyntaxError:
        return set()

    # Check for explicit __all__
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id == "__all__":
                    if isinstance(node.value, (ast.List, ast.Tuple)):
                        return {
                            elt.s for elt in node.value.elts
                            if isinstance(elt, ast.Constant) and isinstance(elt.s, str)
                        }

    # Fall back: collect all top-level public names
    public_names: set[str] = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            if not node.name.startswith("_"):
                public_names.add(node.name)
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and not target.id.startswith("_"):
                    public_names.add(target.id)
        elif isinstance(node, ast.ImportFrom):
            for alias in node.names:
                name = alias.asname or alias.name
                if name != "*" and not name.startswith("_"):
                    public_names.add(name)
        elif isinstance(node, ast.Import):
            for alias in node.names:
                name = alias.asname or alias.name.split(".")[0]
                if not name.startswith("_"):
                    public_names.add(name)

    return public_names


# ---------------------------------------------------------------------------
# Step 6: Classify reachable modules by whether they were direct or transitive
# ---------------------------------------------------------------------------

def classify_reachable(
    entry_points: dict[ModuleName, set[str]],
    reachable: set[ModuleName],
    import_graph: dict[ModuleName, set[ModuleName]],
) -> tuple[set[ModuleName], set[ModuleName]]:
    """Return (direct_entry_points, transitive_only) sets."""
    direct = set(entry_points.keys()) & reachable
    transitive = reachable - direct
    return direct, transitive


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def _col(text: str, width: int) -> str:
    return text.ljust(width)


def print_text_report(
    all_modules: dict[ModuleName, Path],
    entry_points: dict[ModuleName, set[str]],
    reachable: set[ModuleName],
    deletable: set[ModuleName],
    recipe_used: dict[ModuleName, set[str]],
    wildcard_map: dict[str, ModuleName],
    by_subpackage: bool = False,
) -> None:
    total = len(all_modules)
    n_keep = len(reachable)
    n_delete = len(deletable)

    direct, transitive = classify_reachable(entry_points, reachable, {})

    print("=" * 72)
    print("CONAN2 USAGE ANALYSIS")
    print("=" * 72)
    print(f"  conan2 source root : {CONAN2_SRC.relative_to(ROOT)}")
    print(f"  thirdparty root    : {THIRDPARTY_SRC.relative_to(ROOT)}")
    print(f"  recipes root       : {RECIPES_DIR.relative_to(ROOT)}")
    print()
    print(f"  Total conan2 modules : {total}")
    print(f"  KEEP (reachable)     : {n_keep}  ({n_keep/total*100:.0f}%)")
    print(f"  DELETE (unreachable) : {n_delete}  ({n_delete/total*100:.0f}%)")
    print()

    # ------------------------------------------------------------------
    # Entry points
    # ------------------------------------------------------------------
    print("-" * 72)
    print("ENTRY POINTS  (thirdparty files that import directly from conan2)")
    print("-" * 72)
    for mod in sorted(entry_points):
        syms = sorted(entry_points[mod])
        sym_str = ", ".join(syms) if len(syms) <= 6 else ", ".join(syms[:6]) + f", … (+{len(syms)-6})"
        print(f"  {mod}")
        print(f"    symbols: [{sym_str}]")
    print()

    # ------------------------------------------------------------------
    # Wildcard symbol usage from recipes
    # ------------------------------------------------------------------
    if wildcard_map:
        print("-" * 72)
        print("WILDCARD IMPORT SYMBOL USAGE  (recipe → thirdparty → conan2)")
        print("-" * 72)
        # Group by conan2 module
        conan2_wildcards = {c2 for c2 in entry_points if "*" in entry_points[c2]}
        for c2_mod in sorted(conan2_wildcards):
            tp_mods = [tp for tp, c2 in wildcard_map.items() if c2 == c2_mod]
            tp_str = ", ".join(sorted(tp_mods))
            used = sorted(recipe_used.get(c2_mod, set()) - {"*"})
            print(f"  {c2_mod}")
            print(f"    re-exported via : {tp_str}")
            if used:
                print(f"    used in recipes : {', '.join(used)}")
            else:
                print(f"    used in recipes : (none detected — all via import *)")
            print()

    # ------------------------------------------------------------------
    # KEEP list
    # ------------------------------------------------------------------
    print("-" * 72)
    print(f"KEEP  ({n_keep} modules)")
    print("-" * 72)
    if by_subpackage:
        _print_by_subpackage(reachable, marker="  KEEP  ")
    else:
        for mod in sorted(reachable):
            tag = "[DIRECT]    " if mod in direct else "[transitive]"
            print(f"  {tag}  {mod}")
    print()

    # ------------------------------------------------------------------
    # DELETE list
    # ------------------------------------------------------------------
    print("-" * 72)
    print(f"DELETE  ({n_delete} modules — not reachable from thirdparty)")
    print("-" * 72)
    if by_subpackage:
        _print_by_subpackage(deletable, marker="  DELETE ")
    else:
        for mod in sorted(deletable):
            print(f"  {mod}")
    print()

    # ------------------------------------------------------------------
    # Top-level subpackage summary
    # ------------------------------------------------------------------
    print("-" * 72)
    print("TOP-LEVEL SUBPACKAGE SUMMARY")
    print("-" * 72)
    subpkg_keep: dict[str, int] = defaultdict(int)
    subpkg_total: dict[str, int] = defaultdict(int)
    for mod in all_modules:
        parts = mod.split(".")
        subpkg = ".".join(parts[:2]) if len(parts) > 1 else parts[0]
        subpkg_total[subpkg] += 1
        if mod in reachable:
            subpkg_keep[subpkg] += 1
    print(f"  {'subpackage':<40}  {'keep':>5}  {'total':>5}  {'pct':>5}")
    print(f"  {'-'*40}  {'-----':>5}  {'-----':>5}  {'-----':>5}")
    for subpkg in sorted(subpkg_total):
        k = subpkg_keep.get(subpkg, 0)
        t = subpkg_total[subpkg]
        pct = f"{k/t*100:.0f}%"
        flag = "  ← fully deletable" if k == 0 else ""
        print(f"  {subpkg:<40}  {k:>5}  {t:>5}  {pct:>5}{flag}")
    print()


def _print_by_subpackage(modules: set[ModuleName], marker: str = "") -> None:
    """Print a module set grouped by top-level conan2 subpackage."""
    grouped: dict[str, list[ModuleName]] = defaultdict(list)
    for mod in modules:
        parts = mod.split(".")
        subpkg = ".".join(parts[:2]) if len(parts) > 1 else parts[0]
        grouped[subpkg].append(mod)
    for subpkg in sorted(grouped):
        mods = sorted(grouped[subpkg])
        print(f"  [{subpkg}]  ({len(mods)} modules)")
        for m in mods:
            print(f"    {m}")


def build_json_report(
    all_modules: dict[ModuleName, Path],
    entry_points: dict[ModuleName, set[str]],
    reachable: set[ModuleName],
    deletable: set[ModuleName],
    recipe_used: dict[ModuleName, set[str]],
    wildcard_map: dict[str, ModuleName],
) -> dict:
    direct, transitive = classify_reachable(entry_points, reachable, {})

    def _sym_list(s: set[str]) -> list[str]:
        return sorted(s)

    return {
        "summary": {
            "total_conan2_modules": len(all_modules),
            "keep_count": len(reachable),
            "delete_count": len(deletable),
            "keep_pct": round(len(reachable) / len(all_modules) * 100, 1),
            "delete_pct": round(len(deletable) / len(all_modules) * 100, 1),
        },
        "entry_points": {
            mod: _sym_list(syms) for mod, syms in sorted(entry_points.items())
        },
        "wildcard_symbol_usage": {
            c2_mod: {
                "thirdparty_wrappers": sorted(
                    tp for tp, c2 in wildcard_map.items() if c2 == c2_mod
                ),
                "symbols_used_in_recipes": _sym_list(
                    recipe_used.get(c2_mod, set()) - {"*"}
                ),
            }
            for c2_mod in sorted(entry_points)
            if "*" in entry_points[c2_mod]
        },
        "keep": {
            "direct": sorted(direct),
            "transitive": sorted(transitive),
        },
        "delete": sorted(deletable),
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(
        description="Static reachability analysis for conan2 modules.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Determines which conan2 modules are reachable from src/thirdparty/
            and produces a KEEP / DELETE report to guide the conan2 removal migration.
        """),
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output machine-readable JSON instead of human-readable text.",
    )
    parser.add_argument(
        "--output",
        metavar="FILE",
        help="Write output to FILE instead of stdout.",
    )
    parser.add_argument(
        "--by-subpackage",
        action="store_true",
        help="In text mode, group KEEP/DELETE lists by top-level subpackage.",
    )
    args = parser.parse_args()

    # Validate that we're running from the right place
    if not CONAN2_SRC.exists():
        print(f"ERROR: conan2 source not found at {CONAN2_SRC}", file=sys.stderr)
        sys.exit(1)
    if not THIRDPARTY_SRC.exists():
        print(f"ERROR: thirdparty source not found at {THIRDPARTY_SRC}", file=sys.stderr)
        sys.exit(1)

    if not args.json:
        print("Analyzing conan2 module usage…", file=sys.stderr)

    # Run the analysis
    if not args.json:
        print("  [1/5] Enumerating conan2 modules…", file=sys.stderr)
    all_modules = build_all_conan2_modules()

    if not args.json:
        print(f"        {len(all_modules)} modules found", file=sys.stderr)
        print("  [2/5] Building conan2 internal import graph…", file=sys.stderr)
    import_graph = build_conan2_import_graph(all_modules)

    if not args.json:
        print("  [3/5] Collecting thirdparty → conan2 entry points…", file=sys.stderr)
    entry_points = collect_entry_points()

    if not args.json:
        print(f"        {len(entry_points)} entry points found", file=sys.stderr)
        print("  [4/5] Running BFS reachability analysis…", file=sys.stderr)
    reachable, deletable = compute_reachability(entry_points, import_graph, all_modules)

    if not args.json:
        print("  [5/5] Auditing recipe symbol usage…", file=sys.stderr)
    wildcard_map = build_thirdparty_wildcard_map()
    recipe_used = audit_recipe_symbols(wildcard_map)

    if not args.json:
        print("Done.\n", file=sys.stderr)

    # Build and emit output
    if args.json:
        report = build_json_report(
            all_modules, entry_points, reachable, deletable, recipe_used, wildcard_map
        )
        output = json.dumps(report, indent=2)
    else:
        import io
        buf = io.StringIO()
        old_stdout = sys.stdout
        sys.stdout = buf
        print_text_report(
            all_modules,
            entry_points,
            reachable,
            deletable,
            recipe_used,
            wildcard_map,
            by_subpackage=args.by_subpackage,
        )
        sys.stdout = old_stdout
        output = buf.getvalue()

    if args.output:
        Path(args.output).write_text(output, encoding="utf-8")
        print(f"Report written to {args.output}", file=sys.stderr)
    else:
        print(output)


if __name__ == "__main__":
    main()
