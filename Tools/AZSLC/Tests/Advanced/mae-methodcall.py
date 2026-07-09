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


def verify_option_costs(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--options"])
    if ok:
        predicates = [
            # check all references of func()
            lambda: j["ShaderOptions"][0]["name"] == "o",
            lambda: j["ShaderOptions"][0]["costImpact"] == 54,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}option expected cost check...{Style.RESET_ALL}")
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

    if verify_option_costs(os.path.join(work_dir, "mae-methodcall.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
