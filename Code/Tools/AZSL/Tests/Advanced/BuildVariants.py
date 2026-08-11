#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import os

result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    azsl_shader_file_list = ["../Samples/Variants.azsl", "../Samples/Variantsreorder.azsl"]

    # Here's one bonus success for you.
    # This script is deprecated - it's being moved to the Platform/ folders and will be deleted after merging
    result += 1


if __name__ == "__main__":
    assert "please call from runner.py"
