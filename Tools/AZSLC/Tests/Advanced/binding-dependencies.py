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


def verify_binding_dependencies_1(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--bindingdep"])

    if ok:
        predicates = [
            lambda: "MainPS" in j["Srg1"]["Srg1_SRGConstantBuffer"]["dependentFunctions"],
            lambda: "MainVS" in j["Srg1"]["Srg1_SRGConstantBuffer"]["dependentFunctions"],
            lambda: "m_meshDisplacement" in j["Srg1"]["Srg1_SRGConstantBuffer"]["participantConstants"],
            lambda: "MainPS" in j["Srg1"]["m_environmentMap"]["dependentFunctions"],
            lambda: "MainPS" in j["Srg1"]["m_extendedMaterials"]["dependentFunctions"],
            lambda: "MainVS" in j["Srg1"]["m_extendedMaterials"]["dependentFunctions"],
            lambda: "MainPS" in j["Srg1"]["m_materialConstants"]["dependentFunctions"],
            lambda: "MainPS" in j["Srg1"]["m_sampler1"]["dependentFunctions"], lambda: len(j["Srg1"]["m_sampler2"]["dependentFunctions"]) == 0,
            lambda: "MainVS" in j["Srg2"]["Srg2_SRGConstantBuffer"]["dependentFunctions"],
            lambda: "m_inverseTranspose" in j["Srg2"]["Srg2_SRGConstantBuffer"]["participantConstants"],
            lambda: "m_world" in j["Srg2"]["Srg2_SRGConstantBuffer"]["participantConstants"],
            lambda: "MainPS" in j["Srg2"]["m_IBLsampler"]["dependentFunctions"], lambda: "MainPS" in j["Srg2"]["m_diffuseIBL"]["dependentFunctions"],
            lambda: "over" not in j["Srg1"]["m_materialConstants"]["dependentFunctions"],
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}binding dependency analysis verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_binding_dependencies_2(file, compiler_path, silent):
    symbols, ok = common.build_and_get_json(file, compiler_path, silent, ["--bindingdep"])

    if ok:
        predicates = [
            # this test is mostly to verify that the analysis doesn't crash altogether.
            # this is a regression test, since we had a build where that shader crashed the --bindingdep build.
            lambda: "StandardPbr_ForwardPassPS" in symbols["MaterialSrg"]["MaterialSrg_SRGConstantBuffer"]["dependentFunctions"],
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}complex input program binding dep verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} verified.{Style.RESET_ALL}")

    return 1 if ok else 0


def verify_binding_dependencies_3(file, compiler_path, silent):
    symbols, ok = common.build_and_get_json(file, compiler_path, silent, ["--bindingdep"])

    if ok:
        predicates = [
            # this is a regression test to make sure we can analyze fully the dependencies when we are in the
            # presence of variant options. because options have a fallback in one of the SRG.
            # and tracking the use of options to the fallback is its own code path
            # every increase of the degree of cyclomatic complexity mandates its own test.
            lambda: "MainPS" in symbols["TrianglePerInstanceSRG"]["TrianglePerInstanceSRG_SRGConstantBuffer"]["dependentFunctions"],
            lambda: "MainVS" in symbols["TrianglePerInstanceSRG"]["TrianglePerInstanceSRG_SRGConstantBuffer"]["dependentFunctions"],
            lambda: "m_objectMatrix" in symbols["TrianglePerInstanceSRG"]["TrianglePerInstanceSRG_SRGConstantBuffer"]["participantConstants"],
            lambda: "m_SHADER_VARIANT_KEY_NAME_" in symbols["TrianglePerInstanceSRG"]["TrianglePerInstanceSRG_SRGConstantBuffer"]["participantConstants"],
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}Program with variant fallback binding dep verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} verified.{Style.RESET_ALL}")

    return 1 if ok else 0


def verify_binding_dependencies_4(file, compiler_path, silent):
    symbols, ok = common.build_and_get_json(file, compiler_path, silent, ["--bindingdep"])

    if ok:
        predicates = [
            # this is a regression test to make sure the compiler doesn't crash on variables
            # internal to functions but declared after an unnamed scope.
            lambda: "MainCS" in symbols["PassSrg"]["PassSrg_SRGConstantBuffer"]["dependentFunctions"],
            lambda: "MainCS" in symbols["PassSrg"]["m_lutTexture"]["dependentFunctions"],
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}Program with unnamed scopes binding dep verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, symbols)
        if ok and not silent:
            print(f"{Style.BRIGHT}OK! {len(predicates)} verified.{Style.RESET_ALL}")

    return 1 if ok else 0


result = 0  # to define for subtests
result_failed = 0  # to define for subtests


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    if verify_binding_dependencies_1(os.path.join(work_dir, "entry-dependencies.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if result and verify_binding_dependencies_2(os.path.join(work_dir, "../Semantic/standardpbr_forwardpass.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if result and verify_binding_dependencies_3(os.path.join(work_dir, "../Semantic/Triangle.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if result and verify_binding_dependencies_4(os.path.join(work_dir, "BakeAcesOutputTransformLutCS.azslin"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
