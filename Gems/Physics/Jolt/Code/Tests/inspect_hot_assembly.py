#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


FUNCTION_LABEL = re.compile(r"^[0-9a-fA-F]+ <(.+)>:$")
INSTRUCTION = re.compile(r"^\s*[0-9a-fA-F]+:\s+([a-zA-Z][a-zA-Z0-9.]*)\s*(.*)$")
INTEL_STACK_ALLOCATION = re.compile(r"\brsp\s*,\s*(0x[0-9a-fA-F]+|[0-9]+)")
ATT_STACK_ALLOCATION = re.compile(r"\$(0x[0-9a-fA-F]+|[0-9]+)\s*,\s*%rsp")


def parse_integer(value: str) -> int:
    return int(value, 16 if value.lower().startswith("0x") else 10)


def analyze_function(name: str, lines: list[str]) -> dict:
    instructions = []
    call_targets = []
    stack_frame_bytes = 0
    conditional_branches = 0
    conversion_instructions = 0
    copy_candidates = 0
    pushed_register_bytes = 0
    vector_instructions = 0
    for line in lines:
        match = INSTRUCTION.match(line)
        if not match:
            continue
        opcode = match.group(1).lower()
        operands = match.group(2)
        instructions.append(opcode)
        if opcode.startswith("call"):
            call_targets.append(operands.strip())
        if opcode.startswith("j") and opcode not in ("jmp", "jmpq"):
            conditional_branches += 1
        if "cvt" in opcode:
            conversion_instructions += 1
        if opcode.startswith("v") or re.search(r"\b[xyz]mm\d+\b", operands):
            vector_instructions += 1
        if "movs" in opcode or opcode.startswith("rep"):
            copy_candidates += 1
        if opcode in ("push", "pushq"):
            pushed_register_bytes += 8
        if opcode in ("sub", "subq") and ("rsp" in operands or "%rsp" in operands):
            stack_match = INTEL_STACK_ALLOCATION.search(operands)
            if not stack_match:
                stack_match = ATT_STACK_ALLOCATION.search(operands)
            if stack_match:
                stack_frame_bytes = max(stack_frame_bytes, parse_integer(stack_match.group(1)))

    stack_frame_bytes += pushed_register_bytes

    return {
        "branch_count": conditional_branches,
        "call_count": len(call_targets),
        "call_targets": call_targets,
        "conversion_count": conversion_instructions,
        "copy_candidate_count": copy_candidates,
        "instruction_count": len(instructions),
        "name": name,
        "stack_frame_bytes": stack_frame_bytes,
        "vector_instruction_count": vector_instructions,
    }


def disassemble(objdump: Path, binary: Path) -> str:
    result = subprocess.run(
        (
            str(objdump),
            "--disassemble",
            "--demangle",
            "--no-show-raw-insn",
            str(binary),
        ),
        check=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    return result.stdout


def write_markdown(path: Path, binary: Path, functions: list[dict]) -> None:
    lines = [
        "# Hot assembly inspection",
        "",
        f"Binary: `{binary}`",
        "",
        "These counts are diagnostic heuristics, not exact-opcode pass/fail gates.",
        "",
        "| Function | Stack | Instructions | Calls | Branches | Conversions | Vector | Copy candidates |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for function in functions:
        lines.append(
            f"| `{function['name']}` | {function['stack_frame_bytes']} | "
            f"{function['instruction_count']} | {function['call_count']} | "
            f"{function['branch_count']} | {function['conversion_count']} | "
            f"{function['vector_instruction_count']} | {function['copy_candidate_count']} |"
        )
    lines.append("")
    for function in functions:
        lines.extend((f"## `{function['name']}`", "", "Out-of-line call targets:", ""))
        if function["call_targets"]:
            lines.extend(f"- `{target}`" for target in function["call_targets"])
        else:
            lines.append("- None")
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate diagnostic assembly metrics for selected hot functions."
    )
    parser.add_argument("binary", type=Path)
    parser.add_argument("--objdump", type=Path, required=True)
    parser.add_argument("--symbol", action="append", required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    arguments = parser.parse_args()

    binary = arguments.binary.resolve()
    objdump = arguments.objdump.resolve()
    if not binary.is_file() or not objdump.is_file():
        print("The input binary or llvm-objdump executable does not exist.", file=sys.stderr)
        return 1

    patterns = [re.compile(pattern) for pattern in arguments.symbol]
    functions = []
    current_name = ""
    current_lines = []
    for line in disassemble(objdump, binary).splitlines():
        label = FUNCTION_LABEL.match(line)
        if label:
            if current_name and any(pattern.search(current_name) for pattern in patterns):
                functions.append(analyze_function(current_name, current_lines))
            current_name = label.group(1)
            current_lines = []
            continue
        if current_name:
            current_lines.append(line)
    if current_name and any(pattern.search(current_name) for pattern in patterns):
        functions.append(analyze_function(current_name, current_lines))

    if not functions:
        print("No requested symbols were found in the binary.", file=sys.stderr)
        return 1
    functions.sort(key=lambda function: function["name"])

    arguments.json_output.parent.mkdir(parents=True, exist_ok=True)
    arguments.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    with arguments.json_output.open("w", encoding="utf-8", newline="\n") as output_file:
        json.dump({"binary": str(binary), "functions": functions}, output_file, indent=2)
        output_file.write("\n")
    write_markdown(arguments.markdown_output, binary, functions)
    return 0


if __name__ == "__main__":
    sys.exit(main())
