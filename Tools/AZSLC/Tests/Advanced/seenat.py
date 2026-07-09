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


def test_variables(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [
            # check that there is global variable `a`
            lambda: symbols["Symbol '/a'"]['kind'] == 'Variable',
            lambda: symbols["Symbol '/a'"]['type']['core']['name'] == '?int',
            # and its only reference is at line 41
            lambda: len(symbols["Symbol '/a'"]['references']) == 1,
            lambda: symbols["Symbol '/a'"]['references'][0]['line'] == 42,  # .. a + b;

            # now check global var `b`
            lambda: symbols["Symbol '/b'"]['kind'] == 'Variable',
            lambda: symbols["Symbol '/b'"]['type']['core']['name'] == '?int',
            lambda: set(symbols["Symbol '/b'"]['storage'].split()) == set(['static', 'const']),
            # it appears in 2 places:
            lambda: len(symbols["Symbol '/b'"]['references']) == 2,
            lambda: symbols["Symbol '/b'"]['references'][0]['line'] == 10,  # c = b;
            lambda: symbols["Symbol '/b'"]['references'][1]['line'] == 42,  # .. a + b;

            # check member variable `c`
            lambda: len(symbols["Symbol '/S/c'"]['references']) == 5,
            lambda: symbols["Symbol '/S/c'"]['references'][0]['line'] == 10,  # c = b;
            lambda: symbols["Symbol '/S/c'"]['references'][1]['line'] == 11,  # return c;
            lambda: symbols["Symbol '/S/c'"]['references'][2]['line'] == 30,  # param.c;
            lambda: symbols["Symbol '/S/c'"]['references'][3]['line'] == 31,  # param.c (first appearance)
            lambda: symbols["Symbol '/S/c'"]['references'][4]['line'] == 31,  # param.c; (last appearance)

            # check deep nested `d`
            lambda: len(symbols["Symbol '/S/N/NN/NNN/d'"]['references']) == 2,
            lambda: symbols["Symbol '/S/N/NN/NNN/d'"]['references'][0]['line'] == 44,  # the 's' in `if (s.n.nn.nnn.d)`
            lambda: symbols["Symbol '/S/N/NN/NNN/d'"]['references'][1]['line'] == 50,  # on the right of assignment

            # check refs to global `s` of UDT
            lambda: symbols["Symbol '/s'"]['references'][0]['line'] == 36,  # ref to ::s as passed to func(s)
            lambda: symbols["Symbol '/s'"]['references'][1]['line'] == 44,  # ref to ::s in the s.n.nn... expression
            lambda: symbols["Symbol '/s'"]['references'][2]['line'] == 48,  # ref to ::s in assignment RHS

            # check ref to local var `c`
            lambda: symbols["Symbol '/main()/c'"]['references'][0]['line'] == 41,  # first assignment (LHS)

            # check refs to s1: (local var of type sampler)()
            lambda: symbols["Symbol '/main()/s1'"]['references'][0]['line'] == 53,
            lambda: symbols["Symbol '/main()/s1'"]['references'][0]['col'] == 33,

            # check refs to s2: (local var of type sampler)()
            lambda: symbols["Symbol '/main()/s2'"]['references'][0]['line'] == 57,
            lambda: symbols["Symbol '/main()/s2'"]['references'][0]['col'] == 42,

            # check refs to tex in the if scope
            lambda: symbols["Symbol '/main()/$bk1/tex'"]['references'][0]['line'] == 53,
            lambda: symbols["Symbol '/main()/$bk1/tex'"]['references'][0]['col'] == 21,

            # check refs to tex after the if scope
            lambda: symbols["Symbol '/main()/tex'"]['references'][0]['line'] == 57,
            lambda: symbols["Symbol '/main()/tex'"]['references'][0]['col'] == 31,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}variable references verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} variable references predicates verified.{Style.RESET_ALL}")

    return ok


