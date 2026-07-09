#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import glob
import inspect
import io
import json
import os
import os.path
import platform
import pprint
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple


class Foreground:
    """ANSI escape codes for foreground (text) colors."""
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'
    RESET = '\033[39m'


class Background:
    """ANSI escape codes for background colors."""
    BLACK = '\033[40m'
    RED = '\033[41m'
    GREEN = '\033[42m'
    YELLOW = '\033[43m'
    BLUE = '\033[44m'
    MAGENTA = '\033[45m'
    CYAN = '\033[46m'
    WHITE = '\033[47m'
    RESET = '\033[49m'


class Style:
    """ANSI escape codes for text styling."""
    BRIGHT = '\033[1m'
    DIM = '\033[2m'
    NORMAL = '\033[22m'
    RESET_ALL = '\033[0m'


def find_token_to_int(output: str, expression: str) -> int:
    """
    Search for a regex pattern in output and extract the numeric value.

    Returns the integer value found, -1 if the pattern is not found,
    or -2 if the matched text contains no valid integer.
    """
    match = re.search(expression, output)
    if not match:
        return -1
    digits = re.sub(r"\D", "", match.group())
    return int(digits) if digits.isdigit() else -2


def first_of(haystack: bytes, *needles: str) -> int:
    """
    Return the index of the earliest occurrence of any needle in haystack.

    Returns -1 if none of the needles are found.
    """
    decoded = haystack.decode('utf-8')
    positions = [decoded.find(n) for n in needles]
    valid = [p for p in positions if p != -1]
    return min(valid) if valid else -1


def strip_quotes(text: str) -> str:
    """Strip leading/trailing whitespace and surrounding quotes from text."""
    return text.strip().strip('"').strip("'")


def verify_partition_predicate(operands: List[str]) -> bool:
    """Verify a partitioned predicate expression (==, !=)."""
    if operands[1] == '==':
        return operands[0] == operands[2]
    if operands[1] == '!=':
        return operands[0] != operands[2]
    return False


def execute_predicate_checks(message: bytes) -> Tuple[bool, int]:
    """
    Parse and verify all '@check predicate' annotations in compiler output.

    Returns a tuple of (all_passed, num_checked).
    """
    num_checked = 0
    look_for = "@check predicate"
    for m in re.finditer(look_for, message.decode('utf-8')):
        num_checked += 1
        start = m.start() + len(look_for)
        leftover = message[start:]
        stop = first_of(leftover, '\\n', '\n', '"@')
        whole_expr = leftover[:stop]
        operands = whole_expr.decode('utf-8').partition("==")
        operands = [strip_quotes(s) for s in operands]
        if not verify_partition_predicate(operands):
            print("TEST error:", operands)
            return (False, num_checked)
    return (True, num_checked)


