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


def verify(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--namespace=vk", "--srg", "--max-spaces=2"])

    if ok:
        predicates = [

            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["index-merged"] == 0,
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["space-merged"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["index-merged"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["space-merged"] == 0,

            lambda: j["ShaderResourceGroups"][1]["bufferForSRGConstants"]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["bufferForSRGConstants"]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["bufferForSRGConstants"]["index-merged"] == 0,
            lambda: j["ShaderResourceGroups"][1]["bufferForSRGConstants"]["space-merged"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["index-merged"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["space-merged"] == 1,

            lambda: j["ShaderResourceGroups"][2]["bufferForSRGConstants"]["index"] == 0,
            lambda: j["ShaderResourceGroups"][2]["bufferForSRGConstants"]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["bufferForSRGConstants"]["index-merged"] == 1,
            lambda: j["ShaderResourceGroups"][2]["bufferForSRGConstants"]["space-merged"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForSamplers"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][2]["inputsForSamplers"][0]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForSamplers"][0]["index-merged"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForSamplers"][0]["space-merged"] == 1,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}reflected binding info verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    if verify(os.path.join(work_dir, "srg-layout-merged.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
