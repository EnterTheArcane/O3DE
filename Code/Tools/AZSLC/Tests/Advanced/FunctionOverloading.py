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

"""  the test looks like this↓ so we want to verify symbols and their references
int func(int i) {..}  // 1

int func(float f) {..}  // 2

float4 main() : SV_Target0
{
    g_func(1);  // ref to 1
    g_func(1.5);  // ref to 2
"""


def exec_test(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    # check the specific stuff we want to verify with this test
    if ok:
        try:
            ok = symbols["Symbol '/g_func'"]['kind'] == 'OverloadSet'
            ok = ok and symbols["Symbol '/g_func(?int)'"]['kind'] == 'Function'
            ok = ok and symbols["Symbol '/g_func(?float)'"]['kind'] == 'Function'

            ok = ok and symbols["Symbol '/g_func(?int)'"]['return type']['core']['name'] == '?int'

            ok = ok and symbols["Symbol '/g_func(?int)'"]['parameters'][0]["name"] == 'i'
            ok = ok and symbols["Symbol '/g_func(?int)'"]['parameters'][0]["type"]["core"]["name"] == '?int'
            ok = ok and symbols["Symbol '/g_func(?float)'"]['parameters'][0]["name"] == 'f'
            ok = ok and symbols["Symbol '/g_func(?float)'"]['parameters'][0]["type"]["core"]["name"] == '?float'

            ok = ok and symbols["Symbol '/g_func(?int)/i'"]['kind'] == 'Variable'
            ok = ok and symbols["Symbol '/g_func(?int)/i'"]['type']['core']['name'] == '?int'
            ok = ok and symbols["Symbol '/g_func(?float)/f'"]['kind'] == 'Variable'
            ok = ok and symbols["Symbol '/g_func(?float)/f'"]['type']['core']['name'] == '?float'

            ok = ok and len(symbols["Symbol '/f'"]['references']) == 1  # one unsolved ref, because f()
            ok = ok and len(symbols["Symbol '/f(?int,?float)'"]['references']) == 2  # one decl site. one call site
            ok = ok and len(symbols["Symbol '/f(?int)'"]['references']) == 2  # one decl site. one call site

            ok = ok and symbols["Symbol '/f(?int,?float)'"]['references'][1]['line'] == 28
            ok = ok and symbols["Symbol '/f(?int)'"]['references'][1]['line'] == 27
            ok = ok and symbols["Symbol '/f'"]['references'][0]['line'] == 26

            if not ok:
                print(f"{Style.DIM}{Foreground.YELLOW}ERR: all expected symbol founds, but their semantic understanding seems off{Style.RESET_ALL}")
            else:
                print(f"{Style.BRIGHT}OK! all symbols semantics correctly understood{Style.RESET_ALL}")
        except Exception as e:
            print(f"{Foreground.RED}Err: Parsed --dumpsym may lack some expected keys{Style.RESET_ALL}", e)
    return 1 if ok else 0


result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    if exec_test(os.path.join(work_dir, "FunctionOverloading.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