def test_functions(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [
            # check all references of func()
            lambda: symbols["Symbol '/func()'"]['kind'] == 'Function',
            lambda: symbols["Symbol '/func()'"]['references'][0]['line'] == 1,  # first declaration
            lambda: symbols["Symbol '/func()'"]['references'][1]['line'] == 5,  # call in psmain
            lambda: symbols["Symbol '/func()'"]['references'][2]['line'] == 14,  # re-declaration
            lambda: symbols["Symbol '/func()'"]['references'][3]['line'] == 25,  # in typeof expression
            lambda: symbols["Symbol '/func()'"]['references'][4]['line'] == 26,  # in switch condition expression
            lambda: symbols["Symbol '/func()'"]['references'][5]['line'] == 29,  # call in assignment
            lambda: symbols["Symbol '/func()'"]['references'][6]['line'] == 35,  # ref 7 in constructor
        ]

        # note that "nope" cases didn't appear as false positive references, otherwise they would have
        # disrupted the index.

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}functions references verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} function references predicates verified.{Style.RESET_ALL}")
    return ok


def test_methods(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [
            # let's verify that there is a free function with name f at global scope
            lambda: symbols["Symbol '/f(?int)'"]['kind'] == 'Function',  # this is a symbol meant to lead the tool to possible confusion
            lambda: symbols["Symbol '/f(?int)'"]['line'] == 1,
            lambda: symbols["Symbol '/_(?int)'"]['kind'] == 'Function',  # just there test function call expressions with minimal visual pollution
            lambda: symbols["Symbol '/_(?int)'"]['line'] == 2,
            # setup ok, let's see the references to the Parent/f family
            lambda: symbols["Symbol '/Parent/f(?int)'"]['kind'] == 'Function',
            lambda: len(symbols["Symbol '/Parent/f(?int)'"]['references']) <= 1,  # one ref to itself max. since you can't call it directly.
            lambda: symbols["Symbol '/Parent/f(?int)'"]["has overriding children"][0]["name"] == "/Child/f(?int)",  # one child does override
            # let's check the child also back-refers to this parent:
            lambda: symbols["Symbol '/Child/f(?int)'"]["is hiding base symbol"] == "/Parent/f(?int)",
        ]
        # let's checkout the references of this child:
        appearance_lines = [16, 21, 23, 24, 33, 34, 37]  # as per comments in the file ref, ref2, ref3...
        for ii, line in enumerate(appearance_lines):
            predicates.append(lambda ii=ii, line=line: symbols["Symbol '/Child/f(?int)'"]['references'][ii]['line'] == line)

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}methods references verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} methods references predicates verified.{Style.RESET_ALL}")
    return ok


def test_structs(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = []
        appearance_lines = [7, 12, 13, 15, 20, 29, 36, 40]  # as per comments in the file ref, ref2, ref3...
        for ii, line in enumerate(appearance_lines):
            predicates.append(lambda ii=ii, line=line: symbols["Symbol '/S'"]['references'][ii]['line'] == line)

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}structs references verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} structs references predicates verified.{Style.RESET_ALL}")

    return ok


