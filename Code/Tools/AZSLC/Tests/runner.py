#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import importlib.util
import io
import os
import shutil
import sys
import traceback
from argparse import ArgumentParser
from datetime import timedelta
from os.path import join
from timeit import default_timer as timer
from typing import List, Optional

# Ensure the Tests directory is importable regardless of CWD.
_TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TESTS_DIR not in sys.path:
    sys.path.insert(0, _TESTS_DIR)

from common import Foreground, Style
import common


class TestResult:
    """Aggregated test outcome counters."""

    def __init__(
        self, num_pass: int, num_todo: int, num_fail: int, num_ec: int
    ):
        self.num_pass = num_pass
        self.num_todo = num_todo
        self.num_fail = num_fail
        self.num_ec = num_ec

    def add(self, other: 'TestResult'):
        self.num_pass += other.num_pass
        self.num_todo += other.num_todo
        self.num_fail += other.num_fail
        self.num_ec += other.num_ec


def get_status_verbose(
    num_pass: int,
    num_fail: int,
    input_source: str,
    suffix_message: str,
    extra_color: str,
    extra_message: str
) -> 'TestResult':
    """Print a color-coded status line and return a TestResult."""
    if num_fail == 0:
        print(
            f"{Foreground.GREEN}{Style.BRIGHT}"
            f"\\\\[ OK ]// : {input_source}"
            f"{suffix_message}{extra_color}{extra_message}{Style.RESET_ALL}"
        )
        return TestResult(num_pass, 0, num_fail, 0)

    if input_source.find("wip-") >= 0:
        print(
            f"{Foreground.YELLOW}{Style.BRIGHT}"
            f"\\\\[ TODO ]// : {input_source}"
            f"{suffix_message}{extra_color}{extra_message}{Style.RESET_ALL}"
        )
        return TestResult(num_pass, num_fail, 0, 0)

    if extra_message.find("Missing ErrorCode") >= 0:
        print(
            f"{Foreground.YELLOW}{Style.BRIGHT}"
            f"\\\\[ EC ]// : {input_source}"
            f"{suffix_message}{extra_color}{extra_message}{Style.RESET_ALL}"
        )
        return TestResult(num_pass, 0, 0, num_fail)

    print(
        f"{Foreground.RED}{Style.BRIGHT}"
        f"\\\\[ FAILED ]// : {input_source}"
        f"{suffix_message}{extra_color}{extra_message}{Style.RESET_ALL}"
    )
    return TestResult(num_pass, 0, num_fail, 0)


