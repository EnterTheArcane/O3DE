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


def check_symbol(table, sym, field, value):
    if not table[sym]['type']['core'][field] == value:
        print(f"{Foreground.RED}{sym} must be of type {value}{Style.RESET_ALL}")
        return False
    return True


def exec_test(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if not ok:
        print(f"{Foreground.RED}Couldn't get meaningful symbols table{Style.RESET_ALL}")
        return 0

    if not symbols["Symbol '/Monday'"]['type']['core']['name'] == '/Weekday':
        ok = False
        print(f"{Foreground.RED}Symbol /Monday must be of type '/Weekday'{Style.RESET_ALL}")
    if not symbols["Symbol '/Monday'"]['type']['core']['underlying_scalar'] == '<NA>':
        ok = False
        print(f"{Foreground.RED}Symbol /Monday must have underlying_scalar of type '<NA>'{Style.RESET_ALL}")

    ok = ok and check_symbol(symbols, "Symbol '/Monday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Monday'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Tuesday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Tuesday'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Wednesday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Wednesday'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Thursday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Thursday'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Friday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Friday'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Saturday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Saturday'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Sunday'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/Sunday'", 'underlying_scalar', '<NA>')

    ok = ok and check_symbol(symbols, "Symbol '/FavouriteDay'", 'name', '/Weekday')
    ok = ok and check_symbol(symbols, "Symbol '/FavouriteDay'", 'underlying_scalar', '<NA>')

    ok = ok and check_symbol(symbols, "Symbol '/Season/Spring'", 'name', '/Season')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Spring'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Summer'", 'name', '/Season')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Summer'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Autumn'", 'name', '/Season')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Autumn'", 'underlying_scalar', '<NA>')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Winter'", 'name', '/Season')
    ok = ok and check_symbol(symbols, "Symbol '/Season/Winter'", 'underlying_scalar', '<NA>')

    ok = ok and check_symbol(symbols, "Symbol '/IsComing'", 'name', '/Season')
    ok = ok and check_symbol(symbols, "Symbol '/IsComing'", 'underlying_scalar', '<NA>')

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

    if exec_test(os.path.join(work_dir, "../Samples/Enumeration.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
