#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import os

import common
from common import Foreground, Background, Style


def exec_test(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    # check the specific stuff we want to verify with this test
    if ok:
        try:
            ok = symbols["Symbol '/A/A'"]['kind'] == 'Class'

            ok2 = symbols["Symbol '/A/A'"]["references"] == None  # no references for A::A

            ok3 = len(symbols["Symbol '/A'"]["references"]) == 1

            ok4 = symbols["Symbol '/A'"]["references"][0]["line"] == 11  # /A has one ref in the baselist of B line 11

            if not silent:
                if not ok:
                    print(f"{Foreground.RED}Couldn't verify symbol /A/A is a Class{Style.RESET_ALL}")

                if not ok2:
                    print(f"{Foreground.RED}Couldn't verify symbol /A/A is ot referenced{Style.RESET_ALL}")

                if not ok3:
                    print(f"{Foreground.RED}Couldn't verify /A has 1 reference{Style.RESET_ALL}")

                if not ok4:
                    print(f"{Foreground.RED}Couldn't verify /A's seenat is at line 11{Style.RESET_ALL}")

            ok = ok and ok2 and ok3 and ok4

        except Exception as e:
            print(f"{Foreground.RED}Err: dumpsym didn't match expectations{Style.RESET_ALL}", e)
            ok = False
    return ok


result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    if exec_test(os.path.join(work_dir, "inheritance-lookup-seenats.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
