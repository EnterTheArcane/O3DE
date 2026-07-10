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


def verify_input_layouts(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--ia"])

    if ok:
        predicates = [
            # check all references of func()
            lambda: len(j["inputLayouts"]) == 5, lambda: j["inputLayouts"][0]["entry"] == "MainCS1",
            lambda: len(j["inputLayouts"][0]["numthreads"]) == 3, lambda: j["inputLayouts"][0]["numthreads"][0] == 1,
            lambda: j["inputLayouts"][0]["numthreads"][1] == 1, lambda: j["inputLayouts"][0]["numthreads"][2] == 1,
            lambda: j["inputLayouts"][1]["entry"] == "MainCS2", lambda: len(j["inputLayouts"][1]["numthreads"]) == 3,
            lambda: j["inputLayouts"][1]["numthreads"][0] == 8, lambda: j["inputLayouts"][1]["numthreads"][1] == 1,
            lambda: j["inputLayouts"][1]["numthreads"][2] == 1, lambda: j["inputLayouts"][2]["entry"] == "MainCS3",
            lambda: len(j["inputLayouts"][2]["numthreads"]) == 3, lambda: j["inputLayouts"][2]["numthreads"][0] == 16,
            lambda: j["inputLayouts"][2]["numthreads"][1] == 4, lambda: j["inputLayouts"][2]["numthreads"][2] == 1,
            lambda: j["inputLayouts"][3]["entry"] == "MainCS4", lambda: len(j["inputLayouts"][3]["numthreads"]) == 3,
            lambda: j["inputLayouts"][3]["numthreads"][0] == 1, lambda: j["inputLayouts"][3]["numthreads"][1] == 1,
            lambda: j["inputLayouts"][3]["numthreads"][2] == 64, lambda: j["inputLayouts"][4]["entry"] == "MainVS1",
            lambda: "numthreads" not in j["inputLayouts"][4].keys(),
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)

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

    if verify_input_layouts(os.path.join(work_dir, "compute-entries.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
