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
            lambda: len(j["inputLayouts"]) == 3,

            lambda: len(j["inputLayouts"][0]["streams"]) == 3,
            lambda: j["inputLayouts"][0]["streams"][0]["name"] == "m_position",
            lambda: j["inputLayouts"][0]["streams"][0]["semanticName"] == "POSITION",
            lambda: j["inputLayouts"][0]["streams"][0]["systemValue"] == False,
            lambda: j["inputLayouts"][0]["streams"][1]["name"] == "m_color",
            lambda: j["inputLayouts"][0]["streams"][1]["dimensions"][0] == 4,
            lambda: j["inputLayouts"][0]["streams"][1]["semanticName"] == "COLOR",
            lambda: j["inputLayouts"][0]["streams"][1]["systemValue"] == False,
            lambda: j["inputLayouts"][0]["streams"][2]["name"] == "vtxIndex",
            lambda: j["inputLayouts"][0]["streams"][2]["semanticName"] == "SV_VertexID",
            lambda: j["inputLayouts"][0]["streams"][2]["systemValue"] == True,

            lambda: len(j["inputLayouts"][1]["streams"]) == 10,
            lambda: j["inputLayouts"][1]["streams"][0]["name"] == "m_position",
            lambda: j["inputLayouts"][1]["streams"][0]["semanticName"] == "POSITION",
            lambda: j["inputLayouts"][1]["streams"][0]["systemValue"] == False,
            lambda: j["inputLayouts"][1]["streams"][9]["name"] == "instId",
            lambda: j["inputLayouts"][1]["streams"][9]["semanticName"] == "SV_InstanceID",
            lambda: j["inputLayouts"][1]["streams"][9]["systemValue"] == True,
            lambda: j["inputLayouts"][1]["streams"][8]["name"] == "vtxIndex",
            lambda: j["inputLayouts"][1]["streams"][8]["semanticName"] == "SV_VertexID",
            lambda: j["inputLayouts"][1]["streams"][8]["systemValue"] == True,

            lambda: len(j["inputLayouts"][2]["streams"]) == 2,
            lambda: j["inputLayouts"][2]["streams"][0]["name"] == "position",
            lambda: j["inputLayouts"][2]["streams"][0]["semanticName"] == "POSITION",
            lambda: j["inputLayouts"][2]["streams"][0]["systemValue"] == False,
            lambda: j["inputLayouts"][2]["streams"][1]["name"] == "color",
            lambda: j["inputLayouts"][2]["streams"][1]["semanticName"] == "COLOR",
            lambda: j["inputLayouts"][2]["streams"][1]["systemValue"] == False,
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

    if verify_input_layouts(os.path.join(work_dir, "input-assembler.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
