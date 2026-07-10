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
    This test validates:
        1. unbounded-arrays-unique-idx-should-pass.azsl with --unique-idx used to fail in v1.7.19, should pass now.
        2. unbounded-arrays-unique-idx-should-pass-2srgs.azsl with --unique-idx should pass too, because each unbounded array is in a unique register space.
    """
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd().replace('\\', '/')

    # expect success when using --unique-idx
    sample_file_path = os.path.abspath(os.path.join(work_dir, "../Semantic/unbounded-arrays-unique-idx-should-pass.azsl"))
    if common.verify_emission_patterns(sample_file_path, compiler, silent, ["--unique-idx", "--namespace=dx"]):
        result += 1
    else:
        result_failed += 1

    # expect success when using --unique-idx
    sample_file_path = os.path.abspath(os.path.join(work_dir, "../Semantic/unbounded-arrays-unique-idx-should-pass-2srgs.azsl"))
    if common.verify_emission_patterns(sample_file_path, compiler, silent, ["--unique-idx", "--namespace=dx"]):
        result += 1
    else:
        result_failed += 1

    common.print_failed_test_list(silent)


if __name__ == "__main__":
    assert "please call from runner.py"
