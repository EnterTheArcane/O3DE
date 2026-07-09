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
    """
    The purpose of this test is to validate proper functionality of the compiler option --no-ms.
    --no-ms is an option that triggers automatic conversion of MultiSampling related Texture variables, function calls
    and system semantics, to their non-MultiSampling version. Example Texture2DMS to Texture2D, Texture2DMSArray to Texture2DArray, etc.
    """
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd().replace('\\', '/')

    # First let's make sure that WITHOUT --no-ms, the shader compiles fine and all the MultiSampling related variables and function call
    # remain unchanged.
    azsl_file = os.path.abspath(os.path.join(work_dir, "./texture2DMS-to-texture2D.azsl"))

    fail_list = []
    pattern_file = os.path.abspath(os.path.join(work_dir, "./texture2DMS-to-texture2D.txt"))
    if common.verify_emission_pattern(azsl_file, pattern_file, compiler, silent, []):
        result += 1
    else:
        fail_list.append(pattern_file)
        result_failed += 1

    pattern_file = os.path.abspath(os.path.join(work_dir, "./texture2DMS-to-texture2D-noms.txt"))
    if common.verify_emission_pattern(azsl_file, pattern_file, compiler, silent, ["--no-ms"]):
        result += 1
    else:
        fail_list.append(pattern_file)
        result_failed += 1

    if not silent and len(fail_list) > 0:
        print(f"{Style.BRIGHT}{Foreground.RED}failed files: {Foreground.WHITE}{fail_list}{Style.RESET_ALL}")


if __name__ == "__main__":
    assert "please call from runner.py"
