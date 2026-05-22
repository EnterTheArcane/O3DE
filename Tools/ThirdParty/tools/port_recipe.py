#!/usr/bin/env python3
"""port_recipe.py — Convert a conan-center-index recipe to ThirdParty format.

Usage::

    python tools/port_recipe.py <recipe-name> [options]

The script reads::

    <cci_root>/recipes/<name>/all/conanfile.py

and writes::

    <thirdparty_root>/recipes/<name>/recipe.py

It performs structural transforms using libcst (imports, class rename, method
removal) and expression-level transforms using regular expressions.

Options:
  --cci-root PATH       Path to conan-center-index (default: auto-detect from workspace)
  --out-root PATH       Path to ThirdParty recipes root (default: ./recipes)
  --dry-run             Print result to stdout instead of writing file
  --overwrite           Overwrite existing recipe.py (default: skip if present)
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Union

try:
    import libcst as cst
    from libcst import matchers as m
except ImportError:
    print("ERROR: libcst is required. Install with: pip install libcst", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Import mapping: conan module → thirdparty module (None = remove entirely)
# ---------------------------------------------------------------------------

_IMPORT_MAP: dict[str, str | None] = {
    "conan.tools.cmake":      "thirdparty.tools.cmake",
    "conan.tools.files":      "thirdparty.tools.files",
    "conan.tools.scm":        "thirdparty.tools.scm",
    "conan.tools.microsoft":  "thirdparty.tools.microsoft",
    "conan.tools.apple":      "thirdparty.tools.apple",
    "conan.tools.build":      "thirdparty.tools.build",   # check_min_cppstd stubs
    "conan.tools.env":        None,   # VirtualBuildEnv etc. — no-op
    "conan.tools.gnu":        "thirdparty.tools.gnu",     # PkgConfigDeps
    "conan.tools.meson":      "thirdparty.tools.meson",   # MesonToolchain + Meson
    "conan.tools.layout":     None,   # cmake_layout is a no-op
    "conan.errors":           None,   # ConanInvalidConfiguration
    "conan.tools.system":     None,
    "conan.tools.cross_building": None,
}

# Names to remove from class-level attribute assignments
_REMOVE_CLASS_ATTRS: frozenset[str] = frozenset({
    "url", "homepage", "topics", "description", "package_type",
    "settings", "short_paths", "extension_properties", "generators",
    "no_copy_source",
})

# Method names to remove entirely
_REMOVE_METHODS: frozenset[str] = frozenset({
    "export_sources",
    "export_conandata_patches",
    "config_options",
    "configure",
    "layout",
    "validate",
    "validate_build",
    "build_requirements",
    "system_requirements",
    "package_id",
    "package_info",
    "deploy",
})


# ===========================================================================
# libcst transformer
# ===========================================================================

class _ConanTransformer(cst.CSTTransformer):
    """Structural CST transformer: imports, class name, attribute/method removal."""

    def __init__(self) -> None:
        super().__init__()
        # Stack of booleans: True = this class level is the ConanFile subclass
        self._class_stack: list[bool] = []
        # requires calls collected from requirements() body
        self._requires_list: list[str] = []
        self._in_requirements_method = False

    @property
    def _in_conan_class(self) -> bool:
        return bool(self._class_stack) and self._class_stack[-1]

    # ------------------------------------------------------------------
    # Module-level: remove `required_conan_version = ...`
    # ------------------------------------------------------------------

    def leave_SimpleStatementLine(
        self,
        original_node: cst.SimpleStatementLine,
        updated_node: cst.SimpleStatementLine,
    ) -> Union[cst.SimpleStatementLine, cst.RemovalSentinel]:
        for stmt in updated_node.body:
            if isinstance(stmt, cst.Assign):
                for target in stmt.targets:
                    if (
                        isinstance(target.target, cst.Name)
                        and target.target.value == "required_conan_version"
                    ):
                        return cst.RemovalSentinel.REMOVE
            # Remove removable class-level attributes when inside conan class
            if self._in_conan_class:
                if isinstance(stmt, cst.Assign):
                    for target in stmt.targets:
                        if (
                            isinstance(target.target, cst.Name)
                            and target.target.value in _REMOVE_CLASS_ATTRS
                        ):
                            return cst.RemovalSentinel.REMOVE
                if isinstance(stmt, cst.AnnAssign):
                    if (
                        isinstance(stmt.target, cst.Name)
                        and stmt.target.value in _REMOVE_CLASS_ATTRS
                    ):
                        return cst.RemovalSentinel.REMOVE
        return updated_node

    # ------------------------------------------------------------------
    # Import rewriting
    # ------------------------------------------------------------------

    def leave_ImportFrom(
        self,
        original_node: cst.ImportFrom,
        updated_node: cst.ImportFrom,
    ) -> Union[cst.ImportFrom, cst.RemovalSentinel]:
        if updated_node.module is None:
            return updated_node

        module_str = _module_to_str(updated_node.module)

        # `from conan import ConanFile` → `from thirdparty import RecipeBase`
        if module_str == "conan":
            if isinstance(updated_node.names, (list, tuple)):
                new_names: list[cst.ImportAlias] = []
                for alias in updated_node.names:
                    if isinstance(alias.name, cst.Name) and alias.name.value == "ConanFile":
                        new_names.append(
                            alias.with_changes(name=cst.Name("RecipeBase"))
                        )
                if new_names:
                    cleaned = _clean_import_commas(new_names)
                    return updated_node.with_changes(
                        module=_str_to_module("thirdparty"),
                        names=cleaned,
                    )
            return cst.RemovalSentinel.REMOVE

        # Look up in import map
        mapped = _IMPORT_MAP.get(module_str)
        if mapped is None and module_str.startswith("conan."):
            # Unknown conan module — remove it
            return cst.RemovalSentinel.REMOVE

        if mapped is not None:
            # Replace with thirdparty equivalent, filtering removed symbols
            if isinstance(updated_node.names, (list, tuple)):
                kept = _filter_import_names(module_str, list(updated_node.names))
                if not kept:
                    return cst.RemovalSentinel.REMOVE
                cleaned = _clean_import_commas(kept)
                return updated_node.with_changes(
                    module=_str_to_module(mapped),
                    names=cleaned,
                )

        return updated_node

    # ------------------------------------------------------------------
    # Class definition: track stack, rename XxxConan → Recipe
    # ------------------------------------------------------------------

    def visit_ClassDef(self, node: cst.ClassDef) -> bool:
        is_conan = any(
            isinstance(arg.value, cst.Name) and arg.value.value == "ConanFile"
            for arg in node.bases
        )
        self._class_stack.append(is_conan)
        return True

    def leave_ClassDef(
        self,
        original_node: cst.ClassDef,
        updated_node: cst.ClassDef,
    ) -> cst.ClassDef:
        is_conan = self._class_stack.pop() if self._class_stack else False

        if not is_conan:
            return updated_node

        # Rename base class ConanFile → RecipeBase
        new_bases = [
            arg.with_changes(value=cst.Name("RecipeBase"))
            if (isinstance(arg.value, cst.Name) and arg.value.value == "ConanFile")
            else arg
            for arg in updated_node.bases
        ]

        # Ensure body is non-empty (libcst requires at least one statement)
        body = updated_node.body
        if isinstance(body, cst.IndentedBlock):
            stmts = list(body.body)
            if not stmts:
                stmts = [cst.SimpleStatementLine(body=[cst.Pass()])]
                body = body.with_changes(body=stmts)

        return updated_node.with_changes(
            name=cst.Name("Recipe"),
            bases=new_bases,
            body=body,
        )

    # ------------------------------------------------------------------
    # Method removal and requirements() rewrite
    # ------------------------------------------------------------------

    def visit_FunctionDef(self, node: cst.FunctionDef) -> bool:
        if node.name.value == "requirements":
            self._in_requirements_method = True
            self._requires_list = []
        return True

    def visit_Call(self, node: cst.Call) -> bool:
        if self._in_requirements_method:
            name = _call_name(node)
            if name in ("self.requires", "self.tool_requires"):
                for arg in node.args:
                    if arg.keyword is None:
                        pkg_ref = _string_value(arg.value)
                        if pkg_ref:
                            pkg_name = pkg_ref.split("/")[0].split("[")[0].strip()
                            self._requires_list.append(pkg_name)
                        break
        return True

    def leave_FunctionDef(
        self,
        original_node: cst.FunctionDef,
        updated_node: cst.FunctionDef,
    ) -> Union[cst.FunctionDef, cst.RemovalSentinel]:
        method_name = original_node.name.value

        # Remove unwanted methods inside the conan class
        if self._in_conan_class and method_name in _REMOVE_METHODS:
            return cst.RemovalSentinel.REMOVE

        # Rewrite requirements() to return list[str]
        if method_name == "requirements" and self._in_requirements_method:
            self._in_requirements_method = False
            items = [
                cst.Element(
                    value=cst.SimpleString(f'"{name}"'),
                    comma=cst.MaybeSentinel.DEFAULT,
                )
                for name in self._requires_list
            ]
            return_stmt = cst.SimpleStatementLine(
                body=[cst.Return(value=cst.List(elements=items))]
            )
            new_returns = cst.Annotation(
                annotation=cst.Subscript(
                    value=cst.Name("list"),
                    slice=[cst.SubscriptElement(cst.Index(cst.Name("str")))],
                )
            )
            return updated_node.with_changes(
                body=cst.IndentedBlock(body=[return_stmt]),
                returns=new_returns,
            )

        if method_name == "requirements":
            self._in_requirements_method = False

        return updated_node


# ---------------------------------------------------------------------------
# Regex-based expression transforms (applied after CST pass)
# ---------------------------------------------------------------------------

_EXPR_SUBS: list[tuple[str, str]] = [
    # apply_conandata_patches(self) → apply_patches(self)
    (r"\bapply_conandata_patches\s*\(\s*self\s*\)", "apply_patches(self)"),
    # export_conandata_patches(self) → (remove line)
    (r"^\s*export_conandata_patches\s*\(\s*self\s*\)\s*\n", ""),
    # get(self, **self.conan_data[...], ...) — complex pattern handled separately
    # Simpler: self.conan_data → self.thirdparty_data + sources → versions
    (r"\bself\.conan_data\[.sources.\]\[", "self.thirdparty_data[\"versions\"]["),
    (r"\bself\.conan_data\[\"sources\"\]\[", "self.thirdparty_data[\"versions\"]["),
    (r"\bself\.conan_data\['sources'\]\[", "self.thirdparty_data[\"versions\"]["),
    (r"\bself\.conan_data\b", "self.thirdparty_data"),
    # Remove self as first positional arg to file utility functions
    (r"\b(copy|rmdir|rm|load|save|replace_in_file|rename|collect_libs)\(self,\s*", r"\1("),
    # get(self, ...) → get(...) — but careful not to eat kwargs  
    (r"\bget\(self,\s*", "get("),
    # settings.os checks
    (r'\bself\.settings\.os\s*==\s*["\']Windows["\']', "self.is_windows"),
    (r'\bself\.settings\.os\s*==\s*["\']Linux["\']', "self.is_linux"),
    (r'\bself\.settings\.os\s*==\s*["\']Macos["\']', "self.is_macos"),
    (r'\bself\.settings\.os\s*==\s*["\']Darwin["\']', "self.is_macos"),
    (r'\bself\.settings\.os\s*in\s*\[.*?["\']Linux["\'].*?\]', "self.is_linux"),
    (r'\bself\.settings\.get_safe\(["\']os["\']\)\s*==\s*["\']Windows["\']', "self.is_windows"),
    # settings.build_type
    (r'\bself\.settings\.build_type\b', "self.build_type"),
    # is_msvc / is_apple_os helper functions
    (r'\bis_msvc\s*\(\s*self\s*\)', "self.is_windows"),
    (r'\bis_msvc_static_runtime\s*\(\s*self\s*\)', "False"),
    (r'\bis_apple_os\s*\(\s*self\s*\)', "self.is_macos"),
    # self.settings.arch
    (r'\bself\.settings\.arch\s*==\s*["\']armv8["\']', 'self.arch == "arm64"'),
    # cmake_layout() call — remove the whole line
    (r"^\s*cmake_layout\s*\(.*?\)\s*\n", ""),
    (r"\bcmake_layout\s*\([^)]*\)", ""),
    # options.get_safe → options.get
    (r'\bself\.options\.get_safe\(', "self.options.get("),
    # options.rm_safe — keep as-is (supported by _OptionsAccessor)
    # self.settings.rm_safe — remove the call line
    (r"^\s*self\.settings\.rm_safe\s*\(.*?\)\s*\n", ""),
    # Cross-build stubs
    (r'\bcross_building\s*\(\s*self\s*\)', "False"),
    (r'\bcheck_min_cppstd\s*\(\s*self\s*,', "# check_min_cppstd(self,"),
    # stdcpp_library
    (r'\bstdcpp_library\s*\(\s*self\s*\)', "None"),
    # VirtualBuildEnv / VirtualRunEnv — remove generate() calls
    (r"^\s*VirtualBuildEnv\s*\(.*?\)\.generate\s*\(\s*\)\s*\n", ""),
    (r"^\s*VirtualRunEnv\s*\(.*?\)\.generate\s*\(\s*\)\s*\n", ""),
    # PkgConfigDeps — remove
    (r"^\s*pc\s*=\s*PkgConfigDeps\s*\(.*?\)\s*\n", ""),
    (r"^\s*PkgConfigDeps\s*\(.*?\)\.generate\s*\(\s*\)\s*\n", ""),
]


def _apply_regex_transforms(code: str) -> str:
    for pattern, replacement in _EXPR_SUBS:
        code = re.sub(pattern, replacement, code, flags=re.MULTILINE)

    # Special case: get(self, **self.thirdparty_data["versions"][self.version], ...)
    # Transform to: data = self.thirdparty_data["versions"][self.version]; get(url=data["url"], dest=self.source_folder, sha256=data["sha256"])
    code = _transform_get_conandata(code)

    return code


def _transform_get_conandata(code: str) -> str:
    """Transform ``get(**self.thirdparty_data[...][self.version], ...)`` calls.

    After the earlier regex pass, conan_data has been renamed to
    thirdparty_data and sources → versions.  This pass rewrites the
    double-star-unpack form into explicit keyword arguments.
    """
    pattern = re.compile(
        r'get\s*\(\s*\*\*\s*self\.thirdparty_data\["versions"\]\[self\.version\]'
        r'(?:\s*,\s*strip_root\s*=\s*(?:True|False))?\s*\)',
        re.MULTILINE,
    )

    def _replace(m: re.Match[str]) -> str:
        return (
            "get("
            "url=self.thirdparty_data[\"versions\"][self.version][\"url\"], "
            "dest=self.source_folder, "
            "sha256=self.thirdparty_data[\"versions\"][self.version][\"sha256\"]"
            ")"
        )

    return pattern.sub(_replace, code)


# ===========================================================================
# Helpers
# ===========================================================================

def _module_to_str(node: cst.BaseExpression) -> str:
    if isinstance(node, cst.Name):
        return node.value
    if isinstance(node, cst.Attribute):
        return _module_to_str(node.value) + "." + node.attr.value
    return ""


def _str_to_module(s: str) -> cst.BaseExpression:
    parts = s.split(".")
    result: cst.BaseExpression = cst.Name(parts[0])
    for part in parts[1:]:
        result = cst.Attribute(value=result, attr=cst.Name(part))
    return result


def _clean_import_commas(aliases: list[cst.ImportAlias]) -> list[cst.ImportAlias]:
    """Ensure no trailing comma on the last import alias."""
    if not aliases:
        return aliases
    last = aliases[-1]
    aliases[-1] = last.with_changes(comma=cst.MaybeSentinel.DEFAULT)
    return aliases


def _filter_import_names(
    original_module: str,
    names: list[cst.ImportAlias],
) -> list[cst.ImportAlias]:
    """Remove/rename symbols from a module as needed."""
    _REMOVE_SYMBOLS: dict[str, set[str]] = {
        "conan.tools.cmake": {"cmake_layout", "basic_layout"},
        "conan.tools.build": {"check_min_cppstd", "cross_building", "stdcpp_library",
                               "valid_min_cppstd", "check_max_cppstd"},
        "conan.tools.files": {"export_conandata_patches"},
    }
    # Rename symbols: old_name → new_name
    _RENAME_SYMBOLS: dict[str, dict[str, str]] = {
        "conan.tools.files": {"apply_conandata_patches": "apply_patches"},
    }
    drop = _REMOVE_SYMBOLS.get(original_module, set())
    rename = _RENAME_SYMBOLS.get(original_module, {})
    kept: list[cst.ImportAlias] = []
    for alias in names:
        sym = alias.name.value if isinstance(alias.name, cst.Name) else ""
        if sym in drop:
            continue
        if sym in rename:
            alias = alias.with_changes(name=cst.Name(rename[sym]))
        kept.append(alias)
    return kept


def _call_name(node: cst.Call) -> str:
    func = node.func
    if isinstance(func, cst.Name):
        return func.value
    if isinstance(func, cst.Attribute):
        return _module_to_str(func.value) + "." + func.attr.value
    return ""


def _string_value(node: cst.BaseExpression) -> str | None:
    """Extract plain string value, or the initial literal prefix for f-strings."""
    if isinstance(node, cst.SimpleString):
        try:
            return node.evaluated_value  # type: ignore[attr-defined]
        except Exception:
            return None
    if isinstance(node, cst.FormattedString):
        # For f"name/{version}" patterns, extract the part before the first `{`
        for part in node.parts:
            if isinstance(part, cst.FormattedStringText):
                text = part.value
                if "/" in text:
                    return text.split("/")[0].strip()
                if text.strip():
                    return text.strip()
        return None
    if isinstance(node, cst.ConcatenatedString):
        return None
    return None


# ===========================================================================
# Post-processing: clean up empty lines, add header comment
# ===========================================================================

def _post_process(code: str, recipe_name: str) -> str:
    # Collapse 3+ blank lines → 2
    code = re.sub(r"\n{3,}", "\n\n", code)
    return code.lstrip("\n")


# ===========================================================================
# Entry point
# ===========================================================================

def _find_cci_root() -> Path:
    """Attempt to locate conan-center-index relative to common workspace layout."""
    candidates = [
        Path(r"D:\OpenSource\conan-center-index"),
        Path(__file__).parent.parent.parent.parent.parent / "OpenSource" / "conan-center-index",
    ]
    for c in candidates:
        if c.is_dir():
            return c
    raise RuntimeError(
        "Cannot auto-detect conan-center-index root. "
        "Pass --cci-root explicitly."
    )


def port_recipe(
    name: str,
    cci_root: Path,
    out_root: Path,
    dry_run: bool = False,
    overwrite: bool = False,
) -> None:
    # Locate source
    src_path = cci_root / "recipes" / name / "all" / "conanfile.py"
    if not src_path.exists():
        # Some recipes have a single version folder instead of "all"
        candidates = sorted((cci_root / "recipes" / name).glob("*/conanfile.py"))
        if not candidates:
            print(f"ERROR: conanfile.py not found for {name!r} in {cci_root}", file=sys.stderr)
            sys.exit(1)
        src_path = candidates[0]
        print(f"[port] Using {src_path.relative_to(cci_root)}")

    out_path = out_root / name / "recipe.py"

    if not overwrite and out_path.exists() and not dry_run:
        print(f"[port] SKIP {name} — {out_path} already exists (use --overwrite)")
        return

    source_code = src_path.read_text(encoding="utf-8")

    # --- CST transforms ---
    try:
        tree = cst.parse_module(source_code)
        transformer = _ConanTransformer()
        new_tree = tree.visit(transformer)
        transformed = new_tree.code
    except cst.ParserSyntaxError as exc:
        print(f"[port] WARNING: libcst parse error for {name}: {exc}", file=sys.stderr)
        print("[port] Falling back to regex-only transforms")
        transformed = source_code

    # --- Regex transforms ---
    transformed = _apply_regex_transforms(transformed)

    # --- Post-processing ---
    transformed = _post_process(transformed, name)

    if dry_run:
        print(transformed)
        return

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(transformed, encoding="utf-8")
    print(f"[port] Wrote {out_path}")


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(
        description="Convert a conan-center-index recipe to ThirdParty format."
    )
    parser.add_argument("name", help="Recipe name (e.g. zlib-ng)")
    parser.add_argument(
        "--cci-root",
        default=None,
        metavar="PATH",
        help="Path to conan-center-index root",
    )
    parser.add_argument(
        "--out-root",
        default=None,
        metavar="PATH",
        help="Path to ThirdParty recipes/ directory (default: ./recipes)",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print to stdout only")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite existing recipe.py")

    args = parser.parse_args(argv)

    cci_root = Path(args.cci_root) if args.cci_root else _find_cci_root()
    if args.out_root:
        out_root = Path(args.out_root)
    else:
        # Default: <script-dir>/../recipes
        out_root = Path(__file__).parent.parent / "recipes"

    port_recipe(
        name=args.name,
        cci_root=cci_root,
        out_root=out_root,
        dry_run=args.dry_run,
        overwrite=args.overwrite,
    )


if __name__ == "__main__":
    main()