def test_srgs(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = []
        # let's check MySRG
        appearances = [(24, 5), (29, 5), (29, 24), (30, 12), (31, 14), (33, 15), (35, 17), (39, 48), (41, 19), (41, 41)]  # line:col
        for ii, line_col in enumerate(appearances):
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRG'"]['references'][ii]['line'] == line_col[0])
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRG'"]['references'][ii]['col'] == line_col[1])

        # let's check Inner
        appearances = [(17, 22), (24, 12), (29, 12), (30, 19), (31, 23)]
        for ii, line_col in enumerate(appearances):
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRG/Inner'"]['references'][ii]['line'] == line_col[0])
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRG/Inner'"]['references'][ii]['col'] == line_col[1])

        # let's check m_mat
        appearances = [(30, 26), (30, 43), (41, 57)]
        for ii, line_col in enumerate(appearances):
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRG/Inner/m_mat'"]['references'][ii]['line'] == line_col[0])
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRG/Inner/m_mat'"]['references'][ii]['col'] == line_col[1])

        # check one appearance of Deep
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/Deep'"]['references'][0]['line'] == 32)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/Deep'"]['references'][0]['col'] == 21)

        # check m_h (as a leaf)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/Deep/m_h'"]['references'][0]['line'] == 32)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/Deep/m_h'"]['references'][0]['col'] == 29)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/Deep/m_h'"]['references'][1]['line'] == 32)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/Deep/m_h'"]['references'][1]['col'] == 55)

        # check m_deep (as an instance declared from a type grammar rule)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/m_deep'"]['references'][0]['line'] == 32)
        predicates.append(lambda: symbols["Symbol '/MySRG/Inner/m_deep'"]['references'][0]['col'] == 48)

        # check m_stb as a special type of data (structured buffer)
        predicates.append(lambda: symbols["Symbol '/MySRG/m_stb'"]['references'][0]['line'] == 29)
        predicates.append(lambda: symbols["Symbol '/MySRG/m_stb'"]['references'][0]['col'] == 31)

        predicates.append(lambda: symbols["Symbol '/MySRG/m_stb'"]['references'][1]['line'] == 41)
        predicates.append(lambda: symbols["Symbol '/MySRG/m_stb'"]['references'][1]['col'] == 48)

        # check ref to SData as a local variable type
        predicates.append(lambda: symbols["Symbol '/SData'"]['references'][0]['line'] == 36)
        predicates.append(lambda: symbols["Symbol '/SData'"]['references'][0]['col'] == 5)

        # check sampler ref line 34
        predicates.append(lambda: symbols["Symbol '/MySRG/m_sampler'"]['references'][0]['line'] == 35)

        # ref to d, clr, s
        predicates.append(lambda: symbols["Symbol '/main()/d'"]['references'][0]['line'] == 37)
        predicates.append(lambda: symbols["Symbol '/SData/clr'"]['references'][0]['line'] == 37)
        predicates.append(lambda: symbols["Symbol '/main()/s'"]['references'][0]['line'] == 37)

        # ref to f at the end
        predicates.append(lambda: symbols["Symbol '/main()/f'"]['references'][0]['line'] == 41)
        predicates.append(lambda: symbols["Symbol '/main()/f'"]['references'][0]['col'] == 31)

        # worldmatrix
        predicates.append(lambda: symbols["Symbol '/MySRG/m_worldMatrix'"]['references'][0]['line'] == 39)
        predicates.append(lambda: symbols["Symbol '/MySRG/m_worldMatrix'"]['references'][0]['col'] == 55)

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}SRG references verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} SRG references predicates verified.{Style.RESET_ALL}")
    return ok


def test_cbs(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:

        predicates = []
        # let's check that m_diffuseColor appears in the good places
        appearances = [(17, 34), (26, 48), (43, 49), (45, 38), (51, 18)]  # line:col
        for ii, line_col in enumerate(appearances):
            predicates.append(
                lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRGOne/InnerStruct/m_diffuseColor'"]['references'][ii]['line'] == line_col[0])
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRGOne/InnerStruct/m_diffuseColor'"]['references'][ii]['col'] == line_col[1])

        # let's check the real materialConstants references
        appearances = [(17, 16), (26, 30), (42, 29), (43, 31), (44, 49)]  # line:col
        for ii, line_col in enumerate(appearances):
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRGOne/materialConstants'"]['references'][ii]['line'] == line_col[0])
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/MySRGOne/materialConstants'"]['references'][ii]['col'] == line_col[1])

        # let's check the global decoy main/materialConstants references
        appearances = [(41, 5), (45, 20), (48, 17)]  # line:col
        for ii, line_col in enumerate(appearances):
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/materialConstants'"]['references'][ii]['line'] == line_col[0])
            predicates.append(lambda ii=ii, line_col=line_col: symbols["Symbol '/materialConstants'"]['references'][ii]['col'] == line_col[1])

        # let's check the local decoy main/materialConstants references
        appearances = [(47, 19), (49, 24)]  # line:col
        for ii, line_col in enumerate(appearances):
            predicates.append(
                lambda ii=ii, line_col=line_col: symbols["Symbol '/main(?float2)/MySRGOne/materialConstants'"]['references'][ii]['line'] == line_col[0])
            predicates.append(
                lambda ii=ii, line_col=line_col: symbols["Symbol '/main(?float2)/MySRGOne/materialConstants'"]['references'][ii]['col'] == line_col[1])

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}CB references verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} CB references verified.{Style.RESET_ALL}")

    return ok


# test Member Access Expression
def test_mae(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [
            lambda: symbols["Symbol '/A'"]['references'][0]['line'] == 8,
            lambda: symbols["Symbol '/A'"]['references'][0]['col'] == 5,
            lambda: symbols["Symbol '/A'"]['references'][1]['line'] == 9,
            lambda: symbols["Symbol '/A'"]['references'][1]['col'] == 7,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}qualified-id verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} qualified-id verified.{Style.RESET_ALL}")

    return ok


