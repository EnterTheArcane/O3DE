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


def verify_output_formats(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--om"])

    if ok:
        predicates = [
            # check all references of func()
            lambda: len(j["outputLayouts"]) == 12,

            lambda: len(j["outputLayouts"][0]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][0]["entry"] == "MainPS_1_1",
            lambda: j["outputLayouts"][0]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][0]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][1]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][1]["entry"] == "MainPS_1_2",
            lambda: j["outputLayouts"][1]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][1]["renderTargets"][0]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][1]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][2]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][2]["entry"] == "MainPS_1_3",
            lambda: j["outputLayouts"][2]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][2]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][2]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][3]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][3]["entry"] == "MainPS_1_4",
            lambda: j["outputLayouts"][3]["renderTargets"][0]["semanticName"] == "SV_Depth",
            lambda: j["outputLayouts"][3]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][3]["renderTargets"][0]["format"] == "R32",

            lambda: len(j["outputLayouts"][4]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][4]["entry"] == "MainPS_1_5",
            lambda: j["outputLayouts"][4]["renderTargets"][0]["semanticName"] == "SV_Depth",
            lambda: j["outputLayouts"][4]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][4]["renderTargets"][0]["format"] == "R32",

            lambda: len(j["outputLayouts"][5]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][5]["entry"] == "MainPS_1_8",
            lambda: j["outputLayouts"][5]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][5]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][5]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][6]["renderTargets"]) == 1,
            lambda: j["outputLayouts"][6]["entry"] == "MainPS_2_1",
            lambda: j["outputLayouts"][6]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][6]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][6]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][7]["renderTargets"]) == 8,
            lambda: j["outputLayouts"][7]["entry"] == "MainPS_2_2",
            lambda: j["outputLayouts"][7]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][7]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][1]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][7]["renderTargets"][1]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][2]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][2]["semanticIndex"] == 2,
            lambda: j["outputLayouts"][7]["renderTargets"][2]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][3]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][3]["semanticIndex"] == 3,
            lambda: j["outputLayouts"][7]["renderTargets"][3]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][4]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][4]["semanticIndex"] == 4,
            lambda: j["outputLayouts"][7]["renderTargets"][4]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][5]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][5]["semanticIndex"] == 5,
            lambda: j["outputLayouts"][7]["renderTargets"][5]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][6]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][6]["semanticIndex"] == 6,
            lambda: j["outputLayouts"][7]["renderTargets"][6]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][7]["renderTargets"][7]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][7]["renderTargets"][7]["semanticIndex"] == 7,
            lambda: j["outputLayouts"][7]["renderTargets"][7]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][8]["renderTargets"]) == 8,
            lambda: j["outputLayouts"][8]["entry"] == "MainPS_2_3",
            lambda: j["outputLayouts"][8]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][0]["semanticIndex"] == 3,
            lambda: j["outputLayouts"][8]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][1]["semanticIndex"] == 4,
            lambda: j["outputLayouts"][8]["renderTargets"][1]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][2]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][2]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][8]["renderTargets"][2]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][3]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][3]["semanticIndex"] == 6,
            lambda: j["outputLayouts"][8]["renderTargets"][3]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][4]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][4]["semanticIndex"] == 7,
            lambda: j["outputLayouts"][8]["renderTargets"][4]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][5]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][5]["semanticIndex"] == 5,
            lambda: j["outputLayouts"][8]["renderTargets"][5]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][6]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][6]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][8]["renderTargets"][6]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][8]["renderTargets"][7]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][8]["renderTargets"][7]["semanticIndex"] == 2,
            lambda: j["outputLayouts"][8]["renderTargets"][7]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][9]["renderTargets"]) == 8,
            lambda: j["outputLayouts"][9]["entry"] == "MainPS_2_3A",
            lambda: j["outputLayouts"][9]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][9]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][1]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][9]["renderTargets"][1]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][2]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][2]["semanticIndex"] == 2,
            lambda: j["outputLayouts"][9]["renderTargets"][2]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][3]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][3]["semanticIndex"] == 3,
            lambda: j["outputLayouts"][9]["renderTargets"][3]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][4]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][4]["semanticIndex"] == 4,
            lambda: j["outputLayouts"][9]["renderTargets"][4]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][5]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][5]["semanticIndex"] == 5,
            lambda: j["outputLayouts"][9]["renderTargets"][5]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][6]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][6]["semanticIndex"] == 6,
            lambda: j["outputLayouts"][9]["renderTargets"][6]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][9]["renderTargets"][7]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][9]["renderTargets"][7]["semanticIndex"] == 7,
            lambda: j["outputLayouts"][9]["renderTargets"][7]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][10]["renderTargets"]) == 2,
            lambda: j["outputLayouts"][10]["entry"] == "MainPS1",
            lambda: j["outputLayouts"][10]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][10]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][10]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][10]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][10]["renderTargets"][1]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][10]["renderTargets"][1]["format"] == "R16G16B16A16_FLOAT",

            lambda: len(j["outputLayouts"][11]["renderTargets"]) == 11,
            lambda: j["outputLayouts"][11]["entry"] == "MainPS_All",
            lambda: j["outputLayouts"][11]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][11]["renderTargets"][0]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][1]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][11]["renderTargets"][1]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][2]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][2]["semanticIndex"] == 2,
            lambda: j["outputLayouts"][11]["renderTargets"][2]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][3]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][3]["semanticIndex"] == 3,
            lambda: j["outputLayouts"][11]["renderTargets"][3]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][4]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][4]["semanticIndex"] == 4,
            lambda: j["outputLayouts"][11]["renderTargets"][4]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][5]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][5]["semanticIndex"] == 5,
            lambda: j["outputLayouts"][11]["renderTargets"][5]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][6]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][6]["semanticIndex"] == 6,
            lambda: j["outputLayouts"][11]["renderTargets"][6]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][7]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][11]["renderTargets"][7]["semanticIndex"] == 7,
            lambda: j["outputLayouts"][11]["renderTargets"][7]["format"] == "R16G16B16A16_FLOAT",
            lambda: j["outputLayouts"][11]["renderTargets"][8]["semanticName"] == "SV_Coverage",
            lambda: j["outputLayouts"][11]["renderTargets"][8]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][11]["renderTargets"][8]["format"] == "R32",
            lambda: j["outputLayouts"][11]["renderTargets"][9]["semanticName"] == "SV_StencilRef",
            lambda: j["outputLayouts"][11]["renderTargets"][9]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][11]["renderTargets"][9]["format"] == "R32",
            lambda: j["outputLayouts"][11]["renderTargets"][10]["semanticName"] == "SV_Depth",
            lambda: j["outputLayouts"][11]["renderTargets"][10]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][11]["renderTargets"][10]["format"] == "R32",
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_output_formats_attr(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--om"])

    if ok:
        predicates = [
            # check all references of func()
            lambda: len(j["outputLayouts"]) == 2,

            lambda: len(j["outputLayouts"][0]["renderTargets"]) == 8,
            lambda: j["outputLayouts"][0]["entry"] == "MainPS_All",
            lambda: j["outputLayouts"][0]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][0]["renderTargets"][0]["format"] == "R32",
            lambda: j["outputLayouts"][0]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][1]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][0]["renderTargets"][1]["format"] == "R32G32",
            lambda: j["outputLayouts"][0]["renderTargets"][2]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][2]["semanticIndex"] == 2,
            lambda: j["outputLayouts"][0]["renderTargets"][2]["format"] == "R32A32",
            lambda: j["outputLayouts"][0]["renderTargets"][3]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][3]["semanticIndex"] == 3,
            lambda: j["outputLayouts"][0]["renderTargets"][3]["format"] == "R16G16B16A16_UNORM",
            lambda: j["outputLayouts"][0]["renderTargets"][4]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][4]["semanticIndex"] == 4,
            lambda: j["outputLayouts"][0]["renderTargets"][4]["format"] == "R16G16B16A16_SNORM",
            lambda: j["outputLayouts"][0]["renderTargets"][5]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][5]["semanticIndex"] == 5,
            lambda: j["outputLayouts"][0]["renderTargets"][5]["format"] == "R16G16B16A16_UINT",
            lambda: j["outputLayouts"][0]["renderTargets"][6]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][6]["semanticIndex"] == 6,
            lambda: j["outputLayouts"][0]["renderTargets"][6]["format"] == "R16G16B16A16_SINT",
            lambda: j["outputLayouts"][0]["renderTargets"][7]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][0]["renderTargets"][7]["semanticIndex"] == 7,
            lambda: j["outputLayouts"][0]["renderTargets"][7]["format"] == "R32G32B32A32",

            lambda: len(j["outputLayouts"][1]["renderTargets"]) == 4,
            lambda: j["outputLayouts"][1]["entry"] == "MainPS_Half",
            lambda: j["outputLayouts"][1]["renderTargets"][0]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][1]["renderTargets"][0]["semanticIndex"] == 0,
            lambda: j["outputLayouts"][1]["renderTargets"][0]["format"] == "R32",
            lambda: j["outputLayouts"][1]["renderTargets"][1]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][1]["renderTargets"][1]["semanticIndex"] == 1,
            lambda: j["outputLayouts"][1]["renderTargets"][1]["format"] == "R32G32",
            lambda: j["outputLayouts"][1]["renderTargets"][2]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][1]["renderTargets"][2]["semanticIndex"] == 2,
            lambda: j["outputLayouts"][1]["renderTargets"][2]["format"] == "R32A32",
            lambda: j["outputLayouts"][1]["renderTargets"][3]["semanticName"] == "SV_Target",
            lambda: j["outputLayouts"][1]["renderTargets"][3]["semanticIndex"] == 3,
            lambda: j["outputLayouts"][1]["renderTargets"][3]["format"] == "R16G16B16A16_UNORM",
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
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

    if verify_output_formats(os.path.join(work_dir, "../Samples/PixelShaderOutput.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_output_formats_attr(os.path.join(work_dir, "../Samples/PixelShaderOutputAttributes.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
