#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import os
import os.path

import common
from common import Foreground, Background, Style

result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd().replace('\\', '/')

    for base, dirs, files in os.walk(os.path.join(work_dir, "../Emission/")):
        for f in files:
            if f.endswith(".azsl"):
                subdir_name = os.path.basename(base)
                complete_path = os.path.join(base, f)
                if subdir_name != "Error":
                    success = common.verify_emission_patterns(complete_path, compiler, silent, []) > 0
                else:
                    success = common.compile_and_expect_error(complete_path, compiler, silent, []) > 0
                if success:
                    result += 1
                else:
                    result_failed += 1
                    if not silent: print(f"{Foreground.RED}{Style.BRIGHT}failed {Style.NORMAL}{f}{Foreground.RESET}")

    common.print_failed_test_list(silent)


if __name__ == "__main__":
    assert "please call from runner.py"
