#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import os

import common

result = 0  # to define for subtests
result_failed = 0  # to define for subtests


# delete artefacts from previous compilations
def clean_artifacts():
    if os.path.exists("simple-surface.hlsl"): os.remove("simple-surface.hlsl")
    if os.path.exists("simple-surface.ia.json"): os.remove("simple-surface.ia.json")
    if os.path.exists("simple-surface.om.json"): os.remove("simple-surface.om.json")
    if os.path.exists("simple-surface.srg.json"): os.remove("simple-surface.srg.json")
    if os.path.exists("simple-surface.options.json"): os.remove("simple-surface.options.json")
    if os.path.exists("simple-surface.bindingdep.json"): os.remove("simple-surface.bindingdep.json")


def do_tests(compiler_path, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    clean_artifacts()

    if common.build_and_get("simple-surface.azsl", compiler_path, silent, ["-o", "simple-surface.hlsl", "--full"]):
        result += 1
    else:
        result_failed += 1

    # check existence of HLSL output file
    if os.stat("simple-surface.hlsl").st_size != 0:
        result += 1
    else:
        result_failed += 1

    # check existence of JSON output files

    if os.stat("simple-surface.ia.json").st_size != 0:
        result += 1
    else:
        result_failed += 1

    if os.stat("simple-surface.om.json").st_size != 0:
        result += 1
    else:
        result_failed += 1

    if os.stat("simple-surface.srg.json").st_size != 0:
        result += 1
    else:
        result_failed += 1

    if os.stat("simple-surface.options.json").st_size != 0:
        result += 1
    else:
        result_failed += 1

    if os.stat("simple-surface.bindingdep.json").st_size != 0:
        result += 1
    else:
        result_failed += 1

    clean_artifacts()


if __name__ == "__main__":
    assert "please call from runner.py"