def test_file(
    advanced_folder: str,
    input_source: str,
    expect_pass: bool,
    what_to_test: str,
    compiler_path: str,
    silent: bool
) -> 'TestResult':
    """Run a single test file and return the result."""
    if input_source.endswith(".py"):
        # Resolve the script's directory from its path so that CWD and
        # sys.path are correct regardless of how the runner was invoked.
        abs_source = os.path.abspath(input_source)
        script_dir = os.path.dirname(abs_source)

        # Import the test module by file path (supports hyphens in filenames).
        # The Tests directory (_TESTS_DIR) is added to sys.path so that
        # "from Shared import ..." works inside every test script without
        # each one needing its own sys.path hack.
        added_tests_dir = _TESTS_DIR not in sys.path
        if added_tests_dir:
            sys.path.insert(0, _TESTS_DIR)
        module_name = os.path.splitext(os.path.basename(input_source))[0]
        spec = importlib.util.spec_from_file_location(module_name, abs_source)
        test_script_module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = test_script_module
        old_cwd = os.getcwd()
        try:
            spec.loader.exec_module(test_script_module)
            os.chdir(script_dir)

            # Run the test
            test_script_module.do_tests(compiler_path, silent)
        except Exception as exc:
            os.chdir(old_cwd)
            if added_tests_dir and _TESTS_DIR in sys.path:
                sys.path.remove(_TESTS_DIR)
            if module_name in sys.modules:
                del sys.modules[module_name]
            error_type = type(exc).__name__
            print(
                f"{Foreground.RED}{Style.BRIGHT}"
                f"\\\\[ ERROR ]// : {input_source} "
                f"-- {error_type}: {exc}{Style.RESET_ALL}"
            )
            return TestResult(0, 0, 1, 0)

        # Unload the module:
        # - change the current working directory back
        # - restore sys.path if we modified it
        # - delete the module
        os.chdir(old_cwd)
        if added_tests_dir:
            sys.path.remove(_TESTS_DIR)
        run_result = get_status_verbose(
            test_script_module.result,
            test_script_module.result_failed,
            input_source, "", Foreground.WHITE, ""
        )
        del sys.modules[module_name]
        return run_result

    options = [input_source]
    if what_to_test == "[syntax]":
        options.append("--syntax")
    if what_to_test == "[semantic]":
        options.append("--semantic")

    try:
        out, err, code = common.launch_compiler(compiler_path, options, silent)
    except FileNotFoundError:
        print(
            f"{Foreground.RED}{Style.BRIGHT}"
            f"\\\\[ ERROR ]// : {input_source} "
            f"-- compiler not found: {compiler_path}{Style.RESET_ALL}"
        )
        return TestResult(0, 0, 1, 0)
    except Exception as exc:
        error_type = type(exc).__name__
        print(
            f"{Foreground.RED}{Style.BRIGHT}"
            f"\\\\[ ERROR ]// : {input_source} "
            f"-- {error_type}: {exc}{Style.RESET_ALL}"
        )
        return TestResult(0, 0, 1, 0)
    okpreds, numpreds = common.execute_predicate_checks(out)
    output_ec = common.find_token_to_int(
        err.decode('utf-8'), r"error\s#\d*:"
    )
    fine = code == 0 and okpreds

    if expect_pass == fine:
        if not what_to_test == "[syntax]" and not expect_pass:
            # Further verify: semantic error tests must have valid syntax.
            if what_to_test == "[semantic]":
                options.pop()
            options.append("--syntax")
            out, err, syntax_code = common.launch_compiler(
                compiler_path, options, silent
            )
            if syntax_code != 0:
                return get_status_verbose(
                    0, 1, input_source, "", Foreground.RED,
                    "Invalid syntax in a 'semantic error' check. "
                    "These tests must have valid syntax."
                )

        preds_ok_msg = (
            f" ({numpreds} predicates ok)" if numpreds else ""
        )

        # Check if error code matches between azslc and azsl file.
        with io.open(input_source, 'r', encoding="latin-1") as f:
            azsl_code = f.read()
        error_code = common.find_token_to_int(azsl_code, r"#EC\s\d*")
        if error_code == -2:
            return get_status_verbose(
                0, 1, input_source, "", Foreground.RED,
                ' Error code does not contain an integer'
            )
        if error_code != -1 and output_ec != error_code:
            return get_status_verbose(
                0, 1, input_source, "", Foreground.RED,
                f' Error code returned from the compiler("{output_ec}") does not match '
                f'the one expected in the azsl file("{error_code}")'
            )

        # Error codes only supported for semantic errors.
        # Log files without a specific error code.
        if output_ec == 1 and not what_to_test == "[syntax]":
            return get_status_verbose(
                1, 1, input_source, "", Foreground.YELLOW,
                ' Missing ErrorCode'
            )

        return get_status_verbose(
            1, 0, input_source, preds_ok_msg, Foreground.WHITE, ""
        )
    else:
        expected_code = 0 if expect_pass else 1
        got_expected = expected_code == code
        code_message = (
            f"{Foreground.WHITE} got expected code {code}"
        )
        error_color = Foreground.WHITE if okpreds else Foreground.RED
        error_message = (
            f" predicates: "
            f"{'None' if numpreds == 0 else str(okpreds)}"
        )
        if not got_expected:
            code_message = (
                f" expected code {expected_code}"
                f" (but got {code})"
            )
        return get_status_verbose(
            0, 1, input_source, code_message,
            error_color, error_message
        )