# test Deported Method definition test file
def test_deported(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [
            # check *
            lambda: symbols["Symbol '/Random/Next()'"]['line'] == 20,
            lambda: symbols["Symbol '/Random/Next()'"]['def line'] == 34,
            lambda: symbols["Symbol '/Random/Next()'"]['references'][0]['line'] == 20,
            lambda: symbols["Symbol '/Random/Next()'"]['references'][1]['line'] == 29,
            # check **
            lambda: symbols["Symbol '/Random/cur'"]['line'] == 23,
            lambda: symbols["Symbol '/Random/cur'"]['references'][0]['line'] == 28,
            lambda: symbols["Symbol '/Random/cur'"]['references'][1]['line'] == 38,
            # check ***
            lambda: symbols["Symbol '/Random/Init()'"]['line'] == 26,
            lambda: symbols["Symbol '/Random/Init()'"]['def line'] == 26,
            lambda: symbols["Symbol '/Random/Init()'"]['references'][0]['line'] == 37,
            lambda: symbols["Symbol '/Random/Init()'"]['references'][1]['line'] == 45,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT} deported-methods verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} deported-methods verified.{Style.RESET_ALL}")

    return ok


# test typealias is registered and doesn't break the reference chain of other user defined types
def test_typealias(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [
            # check for the understanding of the access of the stuff variable through indirect structuredbuffer access
            lambda: symbols["Symbol '/PassVars/stuff'"]['references'][0]['line'] == 15,
            # check for registration of the alias itself
            lambda: symbols["Symbol '/StructBuf'"]['kind'] == 'TypeAlias',
            lambda: symbols["Symbol '/StructBuf'"]['canonical type']['generic']['name'] == '/PassVars',
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT} alias predicates verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} alias predicates verified.{Style.RESET_ALL}")

    return ok


def test_function_overloads(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [

            lambda: symbols["Symbol '/MySRG/Metal'"]['kind'] == "Struct",

            lambda: symbols["Symbol '/MySRG/Barrel'"]['kind'] == "Struct",
            lambda: symbols["Symbol '/MySRG/Barrel'"]['kind'] == "Struct",

            lambda: symbols["Symbol '/MySRG/MakeMat(?float)'"]['kind'] == "Function",
            lambda: symbols["Symbol '/MySRG/MakeMat(?float)'"]['references'][0]['line'] == 53,
            lambda: symbols["Symbol '/MySRG/MakeMat(?float)'"]['references'][1]['line'] == 58,

            lambda: symbols["Symbol '/MySRG/MakeMat'"]['kind'] == 'OverloadSet',
            lambda: symbols["Symbol '/MySRG/MakeMat'"]['functions'][0] == '/MySRG/MakeMat(/MySRG/Metal)',
            lambda: symbols["Symbol '/MySRG/MakeMat'"]['functions'][1] == '/MySRG/MakeMat(?float)',
            lambda: not symbols["Symbol '/MySRG/MakeMat'"]['references'],

            lambda: symbols["Symbol '/MySRG/MakeMat(/MySRG/Metal)'"]['references'][0]['line'] == 55,

            # because of use of * multiply, azslc can't resolve the overload, so the set holds the reference
            lambda: symbols["Symbol '/MySRG/Luminosity'"]['references'][0]['line'] == 63,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT} overload predicates verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} overload predicates verified.{Style.RESET_ALL}")

    return ok


