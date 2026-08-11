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


def verify_packing_relaxed(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Storage Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst

            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["id"] == "ExampleSRG",
            lambda: j["ShaderResourceGroups"][0]["bufferForSRGConstants"]["usage"] == "Read",

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3][
                                      "constantByteSize"] == 16,  # Complex type. Size & offset match previous entries' sum
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteSize"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteSize"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 96,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 96,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteSize"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 176,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteOffset"] == 272,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteOffset"] == 288,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteSize"] == 36,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteSize"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteSize"] == 272,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["typeName"] == "/ExampleSRG/T",

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<T>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 268,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<T>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["index"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 268,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["index"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["stride"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["type"] == "Buffer<float2>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["index"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["stride"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["index"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["stride"] == 12,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["index"] == 6,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["index"] == 7,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_direct_x(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-dx12", "--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches 2 (DX) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3][
                                      "constantByteSize"] == 12,  # Complex type. Size & offset match previous entries' sum
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4][
                                      "constantByteSize"] == 40,  # float2x3 - 2 full registers of 16 bytes each and 2 elements of the last register of 4 bytes each
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5][
                                      "constantByteSize"] == 28,  # row float2x3 - 1 full register of 16 bytes and 3 elements of the last register of 4 bytes each
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteSize"] == 36,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 176,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteSize"] == 24,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteSize"] == 28,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 272,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 288,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 320,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteOffset"] == 336,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteOffset"] == 352,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteSize"] == 132,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteOffset"] == 368,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteOffset"] == 384,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteOffset"] == 400,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteOffset"] == 416,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteOffset"] == 368,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteSize"] == 60,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteSize"] == 428,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["typeName"] == "/ExampleSRG/T",

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 428,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 428,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["stride"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["type"] == "Buffer<float2>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["stride"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["stride"] == 12,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_vulkan(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-vulkan", "--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Uniform Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3][
                                      "constantByteSize"] == 16,  # Complex type. Size & offset match previous entries' sum
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteSize"] == 48,  # float2x3 - 3 full registers of 16 bytes each
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteSize"] == 32,  # row float2x3 - 2 full registers of 16 bytes each
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteSize"] == 16,  # each array element is 16-byte aligned
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteSize"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 176,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteSize"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteSize"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16][
                                      "constantByteOffset"] == 224,  # Confirmed: SpirV can't pack 4 bytes after an array
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 272,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 288,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 320,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteOffset"] == 336,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteOffset"] == 352,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteSize"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteOffset"] == 368,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteOffset"] == 384,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteOffset"] == 400,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteOffset"] == 416,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteOffset"] == 368,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteSize"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteSize"] == 432,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["typeName"] == "/ExampleSRG/T",

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 432,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 432,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4][
                                      "stride"] == 16,  # Note that StructuredBuffers in SpirV follow the base alignment for arrays rule (align-16)
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["type"] == "Buffer<float2>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5][
                                      "stride"] == 16,  # Note that Buffers in SpirV follow the base alignment for arrays rule (align-16)
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6][
                                      "stride"] == 16,  # Note that StructuredBuffers in SpirV follow the base alignment for arrays rule (align-16)

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_structs_packing_vulkan(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-vulkan", "--srg"])

    # Packing vectors in nested structs is trickier.
    # The following values match the dxc's SpirV generation

    if ok:
        predicates = [
            # check all references of func()

            # Matches Uniform Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["qualifiedName"] == "/ExampleSRG/Sab/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["qualifiedName"] == "/ExampleSRG/Sab/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["qualifiedName"] == "/ExampleSRG/T/ab",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["typeName"] == "/ExampleSRG/Sab",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteSize"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["qualifiedName"] == "/ExampleSRG/Sba/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["qualifiedName"] == "/ExampleSRG/Sba/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 24,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["qualifiedName"] == "/ExampleSRG/T/ba",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["typeName"] == "/ExampleSRG/Sba",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteSize"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["qualifiedName"] == "/ExampleSRG/Sabc/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["qualifiedName"] == "/ExampleSRG/Sabc/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 36,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["qualifiedName"] == "/ExampleSRG/Sabc/c",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["qualifiedName"] == "/ExampleSRG/T/abc",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["typeName"] == "/ExampleSRG/Sabc",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteSize"] == 32,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["qualifiedName"] == "/ExampleSRG/Sac/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["qualifiedName"] == "/ExampleSRG/Sac/c",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 68,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["qualifiedName"] == "/ExampleSRG/T/ac",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["typeName"] == "/ExampleSRG/Sac",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteSize"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["qualifiedName"] == "/ExampleSRG/S/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["qualifiedName"] == "/ExampleSRG/S/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 96,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["qualifiedName"] == "/ExampleSRG/T/s",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteSize"] == 32,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["qualifiedName"] == "/ExampleSRG/T/a_float",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteSize"] == 4,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["qualifiedName"] == "/ExampleSRG/U/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["qualifiedName"] == "/ExampleSRG/U/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 132,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["qualifiedName"] == "/ExampleSRG/U/c",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 140,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["qualifiedName"] == "/ExampleSRG/U/d",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["qualifiedName"] == "/ExampleSRG/T/u",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["typeName"] == "/ExampleSRG/U",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteSize"] == 32,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["qualifiedName"] == "/ExampleSRG/m_CB",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["typeName"] == "/ExampleSRG/T",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteSize"] == 160,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_open_gl(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-opengl", "--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches std140 (GL) standard here:
            # https://www.khronos.org/registry/OpenGL/specs/gl/glspec45.core.pdf#page=159
            # std140 : https://www.oreilly.com/library/view/opengl-programming-guide/9780132748445/app09lev1sec2.html
            # std430 : https://www.oreilly.com/library/view/opengl-programming-guide/9780132748445/app09lev1sec3.html
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteSize"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteSize"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 116,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 120,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteSize"] == 24,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 160,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteSize"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 212,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 216,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 220,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 228,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 232,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteOffset"] == 236,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25]["constantByteSize"] == 36,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteOffset"] == 244,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteOffset"] == 272,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteOffset"] == 288,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30]["constantByteSize"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["constantByteSize"] == 320,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31]["typeName"] == "/ExampleSRG/T",

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 320,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 320,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["type"] == "Buffer<float2>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["stride"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["stride"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_structs_packing_open_gl(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-opengl", "--srg"])

    # Packing vectors in nested structs is trickier.
    # The following values match the dxc's OpenGL generation

    if ok:
        predicates = [
            # check all references of func()

            # Matches std340 (GL) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["qualifiedName"] == "/ExampleSRG/Sab/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["qualifiedName"] == "/ExampleSRG/Sab/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["qualifiedName"] == "/ExampleSRG/T/ab",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["typeName"] == "/ExampleSRG/Sab",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteSize"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["qualifiedName"] == "/ExampleSRG/Sba/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["qualifiedName"] == "/ExampleSRG/Sba/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 24,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["qualifiedName"] == "/ExampleSRG/T/ba",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["typeName"] == "/ExampleSRG/Sba",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteSize"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["qualifiedName"] == "/ExampleSRG/Sabc/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["qualifiedName"] == "/ExampleSRG/Sabc/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 36,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["qualifiedName"] == "/ExampleSRG/Sabc/c",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["qualifiedName"] == "/ExampleSRG/T/abc",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["typeName"] == "/ExampleSRG/Sabc",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteSize"] == 32,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["qualifiedName"] == "/ExampleSRG/Sac/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["qualifiedName"] == "/ExampleSRG/Sac/c",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 68,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteSize"] == 12,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["qualifiedName"] == "/ExampleSRG/T/ac",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["typeName"] == "/ExampleSRG/Sac",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteSize"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["qualifiedName"] == "/ExampleSRG/S/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["qualifiedName"] == "/ExampleSRG/S/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 96,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteSize"] == 16,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["qualifiedName"] == "/ExampleSRG/T/s",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["typeName"] == "/ExampleSRG/S",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteSize"] == 32,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["qualifiedName"] == "/ExampleSRG/T/a_float",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteSize"] == 4,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["qualifiedName"] == "/ExampleSRG/U/a",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 116,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["qualifiedName"] == "/ExampleSRG/U/b",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 120,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["qualifiedName"] == "/ExampleSRG/U/c",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteSize"] == 4,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["qualifiedName"] == "/ExampleSRG/U/d",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 132,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteSize"] == 8,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["qualifiedName"] == "/ExampleSRG/T/u",  # Complex type
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["typeName"] == "/ExampleSRG/U",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 128,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteSize"] == 28,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["qualifiedName"] == "/ExampleSRG/m_CB",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["typeName"] == "/ExampleSRG/T",
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteSize"] == 160,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_relaxed_use_spaces(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Storage Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst

            # Shader Resource Group 0
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,

            # Shader Resource Group 1
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<Light>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["stride"] == 144,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["type"] == "Buffer<float3>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["stride"] == 12,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["index"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["stride"] == 16,

            # Shader Resource Group 2
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_relaxed_no_spaces(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Storage Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst

            # Shader Resource Group 0
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,

            # Shader Resource Group 1
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<Light>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["stride"] == 144,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["type"] == "Buffer<float3>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["stride"] == 12,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["index"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["stride"] == 16,

            # Shader Resource Group 2
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_relaxed_unique_idx(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg", "--unique-idx"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Storage Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst

            # Note! Bacause AZSLC emits the resource indices in order SRVs/UAVs, then Samplers, then CBVs
            #  the indices (when using unique index) don't necessarily match the order of declaration.
            # Since the data is reflected this should not be a problem!
            # In fact, it verifies the consumer application is data driven and accepts the emitted register indices

            # Shader Resource Group 0
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["index"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["index"] == 3,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,

            # Shader Resource Group 1
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<Light>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["index"] == 4,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["stride"] == 144,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["type"] == "Buffer<float3>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["stride"] == 12,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["index"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["stride"] == 16,

            # Shader Resource Group 2
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_relaxed_unique_idx_use_spaces(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg", "--unique-idx"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Storage Buffer 1 (VK) standard here:
            # https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst

            # Note! Bacause AZSLC emits the resource indices in order SRVs/UAVs, then Samplers, then CBVs
            #  the indices (when using unique index) don't necessarily match the order of declaration.
            # Since the data is reflected this should not be a problem!
            # In fact, it verifies the consumer application is data driven and accepts the emitted register indices

            # Shader Resource Group 0
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["index"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["index"] == 3,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["type"] == "Buffer<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["space"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 16,

            # Shader Resource Group 1
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<Light>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["index"] == 4,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["stride"] == 144,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["type"] == "Buffer<float3>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["stride"] == 12,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["type"] == "StructuredBuffer<S>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["count"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["index"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["space"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["stride"] == 16,

            # Shader Resource Group 2
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["id"] == "m_texCube1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["type"] == "TextureCube",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][0]["stride"] == 16,

            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["id"] == "m_tex2d1",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["type"] == "Texture2D<float3>",
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["count"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["space"] == 2,
            lambda: j["ShaderResourceGroups"][2]["inputsForImageViews"][1]["stride"] == 12,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_unbounded_spill_spaces(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--srg", "--namespace=dx"])

    if ok:
        predicates = [

            # Shader Resource Group 0
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["id"] == "m_texSRVa",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][0]["space"] == 1000,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["id"] == "m_texSRVb",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][1]["space"] == 1001,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][2]["id"] == "m_texSRVc",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][2]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][2]["index"] == 2,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][2]["space"] == 0,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][3]["id"] == "m_texSRVd",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][3]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][3]["index"] == 3,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][3]["space"] == 0,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][4]["id"] == "m_texUAVa",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][4]["type"] == "RWTexture2D<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][4]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][4]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][4]["space"] == 1002,

            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][5]["id"] == "m_texUAVb",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][5]["type"] == "RWTexture2D<float4>",
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][5]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][5]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForImageViews"][5]["space"] == 1003,

            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["id"] == "m_samplera",
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][0]["space"] == 1004,

            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][1]["id"] == "m_samplerb",
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][1]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForSamplers"][1]["space"] == 1005,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["id"] == "m_structArraya",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<MyStruct>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["space"] == 1006,

            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["id"] == "m_structArrayb",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<MyStruct>",
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["count"] == -1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["space"] == 1007,

            # Shader Resource Group 1
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][0]["id"] == "m_texSRVa",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][0]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][0]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][0]["space"] == 1008,

            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][1]["id"] == "m_texSRVb",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][1]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][1]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][1]["space"] == 1009,

            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][2]["id"] == "m_texSRVc",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][2]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][2]["count"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][2]["index"] == 2,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][2]["space"] == 1,

            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][3]["id"] == "m_texSRVd",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][3]["type"] == "Texture2D<float4>",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][3]["count"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][3]["index"] == 3,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][3]["space"] == 1,

            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][4]["id"] == "m_texUAVa",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][4]["type"] == "RWTexture2D<float4>",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][4]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][4]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][4]["space"] == 1010,

            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][5]["id"] == "m_texUAVb",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][5]["type"] == "RWTexture2D<float4>",
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][5]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][5]["index"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForImageViews"][5]["space"] == 1011,

            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["id"] == "m_samplera",
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][0]["space"] == 1012,

            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][1]["id"] == "m_samplerb",
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][1]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForSamplers"][1]["space"] == 1013,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["id"] == "m_structArraya",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["type"] == "ConstantBuffer<MyStruct>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["index"] == 0,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["space"] == 1014,

            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["id"] == "m_structArrayb",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["type"] == "ConstantBuffer<MyStruct>",
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["count"] == -1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["index"] == 1,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["space"] == 1015,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_direct_x_matrices(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-dx12", "--no-alignment-validation", "--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches 2 (DX) emission here:
            # http://shader-playground.timjones.io/206f18e88f838720db8e3415362551df

            # struct T2
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 24,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 72,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 80,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 136,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 144,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 168,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 176,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 204,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 208,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 256,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 280,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 288,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 328,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 336,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 392,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 400,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 424,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 432,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 464,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 480,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteOffset"] == 512,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24][
                                      "constantByteSize"] == 520,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["typeName"] == "/ExampleSRG/T2",

            # struct T3
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0 + 25]["constantByteOffset"] == 0 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1 + 25]["constantByteOffset"] == 28 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2 + 25]["constantByteOffset"] == 32 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3 + 25]["constantByteOffset"] == 76 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4 + 25]["constantByteOffset"] == 80 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5 + 25]["constantByteOffset"] == 140 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6 + 25]["constantByteOffset"] == 144 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7 + 25]["constantByteOffset"] == 184 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8 + 25]["constantByteOffset"] == 192 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9 + 25]["constantByteOffset"] == 236 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10 + 25]["constantByteOffset"] == 240 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11 + 25]["constantByteOffset"] == 288 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12 + 25]["constantByteOffset"] == 304 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13 + 25]["constantByteOffset"] == 336 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14 + 25]["constantByteOffset"] == 352 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15 + 25]["constantByteOffset"] == 400 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16 + 25]["constantByteOffset"] == 416 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17 + 25]["constantByteOffset"] == 480 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18 + 25]["constantByteOffset"] == 496 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19 + 25]["constantByteOffset"] == 536 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20 + 25]["constantByteOffset"] == 544 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21 + 25]["constantByteOffset"] == 592 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22 + 25]["constantByteOffset"] == 608 + 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23 + 25]["constantByteOffset"] == 656 + 528,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][49]["constantByteOffset"] == 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][49][
                                      "constantByteSize"] == 664,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][49]["typeName"] == "/ExampleSRG/T3",

            # struct T4
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0 + 50]["constantByteOffset"] == 0 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1 + 50]["constantByteOffset"] == 32 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2 + 50]["constantByteOffset"] == 48 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3 + 50]["constantByteOffset"] == 96 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4 + 50]["constantByteOffset"] == 112 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5 + 50]["constantByteOffset"] == 176 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6 + 50]["constantByteOffset"] == 192 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7 + 50]["constantByteOffset"] == 248 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8 + 50]["constantByteOffset"] == 256 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9 + 50]["constantByteOffset"] == 316 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10 + 50]["constantByteOffset"] == 320 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11 + 50]["constantByteOffset"] == 384 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12 + 50]["constantByteOffset"] == 400 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13 + 50]["constantByteOffset"] == 432 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14 + 50]["constantByteOffset"] == 448 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15 + 50]["constantByteOffset"] == 496 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16 + 50]["constantByteOffset"] == 512 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17 + 50]["constantByteOffset"] == 576 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18 + 50]["constantByteOffset"] == 592 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19 + 50]["constantByteOffset"] == 648 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20 + 50]["constantByteOffset"] == 656 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21 + 50]["constantByteOffset"] == 720 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22 + 50]["constantByteOffset"] == 736 + 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23 + 50]["constantByteOffset"] == 800 + 1200,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][74]["constantByteOffset"] == 1200,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][74][
                                      "constantByteSize"] == 808,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][74]["typeName"] == "/ExampleSRG/T4",

            # struct TU
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0 + 75]["constantByteOffset"] == 0 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1 + 75]["constantByteOffset"] == 4 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2 + 75]["constantByteOffset"] == 16 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3 + 75]["constantByteOffset"] == 36 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4 + 75]["constantByteOffset"] == 48 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5 + 75]["constantByteOffset"] == 84 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6 + 75]["constantByteOffset"] == 96 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7 + 75]["constantByteOffset"] == 148 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8 + 75]["constantByteOffset"] == 152 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9 + 75]["constantByteOffset"] == 156 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10 + 75]["constantByteOffset"] == 160 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11 + 75]["constantByteOffset"] == 168 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12 + 75]["constantByteOffset"] == 176 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13 + 75]["constantByteOffset"] == 188 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14 + 75]["constantByteOffset"] == 192 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15 + 75]["constantByteOffset"] == 208 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16 + 75]["constantByteOffset"] == 212 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17 + 75]["constantByteOffset"] == 216 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18 + 75]["constantByteOffset"] == 224 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19 + 75]["constantByteOffset"] == 232 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20 + 75]["constantByteOffset"] == 240 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21 + 75]["constantByteOffset"] == 256 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22 + 75]["constantByteOffset"] == 272 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23 + 75]["constantByteOffset"] == 288 + 2016,
            # Special cases packing tests
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24 + 75]["constantByteOffset"] == 296 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25 + 75]["constantByteOffset"] == 304 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26 + 75]["constantByteOffset"] == 312 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27 + 75]["constantByteOffset"] == 320 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28 + 75]["constantByteOffset"] == 332 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29 + 75]["constantByteOffset"] == 336 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30 + 75]["constantByteOffset"] == 344 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31 + 75]["constantByteOffset"] == 352 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][32 + 75]["constantByteOffset"] == 368 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][33 + 75]["constantByteOffset"] == 384 + 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][34 + 75]["constantByteOffset"] == 392 + 2016,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][110]["constantByteOffset"] == 2016,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][110][
                                      "constantByteSize"] == 400,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][110]["typeName"] == "/ExampleSRG/TU",
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_vulkan_matrices(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-vulkan", "--no-alignment-validation", "--srg"])

    if ok:
        predicates = [
            # check all references of func()

            # Matches Uniform Buffer 1 (VK) emission here:
            # http://shader-playground.timjones.io/206f18e88f838720db8e3415362551df

            # struct T2
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1]["constantByteOffset"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2]["constantByteOffset"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3]["constantByteOffset"] == 96,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4]["constantByteOffset"] == 112,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5]["constantByteOffset"] == 176,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6]["constantByteOffset"] == 192,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7]["constantByteOffset"] == 224,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8]["constantByteOffset"] == 240,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9]["constantByteOffset"] == 272,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10]["constantByteOffset"] == 288,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11]["constantByteOffset"] == 320,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12]["constantByteOffset"] == 336,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13]["constantByteOffset"] == 368,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14]["constantByteOffset"] == 384,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15]["constantByteOffset"] == 432,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16]["constantByteOffset"] == 448,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17]["constantByteOffset"] == 512,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18]["constantByteOffset"] == 528,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19]["constantByteOffset"] == 560,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20]["constantByteOffset"] == 576,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21]["constantByteOffset"] == 608,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22]["constantByteOffset"] == 624,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23]["constantByteOffset"] == 656,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["constantByteOffset"] == 0,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24][
                                      "constantByteSize"] == 672,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24]["typeName"] == "/ExampleSRG/T2",

            # struct T3
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0 + 25]["constantByteOffset"] == 0 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1 + 25]["constantByteOffset"] == 32 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2 + 25]["constantByteOffset"] == 48 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3 + 25]["constantByteOffset"] == 96 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4 + 25]["constantByteOffset"] == 112 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5 + 25]["constantByteOffset"] == 176 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6 + 25]["constantByteOffset"] == 192 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7 + 25]["constantByteOffset"] == 240 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8 + 25]["constantByteOffset"] == 256 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9 + 25]["constantByteOffset"] == 304 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10 + 25]["constantByteOffset"] == 320 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11 + 25]["constantByteOffset"] == 368 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12 + 25]["constantByteOffset"] == 384 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13 + 25]["constantByteOffset"] == 416 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14 + 25]["constantByteOffset"] == 432 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15 + 25]["constantByteOffset"] == 480 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16 + 25]["constantByteOffset"] == 496 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17 + 25]["constantByteOffset"] == 560 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18 + 25]["constantByteOffset"] == 576 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19 + 25]["constantByteOffset"] == 624 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20 + 25]["constantByteOffset"] == 640 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21 + 25]["constantByteOffset"] == 688 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22 + 25]["constantByteOffset"] == 704 + 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23 + 25]["constantByteOffset"] == 752 + 672,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][49]["constantByteOffset"] == 672,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][49][
                                      "constantByteSize"] == 768,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][49]["typeName"] == "/ExampleSRG/T3",

            # struct T4
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0 + 50]["constantByteOffset"] == 0 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1 + 50]["constantByteOffset"] == 32 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2 + 50]["constantByteOffset"] == 48 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3 + 50]["constantByteOffset"] == 96 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4 + 50]["constantByteOffset"] == 112 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5 + 50]["constantByteOffset"] == 176 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6 + 50]["constantByteOffset"] == 192 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7 + 50]["constantByteOffset"] == 256 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8 + 50]["constantByteOffset"] == 272 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9 + 50]["constantByteOffset"] == 336 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10 + 50]["constantByteOffset"] == 352 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11 + 50]["constantByteOffset"] == 416 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12 + 50]["constantByteOffset"] == 432 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13 + 50]["constantByteOffset"] == 464 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14 + 50]["constantByteOffset"] == 480 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15 + 50]["constantByteOffset"] == 528 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16 + 50]["constantByteOffset"] == 544 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17 + 50]["constantByteOffset"] == 608 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18 + 50]["constantByteOffset"] == 624 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19 + 50]["constantByteOffset"] == 688 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20 + 50]["constantByteOffset"] == 704 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21 + 50]["constantByteOffset"] == 768 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22 + 50]["constantByteOffset"] == 784 + 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23 + 50]["constantByteOffset"] == 848 + 1440,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][74]["constantByteOffset"] == 1440,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][74][
                                      "constantByteSize"] == 864,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][74]["typeName"] == "/ExampleSRG/T4",

            # struct TU
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][0 + 75]["constantByteOffset"] == 0 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][1 + 75]["constantByteOffset"] == 4 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][2 + 75]["constantByteOffset"] == 8 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][3 + 75]["constantByteOffset"] == 16 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][4 + 75]["constantByteOffset"] == 20 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][5 + 75]["constantByteOffset"] == 32 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][6 + 75]["constantByteOffset"] == 48 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][7 + 75]["constantByteOffset"] == 64 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][8 + 75]["constantByteOffset"] == 68 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][9 + 75]["constantByteOffset"] == 72 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][10 + 75]["constantByteOffset"] == 80 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][11 + 75]["constantByteOffset"] == 88 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][12 + 75]["constantByteOffset"] == 96 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][13 + 75]["constantByteOffset"] == 108 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][14 + 75]["constantByteOffset"] == 112 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][15 + 75]["constantByteOffset"] == 128 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][16 + 75]["constantByteOffset"] == 132 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][17 + 75]["constantByteOffset"] == 136 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][18 + 75]["constantByteOffset"] == 144 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][19 + 75]["constantByteOffset"] == 152 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][20 + 75]["constantByteOffset"] == 160 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][21 + 75]["constantByteOffset"] == 176 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][22 + 75]["constantByteOffset"] == 192 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][23 + 75]["constantByteOffset"] == 208 + 2304,
            # Special cases packing tests
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][24 + 75]["constantByteOffset"] == 216 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][25 + 75]["constantByteOffset"] == 224 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][26 + 75]["constantByteOffset"] == 232 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][27 + 75]["constantByteOffset"] == 240 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][28 + 75]["constantByteOffset"] == 252 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][29 + 75]["constantByteOffset"] == 256 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][30 + 75]["constantByteOffset"] == 264 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][31 + 75]["constantByteOffset"] == 272 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][32 + 75]["constantByteOffset"] == 288 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][33 + 75]["constantByteOffset"] == 304 + 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][34 + 75]["constantByteOffset"] == 312 + 2304,

            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][110]["constantByteOffset"] == 2304,
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][110][
                                      "constantByteSize"] == 320,  # Complex type. Size & offset match previous entries' sum. Confirmed in Dxc
            lambda: j["ShaderResourceGroups"][0]["inputsForSRGConstants"][110]["typeName"] == "/ExampleSRG/TU",
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_direct_x_stride(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-dx12", "--no-alignment-validation", "--srg"])

    if ok:
        predicates = [

            # Column major Constant Buffers
            # Note that the stride here means size. ConstantBuffers should have stride of 0
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][0]["stride"] == 32,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][1]["stride"] == 48,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][2]["stride"] == 36,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][3]["stride"] == 64,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][4]["stride"] == 40,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][5]["stride"] == 52,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][6]["stride"] == 68,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][7]["stride"] == 56,
            lambda: j["ShaderResourceGroups"][0]["inputsForBufferViews"][8]["stride"] == 72,

            # Row major Constant Buffers
            # Note that the stride here means size. ConstantBuffers should have stride of 0
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][0]["stride"] == 32,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][1]["stride"] == 36,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][2]["stride"] == 48,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][3]["stride"] == 40,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][4]["stride"] == 64,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][5]["stride"] == 52,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][6]["stride"] == 56,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][7]["stride"] == 68,
            lambda: j["ShaderResourceGroups"][1]["inputsForBufferViews"][8]["stride"] == 72,

            # Column major Structured Buffers
            # Note that the stride is the size of a single element in the buffer. For DirectX packing layout majorness is ignored
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][0]["stride"] == 16 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][1]["stride"] == 24 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][2]["stride"] == 24 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][3]["stride"] == 32 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][4]["stride"] == 32 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][5]["stride"] == 36 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][6]["stride"] == 48 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][7]["stride"] == 48 + 4,
            lambda: j["ShaderResourceGroups"][2]["inputsForBufferViews"][8]["stride"] == 64 + 4,

            # Row major Structured Buffers
            # Note that the stride is the size of a single element in the buffer. For DirectX packing layout majorness is ignored
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][0]["stride"] == 16 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][1]["stride"] == 24 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][2]["stride"] == 24 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][3]["stride"] == 32 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][4]["stride"] == 32 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][5]["stride"] == 36 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][6]["stride"] == 48 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][7]["stride"] == 48 + 4,
            lambda: j["ShaderResourceGroups"][3]["inputsForBufferViews"][8]["stride"] == 64 + 4,
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}input assembler layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_direct_x_inline_constants(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--pack-dx12", "--srg", "--root-const=52"])

    if ok:
        predicates = [

            # Inline constant buffer reflection data validation
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["count"] == 1,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["index"] == 0,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["space"] == 1,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["usage"] == "Read",
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["sizeInBytes"] == 60,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["id"] == "Root_Constants",

            # Inline constant structure members validation
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["constantByteOffset"] == 0,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["constantByteSize"] == 16,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["constantId"] == "varFloat4",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["qualifiedName"] == "/Root_Constants/varFloat4",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["typeDimensions"] == [],
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["typeKind"] == "Predefined",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["typeName"] == "?float4",

            # Inline constant structure members validation
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["constantByteOffset"] == 16,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["constantByteSize"] == 44,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["constantId"] == "mat3x3",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["qualifiedName"] == "/Root_Constants/mat3x3",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["typeDimensions"] == [],
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["typeKind"] == "Predefined",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["typeName"] == "?float3x3",
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}inline constant layouts verification...{Style.RESET_ALL}")
        ok = common.verify_all_predicates(predicates, j)
    return True if ok else False


def verify_packing_metal_inline_constants(file, compiler_path, silent):
    j, ok = common.build_and_get_json(file, compiler_path, silent, ["--namespace=mt", "--srg"])

    if ok:
        predicates = [

            # Inline constant buffer reflection data validation
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["count"] == 1,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["index"] == 0,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["space"] == 1,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["usage"] == "Read",
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["sizeInBytes"] == 64,
            lambda: j["RootConstantBuffer"]["bufferForRootConstants"]["id"] == "Root_Constants",

            # Inline constant structure members validation
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["constantByteOffset"] == 0,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["constantByteSize"] == 16,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["constantId"] == "varFloat4",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["qualifiedName"] == "/Root_Constants/varFloat4",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["typeDimensions"] == [],
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["typeKind"] == "Predefined",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][0]["typeName"] == "?float4",

            # Inline constant structure members validation
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["constantByteOffset"] == 16,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["constantByteSize"] == 44,
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["constantId"] == "mat3x3",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["qualifiedName"] == "/Root_Constants/mat3x3",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["typeDimensions"] == [],
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["typeKind"] == "Predefined",
            lambda: j["RootConstantBuffer"]["inputsForRootConstants"][1]["typeName"] == "?float3x3",
        ]

        if not silent: print(f"{Foreground.CYAN}{Style.BRIGHT}inline constant layouts verification...{Style.RESET_ALL}")
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

    # Relaxed packing needs to be reviewed
    # if verifyPackingRelaxed(os.path.join(work_dir, "SrgLayouts.azsl"), compiler, silent) : result += 1
    # else: result_failed += 1

    if verify_packing_direct_x(os.path.join(work_dir, "SrgLayouts.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_vulkan(os.path.join(work_dir, "SrgLayouts.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_structs_packing_vulkan(os.path.join(work_dir, "SrgLayoutsStructs.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_open_gl(os.path.join(work_dir, "SrgLayouts.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_structs_packing_open_gl(os.path.join(work_dir, "SrgLayoutsStructs.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_relaxed_use_spaces(os.path.join(work_dir, "SrgLayoutsSpaces.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_relaxed_no_spaces(os.path.join(work_dir, "SrgLayoutsSpaces.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_relaxed_unique_idx(os.path.join(work_dir, "SrgLayoutsSpaces.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_relaxed_unique_idx_use_spaces(os.path.join(work_dir, "SrgLayoutsSpaces.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_unbounded_spill_spaces(os.path.join(work_dir, "SrgLayoutsMultipleUnboundedArrays.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_direct_x_matrices(os.path.join(work_dir, "SrgLayoutsMatrices.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_vulkan_matrices(os.path.join(work_dir, "SrgLayoutsMatrices.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_direct_x_stride(os.path.join(work_dir, "SrgLayoutsStrides.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_direct_x_inline_constants(os.path.join(work_dir, "InlineConstantLayouts.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1

    if verify_packing_metal_inline_constants(os.path.join(work_dir, "InlineConstantLayouts.azsl"), compiler, silent):
        result += 1
    else:
        result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