def run_tests(
    tests_root: str,
    paths: List[str],
    compiler: str,
    verbose_level: int
) -> tuple:
    """Run all tests in the given paths and return (TestResult, were_only_files)."""
    compiler = os.path.abspath(compiler)
    num_total = TestResult(0, 0, 0, 0)
    args_are_only_files = all(
        os.path.isfile(join(tests_root, p)) for p in paths
    )

    if args_are_only_files:
        for f in paths:
            f = join(tests_root, f)
            what_to_test = "[everything]"
            if "syntax" in f.lower():
                what_to_test = "[syntax]"
            if "semantic" in f.lower():
                what_to_test = "[semantic]"
            if "samples" in f.lower():
                what_to_test = "[samples]"
            if verbose_level > 0:
                print(
                    f"{Foreground.MAGENTA}{Style.BRIGHT}"
                    f"== individual check {f} =={Style.RESET_ALL}"
                )
            num_total.add(test_file(
                "Advanced", f, "error" not in f.lower(),
                what_to_test, compiler,
                True if verbose_level < 2 else False
            ))
    else:
        for directory in paths:
            join_dir = join(tests_root, directory)
            for root, dirs, files in os.walk(join_dir):
                for f in files:
                    expect_pass = "error" not in root.lower()
                    what_to_test = "[everything]"
                    if "syntax" in root.lower():
                        what_to_test = "[syntax]"
                    if "semantic" in root.lower():
                        what_to_test = "[semantic]"
                    if "samples" in root.lower():
                        what_to_test = "[samples]"
                    advanced_test = "advanced" in root.lower()
                    if advanced_test:
                        if f.endswith(".py"):
                            if verbose_level > 0:
                                print(
                                    f"{Foreground.MAGENTA}{Style.BRIGHT}"
                                    f"== advanced script "
                                    f"{join(root, f)}{Style.RESET_ALL}"
                                )
                            num_total.add(test_file(
                                join_dir, join(root, f), expect_pass,
                                what_to_test, compiler,
                                True if verbose_level < 2 else False
                            ))
                    elif f.endswith(".azsl"):
                        what_must = (
                            "must [pass]" if expect_pass else "must [fail]"
                        )
                        if verbose_level > 0:
                            print(
                                f"{Foreground.MAGENTA}{Style.BRIGHT}"
                                f"== start to build {join(root, f)}"
                                f" == {what_must}{what_to_test}"
                                f"{Style.RESET_ALL}"
                            )
                        num_total.add(test_file(
                            join_dir, join(root, f), expect_pass,
                            what_to_test, compiler,
                            True if verbose_level < 2 else False
                        ))

    return (num_total, args_are_only_files)


def run_all(
    tests_root: str,
    paths: List[str],
    compiler: str,
    verbose_level: int
) -> 'TestResult':
    """
    Run all tests including per-platform tests.

    Returns the aggregated TestResult.
    """
    num_all_tests, args_were_only_files = run_tests(tests_root, paths, compiler, verbose_level)

    if args_were_only_files:
        return num_all_tests

    # Run per-platform tests (only for non-specific test runs).
    platforms_dir = join(tests_root, "../Platform/")
    if os.path.exists(platforms_dir):
        sub_dirs = [
            join(platforms_dir, d)
            for d in os.listdir(platforms_dir)
            if os.path.isdir(join(platforms_dir, d))
        ]
        for d in sub_dirs:
            print(
                f"{Foreground.WHITE}{Style.BRIGHT}"
                f"Per platform testing ({d}){Style.RESET_ALL}"
            )
            platform_tests = join(d, "tests")
            if os.path.isdir(platform_tests):
                num_all_tests.add(run_tests(platform_tests, paths, compiler, verbose_level)[0])
            else:
                print(
                    f"{Foreground.GREEN}{Style.BRIGHT}"
                    f"               ... no extra tests found."
                    f"{Style.RESET_ALL}"
                )
    return num_all_tests


