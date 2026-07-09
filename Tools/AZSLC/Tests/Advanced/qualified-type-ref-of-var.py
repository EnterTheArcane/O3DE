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


def exec_test(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    # check the specific stuff we want to verify with this test
    if ok:
        try:
            ok = symbols["Symbol '/func()/d'"]['type']['core']['name'] == '/C/D'

            ok2 = symbols["Symbol '/func()/d2'"]['type']['core']['name'] == '/C/D'

            ok3 = symbols["Symbol '/func()/d3'"]['type']['core']['name'] == '/func()/S'

            mh34_type = symbols["Symbol '/func()/mh34'"]['type']['core']['name']

            rwbf_type = symbols["Symbol '/func()/rwbf'"]['type']

            sb_inl_type = symbols["Symbol '/func()/sbInl'"]['type']

            ok4 = True  # TODO (wip)

            ok5 = rwbf_type['core']['name'] == "?RWBuffer" and rwbf_type['generic']['name'] == "?float4x4"

            ok6 = sb_inl_type['core']['name'] == "?StructuredBuffer" and sb_inl_type['generic']['name'] == "/func()/Inl" and sb_inl_type['generic'][
                'tclass'] == "Struct"

            if not silent:
                if not ok:
                    print(f"{Foreground.RED}Couldn't verify symbol /C/D is type of /func()/d{Style.RESET_ALL}")
                else:
                    print(f"{Style.BRIGHT}/C/D is type of /func()/d. great !{Style.RESET_ALL}")
                if not ok2:
                    print(f"{Foreground.RED}Couldn't verify symbol /C/D is type of /func()/d2{Style.RESET_ALL}")
                else:
                    print(f"{Style.BRIGHT}/C/D is type of /func()/d2. great !{Style.RESET_ALL}")
                if not ok3:
                    print(f"{Foreground.RED}Couldn't verify symbol /func()/d3 is of type /func()/S{Style.RESET_ALL}")
                else:
                    print(f"{Style.BRIGHT}/func()/d3 is of type /func()/S. great !{Style.RESET_ALL}")

                print(f"{Style.BRIGHT}{Foreground.YELLOW}WIP: need to verify {mh34_type} is canonicalized to half3x4 ?{Style.RESET_ALL}")

                if not ok5:
                    print(f"{Foreground.RED}Couldn't verify symbol /func()/rwbf is of type ?RWBuffer<?float4x4>{Style.RESET_ALL}")
                else:
                    print(f"{Style.BRIGHT}/func()/rwbf is of type ?RWBuffer<?float4x4>. great !{Style.RESET_ALL}")

                if not ok6:
                    print(f"{Foreground.RED}Couldn't verify symbol /func()/sbInl is of type ?StructuredBuffer< struct /func()/Inl >{Style.RESET_ALL}")
                else:
                    print(f"{Style.BRIGHT}/func()/sbInl is of type ?StructuredBuffer< struct /func()/Inl >. great !{Style.RESET_ALL}")

            ok = ok and ok2 and ok3 and ok4 and ok5 and ok6

        except Exception as e:
            print(f"{Foreground.RED}Err: Parsed --dumpsym dictionary didn't record /func()/d symbol ?{Style.RESET_ALL}", e)
            ok = False
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

    if exec_test(os.path.join(work_dir, "qualified-type-ref-of-var.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
