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

"""
Validates having multiple attribute namespaces in the same file.
"""

result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    if common.verify_emission_pattern("MultipleAttributeNamespaces.azsl", "MultipleAttributeNamespaces.txt", compiler, silent,
                                        ["--namespace=mt", "--namespace=vk"]):
        result += 1
    else:
        result_failed += 1

    if common.compile_and_expect_error("MultipleAttributeNamespaces.azsl", compiler, silent, ["--namespace=mt,vk"]):
        result += 1
    else:
        result_failed += 1

    if common.compile_and_expect_error("MultipleAttributeNamespaces.azsl", compiler, silent, ["--namespace=mt&vk"]):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