def launch_compiler(
    compiler_path: str,
    options: List[str],
    silent: bool
) -> Tuple[bytes, bytes, int]:
    """
    Run the AZSLC compiler with the given options.

    Returns a tuple of (stdout, stderr, return_code).
    """
    arg_list = [compiler_path] + list(options)
    print("    Running: ", ' '.join(arg_list))
    process = subprocess.Popen(
        arg_list, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    out, err = process.communicate()
    if not silent:
        sys.stdout.write(out.decode('utf-8'))
        sys.stderr.write(err.decode('utf-8'))
    return (out, err, process.returncode)


def parse_yaml(text: bytes) -> Optional[Dict[str, Any]]:
    """
    Parse YAML text (designed for --dumpsym output).

    Returns the parsed dictionary or None on failure.
    """
    try:
        import yaml
    except ImportError:
        print("no yaml module. execute `pip install pyyaml`")
        return None
    try:
        return yaml.load(text, Loader=yaml.FullLoader)
    except yaml.YAMLError as e:
        print("Parsing YAML string failed")
        if hasattr(e, "reason"):
            print(f"Reason: {e.reason}")
            print(
                f"At position: {e.position} "
                f"(line {len(text[0:e.position].splitlines())}) "
                f"with encoding {e.encoding}"
            )
            print(f"Invalid char code: {e.character}")
            print(f"culprit context: {text[e.position - 8: e.position + 8]}")
        else:
            raise
        raise
    except AttributeError as ae:
        if "has no attribute 'FullLoader'" in str(ae):
            return yaml.load(text)
        else:
            raise


def build_and_get_symbols(
    file: str,
    compiler_path: str,
    silent: bool,
    extra_args: Optional[List[str]] = None
) -> Tuple[Optional[Dict[str, Any]], bool]:
    """
    Compile with --dumpsym and return the parsed symbol table.

    Returns a tuple of (symbols_dict, success_bool).
    """
    if extra_args is None:
        extra_args = []
    stdout, stderr, code = launch_compiler(
        compiler_path, [file, "--dumpsym"] + extra_args, silent
    )
    if code != 0:
        if not silent:
            print(
                f"{Foreground.RED}{Style.BRIGHT}compilation failed "
                f"{Style.NORMAL}{file}{Style.RESET_ALL}"
            )
        return (None, False)
    symbols = parse_yaml(stdout)
    if symbols is None:
        print(
            f"{Foreground.RED}Parsing result of --dumpsym failed"
            f"{Style.RESET_ALL}"
        )
        return (None, False)
    return (symbols, True)


def build_and_get_json(
    file: str,
    compiler_path: str,
    silent: bool,
    extra_args: List[str]
) -> Tuple[Optional[Dict[str, Any]], bool]:
    """
    Compile with extra arguments and return the parsed JSON output.

    Returns a tuple of (json_dict, success_bool).
    """
    stdout, stderr, code = launch_compiler(
        compiler_path, [file] + extra_args, silent
    )
    if code != 0:
        if not silent:
            print(
                f"{Foreground.RED}{Style.BRIGHT}compilation failed "
                f"{Style.NORMAL}{extra_args} {file}{Style.RESET_ALL}"
            )
        return (None, False)
    parsed = json.loads(stdout)
    if parsed is None:
        print(
            f"{Foreground.RED}Parsing result of {extra_args} failed"
            f"{Style.RESET_ALL}"
        )
        return (None, False)
    return (parsed, True)


def build_and_get(
    file: str,
    compiler_path: str,
    silent: bool,
    extra_args: List[str]
) -> Tuple[Optional[bytes], bool]:
    """
    Compile and return raw stdout on success.

    Returns a tuple of (stdout_bytes, success_bool).
    """
    # Force silent to avoid polluting caller's output
    stdout, stderr, code = launch_compiler(
        compiler_path, [file] + extra_args, True
    )
    if code != 0:
        if not silent:
            print(
                f"{Foreground.RED}compilation of {file} failed"
                f"{Style.RESET_ALL}"
            )
        return (None, False)
    return (stdout, True)


def build_and_get_error(
    file: str,
    compiler_path: str,
    silent: bool,
    extra_args: List[str]
) -> Tuple[Optional[bytes], bool]:
    """
    Compile expecting failure and return stderr on failure.

    Returns a tuple of (stderr_bytes, did_fail_bool).
    """
    # Force silent to avoid polluting caller's output
    stdout, stderr, code = launch_compiler(
        compiler_path, [file] + extra_args, True
    )
    if code == 0:
        if not silent:
            print(
                f"{Foreground.RED}compilation of {file} succeeded. "
                f"Was expecting failure.{Style.RESET_ALL}"
            )
        return (None, False)
    return (stderr, True)


def dump_keywords(
    compiler_path: str
) -> Tuple[Optional[Dict[str, Any]], bool]:
    """
    Run --listpredefined and return the parsed keyword table.

    Returns a tuple of (keywords_dict, success_bool).
    """
    stdout, stderr, code = launch_compiler(
        compiler_path, ["--listpredefined"], False
    )
    if code != 0:
        print(
            f"{Foreground.RED}compilation failed{Style.RESET_ALL}"
        )
        return (None, False)
    tokens = parse_yaml(stdout)
    if tokens is None:
        print(
            f"{Foreground.RED}Parsing result of --listpredefined failed"
            f"{Style.RESET_ALL}"
        )
        return (None, False)
    return (tokens, True)


def verify_all_predicates(
    predicates: list,
    code: Any,
    silent: bool = True
) -> bool:
    """
    Execute a list of predicate lambdas and report any failures.

    Each predicate should return True on success. On failure the predicate
    source is printed for debugging. If any predicate fails and silent is
    False, the full parsed data is pretty-printed.

    Returns True if all predicates passed.
    """
    all_ok = True
    for i, p in enumerate(predicates):
        ok = True
        try:
            ok = p()
        except Exception as e:
            print(f"{Foreground.RED}exception {Style.RESET_ALL}{e}")
            ok = False
        if not ok:
            try:
                src = inspect.getsource(p)
            except Exception:
                src = "<exception>"
            print(f"{Foreground.RED}FAIL ({i}):{Style.RESET_ALL}{src}")
        all_ok = all_ok and ok
    if not all_ok and not silent:
        print("dump as parsed:")
        pp = pprint.PrettyPrinter(indent=2, width=160)
        pp.pprint(code)
    return all_ok


# Accumulated list of failed test files (for detailed mode reporting).
fail_list: List[str] = []

# Tracks the last regex match position to allow sequential scanning
# without rescanning from the beginning on each call.
_last_end: int = 0


def reset_search_position():
    """Reset the sequential search position back to the start."""
    global _last_end
    _last_end = 0


def find_pattern(needle: str, haystack: str, negative: bool) -> bool:
    """
    Search for a whitespace-flexible pattern in the haystack string.

    The needle is split on whitespace and each token is escaped so it is
    matched literally.  Tokens are then joined with ``\\s+`` to allow any
    amount of whitespace between them.

    Searching resumes from the position after the previous match (use
    ``reset_search_position()`` to start over).

    Args:
        needle: The pattern text to search for.
        haystack: The full text to search within.
        negative: If True, the predicate succeeds when the pattern is
            *not* found (negation check).

    Returns:
        True if the predicate is satisfied.
    """
    global _last_end
    words = re.split(r"\s", needle)
    words = [w for w in words if w]
    words = [re.escape(w) for w in words]
    pattern = r"\s+".join(words)
    compiled = re.compile(pattern)
    consumed = haystack[_last_end:]
    match_obj = compiled.search(consumed)
    if match_obj:
        _last_end += match_obj.end()
        return not negative
    return negative


def parse_string_list(text: str) -> List[str]:
    """
    Parse a bracketed string list from a shader source comment.

    Handles formats like ``['--unique-idx', '--root-sig']`` or plain
    ``--namespace=vk``.
    """
    cleaned = re.sub(r"[\[\],\s]", "", text)
    parts = re.split(r"['\"]", cleaned)
    return [s for s in parts if s != '']


def verify_emission_pattern(
    azsl_file: str,
    patterns_file: str,
    compiler_path: str,
    silent: bool,
    arg_list: List[str]
) -> bool:
    """
    Compile an AZSL file and verify output against a pattern file.

    The pattern file contains one quoted string per line representing an
    expected token sequence in the emitted HLSL.  Lines starting with
    ``^`` are negation checks (the pattern must NOT appear).

    Returns True if all patterns matched successfully.
    """
    if not os.path.exists(patterns_file):
        print(f"Pattern file not found: {patterns_file}")
        return False

    extra_args: List[str] = []
    with io.open(patterns_file, "r", encoding="utf-8") as f:
        for line in f:
            if line.find("Cmdargs") >= 0:
                line = line[line.rfind(':') + 1:]
                extra_args = parse_string_list(line)
                if extra_args and not silent:
                    print(f"Adding extra command line arguments: {extra_args}")

    shader_code, ok = build_and_get(
        azsl_file, compiler_path, silent, arg_list + extra_args
    )
    if ok:
        if not silent:
            print(
                f"{Style.BRIGHT}{Foreground.CYAN}Now to check emission "
                f"patterns for {patterns_file}{Style.RESET_ALL}"
            )
        # Normalize the shader code by inserting spaces around identifiers
        # stuck to other things, e.g. 'func()' becomes 'func ( )'.
        all_idents = re.split(
            r"([a-zA-Z_]+[a-zA-Z_0-9]*)|(\.)|(,)|(::)|(;)|(\()|(\))|(<)|(>)|( )",
            shader_code.decode('utf-8')
        )
        all_idents = [
            s for s in all_idents
            if s is not None and s != "" and s != " "
        ]
        normalized_code = " ".join(all_idents)

        predicates = []
        with io.open(patterns_file, encoding="utf-8") as f:
            i = 0
            for line in f:
                negation = line.startswith('^')
                if negation:
                    line = line[1:]
                if line.startswith('"'):
                    line = line.strip().strip('"')
                    if not silent:
                        print(
                            f"{Foreground.CYAN}Verify ({i})"
                            f"{' not' if negation else ''} : "
                            f"{line}{Style.RESET_ALL}"
                        )
                    predicates.append(
                        lambda line=line, negation=negation:
                        find_pattern(line, normalized_code, negation)
                    )
                    i += 1

        reset_search_position()
        ok = verify_all_predicates(predicates, normalized_code, silent)
    return ok


def verify_emission_patterns(
    file: str,
    compiler_path: str,
    silent: bool,
    arg_list: List[str]
) -> int:
    """
    Verify all pattern files associated with the given AZSL source.

    Pattern files are discovered by matching ``<basename>*.txt`` or
    ``<basename>-[0-9].txt`` in the same directory.

    Returns the number of successfully verified pattern files, or 0 on
    any failure.
    """
    global fail_list
    local_fail_list: List[str] = []
    result = 0
    base = os.path.basename(file)
    file_prefix = f"{os.path.dirname(file)}/{os.path.splitext(base)[0]}"

    for pattern_file in (
        glob.glob(f'{file_prefix}*[0-9].txt') or
        glob.glob(f'{file_prefix}.txt')
    ):
        if not verify_emission_pattern(
            file, pattern_file, compiler_path, silent, arg_list
        ):
            local_fail_list.append(pattern_file)
        else:
            result += 1

    if local_fail_list:
        fail_list.extend(local_fail_list)
        return 0
    return result


def print_failed_test_list(silent: bool) -> None:
    """Print the accumulated list of failed test files and reset it."""
    global fail_list
    if not silent and fail_list:
        print(
            f"{Style.BRIGHT}{Foreground.RED}failed files: "
            f"{Foreground.WHITE}{fail_list}{Style.RESET_ALL}"
        )
    # Reset so the list doesn't carry over between platform runs.
    fail_list = []


def compile_and_expect_error(
    file: str,
    compiler_path: str,
    silent: bool,
    arg_list: List[str]
) -> int:
    """
    Compile a file expecting failure and verify the error code.

    The expected error code is read from a ``#EC <number>`` comment in
    the source file.

    Returns 1 on success (error code matched) or 0 on failure.
    """
    global fail_list

    options = [file] + list(arg_list)
    out, err, code = launch_compiler(compiler_path, options, silent)

    if code == 0:
        if not silent:
            print(
                f"{Foreground.RED}{Style.BRIGHT}FAIL. Expected {file} "
                f"to report compilation errors{Style.RESET_ALL}"
            )
        fail_list.append(file)
        return 0

    # Read the expected error code from the source file.
    with io.open(file, 'r', encoding="utf-8") as f:
        azsl_code = f.read()

    expected_ec = find_token_to_int(azsl_code, r"#EC\s\d*")
    actual_ec = find_token_to_int(
        err.decode('utf-8'), r"error\s#\d*:"
    )

    if actual_ec == expected_ec:
        return 1

    if not silent:
        print(
            f"{Foreground.RED}{Style.BRIGHT}FAIL. Expected error code "
            f"{expected_ec} from {file}, instead got error code "
            f"{actual_ec}{Style.RESET_ALL}"
        )
    fail_list.append(file)
    return 0


class BuildResult:
    """Outcome of a shader build attempt."""

    def __init__(self, can_build: bool, did_build: bool) -> None:
        self.can_build = can_build
        self.did_build = did_build


BUILD_IMPOSSIBLE = BuildResult(False, False)
BUILD_FAILED = BuildResult(True, False)
BUILD_SUCCEEDED = BuildResult(True, True)