if __name__ == "__main__":
    os.system('')  # Activate VT100 mode for Windows console.

    try:
        import yaml
    except ImportError:
        print(
            f"{Foreground.YELLOW}{Style.BRIGHT}"
            f"It seems your python environment lacks pyyaml. "
            f"Run first through project-root's \"test.and.py\" "
            f"(or pip install it){Style.RESET_ALL}"
        )
        if input("Continue (may result in false failures)? y/n:").lower() != "y":
            exit(0)

    parser = ArgumentParser()
    parser.add_argument(
        '--path', dest='path',
        type=str, nargs='*',
        help='the directories with test files (takes . if not provided)',
    )
    parser.add_argument(
        '--compiler', dest='compiler',
        type=str,
        default='azslc',
        help='the path to the compiler exe',
    )
    parser.add_argument(
        '--silent', dest='silent',
        action='store_true', default=False,
        help="not show compiler's stdout",
    )
    args = parser.parse_args()

    # Validate compiler path
    compiler = args.compiler
    if not os.path.isfile(compiler) and not shutil.which(compiler):
        print(
            f"{Foreground.RED}{Style.BRIGHT}"
            f"Error: compiler not found: '{compiler}'"
            f"{Style.RESET_ALL}"
        )
        print(
            f"{Foreground.WHITE}"
            f"Provide the path to the azslc executable via "
            f"--compiler <path>{Style.RESET_ALL}"
        )
        sys.exit(1)

    paths = args.path
    if paths is None:
        paths = ["."]
    verbose_level = 1 if args.silent else 2

    start_time = timer()
    try:
        num_all_tests = run_all(".", paths, compiler, verbose_level)
    except KeyboardInterrupt:
        print(
            f"\n{Foreground.YELLOW}{Style.BRIGHT}"
            f"Interrupted.{Style.RESET_ALL}"
        )
        sys.exit(130)
    except Exception as exc:
        print(
            f"\n{Foreground.RED}{Style.BRIGHT}"
            f"Fatal error: {type(exc).__name__}: {exc}{Style.RESET_ALL}"
        )
        traceback.print_exc()
        sys.exit(1)

    num_total = (
        num_all_tests.num_pass + num_all_tests.num_todo +
        num_all_tests.num_fail + num_all_tests.num_ec
    )
    end_time = timer()

    print(
        f"{Foreground.WHITE}{Style.BRIGHT}"
        f"FINISHED. Total = {num_total}{Style.RESET_ALL}"
    )
    print(
        f"{Foreground.GREEN}{Style.BRIGHT}"
        f"PASS = {num_all_tests.num_pass}"
        f"{Foreground.WHITE} /{num_total}{Style.RESET_ALL}"
    )
    print(
        f"{Foreground.YELLOW}{Style.BRIGHT}"
        f"TODO = {num_all_tests.num_todo}"
        f"{Foreground.WHITE} /{num_total}{Style.RESET_ALL}"
    )
    print(
        f"{Foreground.YELLOW}{Style.BRIGHT}"
        f"Missing EC = {num_all_tests.num_ec}"
        f"{Foreground.WHITE} /{num_total}{Style.RESET_ALL}"
    )
    print(
        f"{Foreground.RED}{Style.BRIGHT}"
        f"FAIL = {num_all_tests.num_fail}"
        f"{Foreground.WHITE} /{num_total}{Style.RESET_ALL}"
    )
    td = timedelta(seconds=(end_time - start_time))
    print(
        f"{Foreground.CYAN}{Style.BRIGHT}"
        f"Time taken: {Foreground.WHITE}{td}{Style.RESET_ALL}"
    )
