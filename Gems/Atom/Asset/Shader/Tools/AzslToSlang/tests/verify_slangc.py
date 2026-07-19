"""Syntax-check converted .slang files with slangc.

The shader builder injects the prelude imports and a per-API prelude before compiling, so this
harness reproduces that preamble in a temp copy of each file. Files are compiled without a target
so entry-point/stage selection is not required; this checks the front end (parse, name resolution,
type checking), which is what the conversion rules can break.
"""

from __future__ import annotations

import argparse
import random
import subprocess
import sys
import tempfile
from pathlib import Path

TOOL_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOL_ROOT))
REPO_ROOT = TOOL_ROOT.parents[5]

SLANGC = REPO_ROOT / "build" / "clang22" / "_deps" / "slang-src" / "bin" / "slangc.exe"

def _shader_lib_roots() -> list[Path]:
    """Every gem's ShaderLib, which is how the builder resolves <Gem/Path/File> includes.

    Each gem contributes its own Assets/ShaderLib as a search root (OpenParticleSystem's
    <OpenParticle/ParticleCommon.slangi> resolves through its gem's root, not Atom's), so they are
    discovered rather than listed.
    """
    roots = [
        path
        for path in REPO_ROOT.glob("Gems/*/Assets/ShaderLib")
        if path.is_dir()
    ]
    roots += [
        path
        for path in REPO_ROOT.glob("Gems/*/*/Assets/ShaderLib")
        if path.is_dir()
    ]
    return roots


INCLUDE_ROOTS = [
    REPO_ROOT / "Gems/Atom/RPI/Assets/ShaderLib",
    REPO_ROOT / "Gems/Atom/Feature/Common/Assets/ShaderLib",
    *_shader_lib_roots(),
    REPO_ROOT / "Gems/Atom/RPI/Assets",
    REPO_ROOT / "Gems/Atom/Feature/Common/Assets",
    REPO_ROOT / "Gems/Atom/Feature/Common",
    REPO_ROOT / "AutomatedTesting/ShaderLib",
    REPO_ROOT / "Gems",
    REPO_ROOT,
]

# The legacy pipeline injects Atom/RPI/Platform/<OS>/AzslcPlatformHeader.azsli before compiling.
# The `real` family now lives in Prelude.slang, but the remaining macros from that header have no
# Slang counterpart yet, so they are supplied here to keep this harness measuring the conversion
# rather than that gap. Values match the Windows/DX12 header.
PLATFORM_PRELUDE = """#define UNBOUNDED_SIZE
#define AZ_TRAITS_MATERIALS_USE_SAMPLER_ARRAY
"""

PREAMBLE = (
    "import Atom.RPI.Prelude;\nimport Atom.RPI.ShaderResourceGroup;\n" + PLATFORM_PRELUDE
)


def check(path: Path) -> tuple[bool, str]:
    source = path.read_text(encoding="utf-8", newline="")
    with tempfile.TemporaryDirectory() as tmp:
        staged = Path(tmp) / path.name
        staged.write_text(PREAMBLE + source, encoding="utf-8", newline="")

        command = [str(SLANGC), str(staged)]
        # The file's own directory first, so quote-form relative includes resolve as they would
        # in place; the staged copy lives elsewhere.
        for root in [path.parent, *INCLUDE_ROOTS]:
            command += ["-I", str(root)]
        # Keep the front end honest but skip codegen: most .slangi fragments have no entry point.
        command += ["-target", "spirv", "-o", str(Path(tmp) / "out.spv")]

        completed = subprocess.run(command, capture_output=True, text=True, timeout=120)

    output = (completed.stdout + completed.stderr).strip()
    # Errors the front end raises AFTER successfully parsing/type-checking, which this harness can't
    # satisfy standalone but the real builder does, so they don't indicate a bad conversion:
    #   E57004  no entry point requested (this harness type-checks, it doesn't select a stage)
    #   E45001  unresolved external -- [AtomOption] options are extern functions the builder's
    #           generated options module satisfies at link time
    HARNESS_EXPECTED = ("E57004", "E45001")
    errors = [
        line
        for line in output.splitlines()
        if "error" in line.lower() and not any(code in line for code in HARNESS_EXPECTED)
    ]
    # Keep the diagnostic body (the "--> file:line" and source excerpt) with each error line.
    detail = []
    lines = output.splitlines()
    for index, line in enumerate(lines):
        if "error" in line.lower() and not any(code in line for code in HARNESS_EXPECTED):
            detail.extend(lines[index : index + 3])
    return (not errors), "\n".join(detail[:9])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sample", type=int, default=0, help="check a random sample of N files")
    parser.add_argument("--entry-only", action="store_true",
                        help="only check .slang (entry point) files, not .slangi fragments")
    parser.add_argument("--seed", type=int, default=17)
    args = parser.parse_args()

    if not SLANGC.exists():
        print(f"slangc not found at {SLANGC}", file=sys.stderr)
        return 2

    pattern = "**/*.slang" if args.entry_only else "**/*.slang*"
    # These modules are the preamble the harness injects, so compiling them with it prepended would
    # self-import; they are validated by being imported into every other file.
    PREAMBLE_MODULES = {"Prelude.slang", "ShaderResourceGroup.slang", "ApiPrelude.slang"}
    files = [
        path
        for root in ("Gems", "Templates", "AutomatedTesting")
        for path in (REPO_ROOT / root).glob(pattern)
        if not any(part in {"Cache", "build", "External", "_deps"} for part in path.parts)
        and path.name not in PREAMBLE_MODULES
    ]
    files.sort()

    if args.sample and args.sample < len(files):
        random.Random(args.seed).shuffle(files)
        files = files[: args.sample]
        files.sort()

    print(f"checking {len(files)} file(s) with slangc\n")
    failures = []
    for path in files:
        try:
            ok, message = check(path)
        except subprocess.TimeoutExpired:
            ok, message = False, "slangc timed out"
        if not ok:
            failures.append((path, message))
            print(f"FAIL {path.relative_to(REPO_ROOT)}")
            for line in message.splitlines():
                print(f"      {line}")

    print(f"\n{len(files) - len(failures)}/{len(files)} compiled clean")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
