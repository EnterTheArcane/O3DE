#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

"""
CTest-compatible test runner for AZSLC tests.

Wraps the existing test infrastructure so individual tests can be
invoked by CTest.  Supports four modes:

  simple          Compile an .azsl file and check exit code + predicates.
  emission        Compile an .azsl file and verify output against .txt patterns.
  emission-error  Compile an .azsl file expecting failure with error-code check.
  advanced        Import and run an Advanced Python test script.

Exit codes:
  0  - Test passed.
  1  - Test failed.
  77 - Test skipped (WIP / TODO).  Used with CTest SKIP_RETURN_CODE.
"""

import argparse
import importlib
import importlib.util
import io
import os
import sys

# Make the Tests/ directory importable so that the Shared package
# (common module) can be found regardless of the
# working directory CTest happens to use.
TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
if TESTS_DIR not in sys.path:
    sys.path.insert(0, TESTS_DIR)

import common

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_SKIP = 77  # Convention used with CTest SKIP_RETURN_CODE


def handle_simple(args):
    """Compile an .azsl file and verify the outcome matches expectations."""
    options = [args.file]
    if args.compiler_flag:
        options.append(args.compiler_flag)

    out, err, code = common.launch_compiler(args.compiler, options, silent=True)
    okpreds, numpreds = common.execute_predicate_checks(out)
    fine = (code == 0) and okpreds

    if args.expect_fail:
        if not fine:
            # Semantic error tests must still have valid *syntax*.
            if args.compiler_flag == "--semantic":
                syn_opts = [args.file, "--syntax"]
                _, _, syn_code = common.launch_compiler(
                    args.compiler, syn_opts, silent=True
                )
                if syn_code != 0:
                    print(
                        f"FAIL: Semantic error test has invalid syntax: "
                        f"{args.file}"
                    )
                    return EXIT_FAIL

            # Verify error code if #EC annotation is present in the source.
            with io.open(args.file, "r", encoding="latin-1") as f:
                azsl_code = f.read()
            expected_ec = common.find_token_to_int(azsl_code, r"#EC\s\d*")
            if expected_ec == -2:
                print(
                    f"FAIL: #EC annotation is not a valid integer: {args.file}"
                )
                return EXIT_FAIL
            if expected_ec not in (-1,):
                actual_ec = common.find_token_to_int(
                    err.decode("utf-8"), r"error\s#\d*:"
                )
                if actual_ec != expected_ec:
                    print(
                        f"FAIL: Error code mismatch in {args.file} "
                        f"(expected {expected_ec}, got {actual_ec})"
                    )
                    return EXIT_FAIL

            print(f"PASS: {args.file} (failed as expected)")
            return EXIT_PASS
        else:
            print(f"FAIL: Expected compilation to fail: {args.file}")
            return EXIT_FAIL
    else:
        if fine:
            extra = f" ({numpreds} predicates ok)" if numpreds else ""
            print(f"PASS: {args.file}{extra}")
            return EXIT_PASS
        else:
            print(f"FAIL: Expected compilation to pass: {args.file}")
            if code != 0:
                sys.stderr.write(err.decode("utf-8", errors="replace"))
            if not okpreds:
                print("  Predicate checks failed")
            return EXIT_FAIL


# Emission Mode
def handle_emission(args):
    """Compile and verify the emitted code against .txt pattern files."""
    common.fail_list = []
    result = common.verify_emission_patterns(
        args.file, args.compiler, silent=True, arg_list=[]
    )
    if result > 0:
        print(f"PASS: {args.file} ({result} pattern file(s) verified)")
        return EXIT_PASS
    print(f"FAIL: Emission pattern verification failed: {args.file}")
    common.print_failed_test_list(False)
    return EXIT_FAIL


def handle_emission_error(args):
    """Compile expecting failure and verify the error code."""
    common.fail_list = []
    result = common.compile_and_expect_error(
        args.file, args.compiler, silent=True, arg_list=[]
    )
    if result > 0:
        print(f"PASS: {args.file} (failed with expected error code)")
        return EXIT_PASS
    print(f"FAIL: Emission error test: {args.file}")
    common.print_failed_test_list(False)
    return EXIT_FAIL


def handle_advanced(args):
    """Import and execute an Advanced Python test script."""
    test_file = os.path.abspath(args.file)
    module_dir = os.path.dirname(test_file)
    module_name = os.path.splitext(os.path.basename(test_file))[0]

    # Put the module's own directory on sys.path (mirrors runner.py).
    if module_dir not in sys.path:
        sys.path.insert(0, module_dir)

    # Set the CWD *before* loading the module because several Advanced
    # scripts do ``sys.path.append("..")`` at module level and that must
    # resolve relative to the script's directory.
    old_cwd = os.getcwd()
    os.chdir(module_dir)
    try:
        spec = importlib.util.spec_from_file_location(module_name, test_file)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)

        if not hasattr(module, "do_tests"):
            print(f"SKIP: No do_tests() function in {args.file}")
            return EXIT_SKIP

        module.do_tests(args.compiler, True)
        passed = getattr(module, "result", 0)
        failed = getattr(module, "result_failed", 0)
    finally:
        os.chdir(old_cwd)
        sys.path[:] = [p for p in sys.path if p != module_dir]
        sys.modules.pop(module_name, None)

    if failed > 0:
        print(
            f"FAIL: {failed} sub-test(s) failed, {passed} passed "
            f"- {args.file}"
        )
        return EXIT_FAIL
    print(f"PASS: {passed} sub-test(s) passed - {args.file}")
    return EXIT_PASS


MODE_HANDLERS = {
    "simple": handle_simple,
    "emission": handle_emission,
    "emission-error": handle_emission_error,
    "advanced": handle_advanced,
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--compiler",
        required=True,
        help="Path to the azslc binary"
    )
    parser.add_argument(
        "--file",
        required=True,
        help="Test file (.azsl or .py)"
    )
    parser.add_argument(
        "--mode",
        required=True,
        choices=MODE_HANDLERS.keys(),
        help="Test mode",
    )
    parser.add_argument(
        "--expect-fail",
        action="store_true",
        default=False,
        help="Expect the compilation to fail (simple mode)",
    )
    parser.add_argument(
        "--compiler-flag",
        default=None,
        help="Extra compiler flag, e.g. --syntax or --semantic",
    )
    parser.add_argument(
        "--wip",
        action="store_true",
        default=False,
        help="Treat failures as skipped (WIP / TODO)",
    )

    args = parser.parse_args()
    args.compiler = os.path.abspath(args.compiler)

    rc = MODE_HANDLERS[args.mode](args)

    # WIP tests: convert failures to skips so CTest does not count them as regressions.
    if rc == EXIT_FAIL and args.wip:
        print(f"WIP/TODO - failure treated as skip: {args.file}")
        rc = EXIT_SKIP

    sys.exit(rc)


if __name__ == "__main__":
    main()