def test_unnamed_blocks(file, compiler_path, silent):
    """return number of successes"""
    symbols, ok = common.build_and_get_symbols(file, compiler_path, silent)
    if ok:
        predicates = [

            lambda: symbols["Symbol '/S'"]['kind'] == "Class",

            lambda: symbols["Symbol '/S/i'"]['kind'] == "Variable",
            lambda: symbols["Symbol '/S/i'"]['references'][0]['line'] == 21,
            lambda: symbols["Symbol '/S/i'"]['references'][1]['line'] == 25,
            lambda: symbols["Symbol '/S/i'"]['references'][2]['line'] == 69,
            lambda: symbols["Symbol '/S/i'"]['references'][3]['line'] == 72,

            lambda: symbols["Symbol '/i'"]['kind'] == "Variable",
            lambda: symbols["Symbol '/i'"]['references'][0]['line'] == 16,
            lambda: symbols["Symbol '/i'"]['references'][1]['line'] == 18,
            lambda: symbols["Symbol '/i'"]['references'][2]['line'] == 29,
            lambda: symbols["Symbol '/i'"]['references'][3]['line'] == 70,

            lambda: symbols["Symbol '/f()/$bk0/i'"]['type']['core']['name'] == '?int',
            lambda: symbols["Symbol '/f()/$bk0/i'"]['line'] == 14,
            lambda: symbols["Symbol '/f()/$bk0/i'"]['references'][0]['line'] == 15,

            lambda: symbols["Symbol '/f()/i'"]['type']['core']['name'] == '?int',
            lambda: symbols["Symbol '/f()/i'"]['line'] == 23,
            lambda: symbols["Symbol '/f()/i'"]['references'][0]['line'] == 28,
            lambda: symbols["Symbol '/f()/i'"]['references'][1]['line'] == 58,

            lambda: symbols["Symbol '/f()/$sw1/$bk2/i'"]['line'] == 33,
            lambda: symbols["Symbol '/f()/$sw1/$bk2/i'"]['references'][0]['line'] == 34,

            lambda: symbols["Symbol '/f()/$sw1/i'"]['line'] == 37,
            lambda: symbols["Symbol '/f()/$sw1/i'"]['references'][0]['line'] == 40,

            lambda: symbols["Symbol '/f()/$for3/i'"]['line'] == 43,
            lambda: symbols["Symbol '/f()/$for3/i'"]['references'][0]['line'] == 43,
            lambda: symbols["Symbol '/f()/$for3/i'"]['references'][1]['line'] == 43,
            lambda: symbols["Symbol '/f()/$for3/i'"]['references'][2]['line'] == 45,
            lambda: symbols["Symbol '/f()/$for3/i'"]['references'][3]['line'] == 45,

            lambda: symbols["Symbol '/f()/$bk4/i'"]['line'] == 50,
            lambda: symbols["Symbol '/f()/$bk4/i'"]['references'][0]['line'] == 51,
            lambda: symbols["Symbol '/f()/$bk4/i'"]['references'][1]['line'] == 51,

            lambda: symbols["Symbol '/f()/$bk5/i'"]['line'] == 56,
            lambda: symbols["Symbol '/f()/$bk5/i'"]['references'][0]['line'] == 57,
            lambda: symbols["Symbol '/f()/$bk5/i'"]['references'][1]['line'] == 57,

            lambda: symbols["Symbol '/f()/$for6/i'"]['line'] == 60,
            lambda: symbols["Symbol '/f()/$for6/i'"]['references'][0]['line'] == 60,
            lambda: symbols["Symbol '/f()/$for6/i'"]['references'][1]['line'] == 60,

            lambda: symbols["Symbol '/f()/$for7/i'"]['line'] == 63,
            lambda: symbols["Symbol '/f()/$for7/i'"]['references'][0]['line'] == 63,
            lambda: symbols["Symbol '/f()/$for7/i'"]['references'][1]['line'] == 64,

            lambda: symbols["Symbol '/S/f()/j'"]['line'] == 75,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT} shadowed symbols predicates verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols, silent)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} shadowed symbols predicates verified.{Style.RESET_ALL}")

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

    if not silent: print("testing for variables...")
    if test_variables(os.path.join(work_dir, "seenat-variables.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for functions...")
    if test_functions(os.path.join(work_dir, "seenat-functions.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for methods...")
    if test_methods(os.path.join(work_dir, "seenat-methods.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for structs...")
    if test_structs(os.path.join(work_dir, "seenat-structs.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for SRGs...")
    if test_srgs(os.path.join(work_dir, "seenat-srgs.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for constant buffers...")
    if test_cbs(os.path.join(work_dir, "seenat-cb.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for qualification in member access expression RHS...")
    if test_mae(os.path.join(work_dir, "seenat-MAE-qualifiedRHS.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for deported methods...")
    if test_deported(os.path.join(work_dir, "seenat-deported-methods.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for type alias...")
    if test_typealias(os.path.join(work_dir, "seenat-typedef.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for function overloading...")
    if test_function_overloads(os.path.join(work_dir, "seenat-function-overloads.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if not silent: print("testing for unnamed blocks...")
    if test_unnamed_blocks(os.path.join(work_dir, "seenat-unnamed-blocks.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
