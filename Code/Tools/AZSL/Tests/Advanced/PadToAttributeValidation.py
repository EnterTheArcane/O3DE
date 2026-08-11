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
Validates the functionality of the [[pad_to(N)]] attribute for struct, class and SRGs.
"""


def check_structured_buffer_vs_constant_buffer_padding(file, compiler_path, silent, expected_size):
    # Compile the shader with --srg and check that the final size of the struct
    # is 256 for both the StructureBuffer<MyStruct> DemoSrg::m_mySB, and MyStruct DemoSrg::m_myStruct
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg"])
    if ok:
        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}check_structured_buffer_vs_constant_buffer_padding: Verifying struct sizes...{Style.RESET_ALL}")

        predicates = [
            lambda expected=expected_size: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == expected,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "StructuredBuffer<MyStruct>",

            lambda expected=expected_size: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteSize"] == expected,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["typeName"] == "/MyStruct",
        ]

        ok = common.verify_all_predicates(predicates, j, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} check_structured_buffer_vs_constant_buffer_padding: All sizes were the same.{Style.RESET_ALL}")
    return ok


def check_srg_padding(file, compiler_path, silent, expected_size):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg"])
    if ok:
        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}check_srg_padding: Verifying SRG sizes...{Style.RESET_ALL}")

        # The offset + size of the last variable in each SRG must match the value of @expectedSize.
        srg1_last_variable_offset = j["ShaderResourceGroups"][0]["inputsForSRGConstants"][-1]["constantByteOffset"]
        srg1_last_variable_size = j["ShaderResourceGroups"][0]["inputsForSRGConstants"][-1]["constantByteSize"]
        srg1_size = srg1_last_variable_offset + srg1_last_variable_size

        srg2_last_variable_offset = j["ShaderResourceGroups"][1]["inputsForSRGConstants"][-1]["constantByteOffset"]
        srg2_last_variable_size = j["ShaderResourceGroups"][1]["inputsForSRGConstants"][-1]["constantByteSize"]
        srg2_size = srg2_last_variable_offset + srg2_last_variable_size

        ok = (srg1_size == srg2_size) and (srg1_size == expected_size)
        if not ok and not silent:
            error_msg = f"Was expecting both SRG sizes to be {expected_size}, instead got SRG1 size={srg1_size} and SRG2 size={srg2_size}"
            print(f"{Foreground.RED}FAIL ({error_msg}):{Style.RESET_ALL}")

        if ok and not silent:
            print(f"{Style.BRIGHT}OK! check_srg_padding: All sizes were the same.{Style.RESET_ALL}")
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

    if not silent: print("testing [[pad_to(256)]] attribute...")
    if check_structured_buffer_vs_constant_buffer_padding(os.path.join(work_dir, "StructPadTo256.azsl"), compiler, silent, 256):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing [[pad_to(252)]] attribute...")
    if check_structured_buffer_vs_constant_buffer_padding(os.path.join(work_dir, "StructPadTo252.azsl"), compiler, silent, 252):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing [[pad_to(N)]] for SRGs...")
    if check_srg_padding(os.path.join(work_dir, "SrgPadTo256.azsl"), compiler, silent, 256):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
