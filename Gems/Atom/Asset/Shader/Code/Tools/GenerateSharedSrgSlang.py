#!/usr/bin/env python3
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Shared-SRG binding-ABI generator (SlangIntegrationPlan.md, Phase 4 / D3).

SceneSrg/ViewSrg/Bindless are a binding ABI: one SRG instance is bound to pipelines compiled from
many shaders, so every consumer must expect identical bindings. AZSL remains the editorial source
during migration; this maintainer-run tool reflects the *assembled* AZSL SRG with AZSLc (once per
API) and emits, from that reflection:

  * a language-neutral binding-ABI manifest (JSON, per API) recording every input's type, count,
    element stride, and register/space -- the human-reviewable, checked-in record of the ABI, and
  * the pinned Slang declaration (.slangi) that reproduces those exact bindings on both PC targets
    (register(...) for DXIL, [[vk::binding(...)]] for SPIR-V), grouped via [AtomShaderResourceGroupMember].

The element structs of StructuredBuffer<T>/ConstantBuffer<T> members are hand-maintained field-accurate
ports (AZSLc SRG reflection carries element *stride* but not element *fields*); the generated .slangi
#includes that companion file. The CI parity gtest (SharedSrgBindingAbiTests) then compiles the
generated .slangi and asserts its ShaderResourceGroupLayout hash equals AZSLc's -- so any drift in the
AZSL editorial source fails the build until this tool is re-run.

Usage (from a dev prompt where AZSLc and the C-preprocessor are available):
  GenerateSharedSrgSlang.py --dx <srg.dx.json> --vk <srg.vk.json> \
      --srg-name ViewSrg --elements-include Atom/Features/Srg/ViewSrgElements.slangi \
      --out-slangi <ViewSrg.slangi> --out-manifest <ViewSrg.abi.json>

The .dx.json/.vk.json are AZSLc `--srg` outputs (--namespace=dx and --namespace=vk --unique-idx) of a
driver that #includes the assembled srgi. See SharedSrgBindingAbiTests.cpp for the preprocess+reflect
recipe (identical include roots).
"""

import argparse
import json
import sys

SCHEMA_VERSION = 1
LICENSE_HEADER = """/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
"""


def register_class(type_text, is_sampler=False):
    """The HLSL register class an input pins to (matches AZSLc's per-class DX numbering)."""
    if is_sampler:
        return "s"
    if type_text.startswith("ConstantBuffer"):
        return "b"
    if type_text.startswith(("RW", "Append", "Consume", "RasterizerOrdered")):
        return "u"
    return "t"


def find_srg(document, srg_name):
    for srg in document["ShaderResourceGroups"]:
        if srg["id"] == srg_name:
            return srg
    raise SystemExit(f"error: SRG '{srg_name}' not found (found: {[s['id'] for s in document['ShaderResourceGroups']]})")


def index_by_id(entries):
    return {entry["id"]: entry for entry in entries}


def strip_type(type_name):
    """AZSLc prefixes predefined scalar/vector/matrix constant types with '?'."""
    return type_name[1:] if type_name.startswith("?") else type_name


def map_address_mode(azslc_value):
    return {
        "TEXTURE_ADDRESS_WRAP": "Wrap",
        "TEXTURE_ADDRESS_MIRROR": "Mirror",
        "TEXTURE_ADDRESS_CLAMP": "Clamp",
        "TEXTURE_ADDRESS_BORDER": "Border",
        "TEXTURE_ADDRESS_MIRRORONCE": "MirrorOnce",
        "TEXTURE_ADDRESS_MIRROR_ONCE": "MirrorOnce",
    }.get(azslc_value, "Wrap")


def map_comparison_func(azslc_value):
    return {
        "COMPARISON_NEVER": "Never",
        "COMPARISON_LESS": "Less",
        "COMPARISON_EQUAL": "Equal",
        "COMPARISON_LESS_EQUAL": "LessEqual",
        "COMPARISON_GREATER": "Greater",
        "COMPARISON_NOT_EQUAL": "NotEqual",
        "COMPARISON_GREATER_EQUAL": "GreaterEqual",
        "COMPARISON_ALWAYS": "Always",
    }.get(azslc_value, "Never")


def map_border_color(azslc_value):
    return {
        "STATIC_BORDER_COLOR_TRANSPARENT_BLACK": "TransparentBlack",
        "STATIC_BORDER_COLOR_OPAQUE_BLACK": "OpaqueBlack",
        "STATIC_BORDER_COLOR_OPAQUE_WHITE": "OpaqueWhite",
    }.get(azslc_value, "TransparentBlack")


def build_model(dx_doc, vk_doc, srg_name):
    """Merge the two per-API AZSLc reflections into one ABI model."""
    dx = find_srg(dx_doc, srg_name)
    vk = find_srg(vk_doc, srg_name)

    model = {
        "schemaVersion": SCHEMA_VERSION,
        "srgName": srg_name,
        "frequencyId": dx["bindingSlot"],
        "resources": [],
        "staticSamplers": [],
        "constants": [],
    }

    # The implicit SRG-constants constant buffer.
    dx_cb = dx["bufferForSRGConstants"]
    vk_cb = vk["bufferForSRGConstants"]
    model["srgConstantsBuffer"] = {
        "dx12": {"class": "b", "register": dx_cb["index"], "space": dx_cb["space"]},
        "vulkan": {"binding": vk_cb["index"], "set": vk_cb["space"]},
    }

    # Resource inputs: buffers, images, samplers. Match dx<->vk by id; dx gives the per-class register,
    # vk gives the unique binding.
    def add_resources(dx_list, vk_list):
        vk_index = index_by_id(vk_list)
        for entry in dx_list:
            name = entry["id"]
            type_text = entry.get("type", "")
            record = {
                "name": name,
                "type": type_text,
                "count": entry.get("count", 1),
                "stride": entry.get("stride", 0),
                "dx12": {"class": register_class(type_text), "register": entry["index"], "space": entry["space"]},
                "vulkan": {"binding": vk_index[name]["index"], "set": vk_index[name]["space"]},
            }
            model["resources"].append(record)

    add_resources(dx["inputsForBufferViews"], vk["inputsForBufferViews"])
    add_resources(dx["inputsForImageViews"], vk["inputsForImageViews"])

    # Static samplers: the full SamplerState contributes to the layout hash, so capture every field.
    vk_samplers = index_by_id(vk.get("inputsForSamplers", []))
    for entry in dx.get("inputsForSamplers", []):
        if entry.get("isDynamic", False):
            continue  # a dynamic sampler is a bindable resource, not part of the ABI's static set
        name = entry["id"]
        vk_entry = vk_samplers[name]
        model["staticSamplers"].append({
            "name": name,
            "filterMin": entry["filterMin"],
            "filterMag": entry["filterMag"],
            "filterMip": entry["filterMip"],
            "addressU": map_address_mode(entry["addressU"]),
            "addressV": map_address_mode(entry["addressV"]),
            "addressW": map_address_mode(entry["addressW"]),
            "maxAnisotropy": entry["anisotropyMax"],
            "comparisonFunc": map_comparison_func(entry["comparisonFunc"]),
            "reductionType": entry["reductionType"],
            "borderColor": map_border_color(entry["borderColor"]),
            "dx12": {"class": "s", "register": entry["index"], "space": entry["space"]},
            "vulkan": {"binding": vk_entry["index"], "set": vk_entry["space"]},
        })

    # Stable, diff-friendly ordering: by register class then register index.
    model["resources"].sort(key=lambda r: (r["dx12"]["class"], r["dx12"]["register"]))
    model["staticSamplers"].sort(key=lambda r: r["dx12"]["register"])

    # SRG constants: top-level members only (skip the flattened struct sub-members that carry a '.'),
    # ordered by byte offset, which is the declaration order.
    for constant in dx["inputsForSRGConstants"]:
        if "." in constant["constantId"]:
            continue
        if constant.get("typeKind") == "Struct":
            type_text = constant["typeName"].rsplit("/", 1)[-1]
        else:
            type_text = strip_type(constant["typeName"])
        model["constants"].append({
            "name": constant["constantId"],
            "type": type_text,
            "offset": constant["constantByteOffset"],
            "size": constant["constantByteSize"],
        })
    model["constants"].sort(key=lambda c: c["offset"])

    return model


def emit_manifest(model, path):
    with open(path, "w", newline="\n") as handle:
        json.dump(model, handle, indent=4)
        handle.write("\n")


def emit_slangi(model, elements_include, path):
    srg = model["srgName"]
    slot = model["frequencyId"]
    lines = [LICENSE_HEADER.rstrip("\n"), ""]
    lines.append(f"// GENERATED by Tools/GenerateSharedSrgSlang.py from the AZSL editorial source -- do not")
    lines.append(f"// hand-edit the pinned bindings. This is the Slang binding-ABI declaration of {srg}; every")
    lines.append(f"// register/space matches what AZSLc assigns so Slang- and AZSL-compiled shaders share the")
    lines.append(f"// one {srg} instance. Verified byte-for-byte by SharedSrgBindingAbiTests.")
    lines.append("")
    lines.append("#pragma once")
    lines.append(f'#include "{elements_include}"')
    lines.append("")

    def member(attr_lines, decl):
        lines.append(f'[AtomShaderResourceGroupMember("{srg}", {slot})]')
        lines.extend(attr_lines)
        lines.append(decl)
        lines.append("")

    # Every member of a shared SRG is pinned to the SRG's frequency as its DX register space and
    # Vulkan descriptor set (PerView=5, PerScene=6, ...), so a shader consuming several shared SRGs
    # plus its private SRGs never collides. AZSLc reflects registers SRG-locally (space 0) and the
    # ShaderResourceGroupLayout hash excludes the space, so this matches AZSLc's layout hash while
    # binding to the correct set/space at runtime.
    space = slot

    # SRG constants: the struct, then the implicit constant buffer.
    if model["constants"]:
        lines.append(f"struct {srg}_Constants")
        lines.append("{")
        for constant in model["constants"]:
            lines.append(f"    {constant['type']} {constant['name']};")
        lines.append("};")
        lines.append("")
        cb = model["srgConstantsBuffer"]
        member(
            [f'[[vk::binding({cb["vulkan"]["binding"]}, {space})]]'],
            f'ConstantBuffer<{srg}_Constants> {srg}_SRGConstantBuffer : '
            f'register(b{cb["dx12"]["register"]}, space{space});',
        )

    for resource in model["resources"]:
        dx = resource["dx12"]
        vk = resource["vulkan"]
        array = f"[{resource['count']}]" if resource["count"] > 1 else ""
        member(
            [f'[[vk::binding({vk["binding"]}, {space})]]'],
            f'{resource["type"]} {resource["name"]}{array} : '
            f'register({dx["class"]}{dx["register"]}, space{space});',
        )

    for sampler in model["staticSamplers"]:
        dx = sampler["dx12"]
        vk = sampler["vulkan"]
        static_sampler = (
            '[AtomStaticSampler("{filterMin}", "{filterMag}", "{filterMip}", '
            '"{addressU}", "{addressV}", "{addressW}", {maxAnisotropy}, '
            '"{comparisonFunc}", "{reductionType}", "{borderColor}")]'
        ).format(**sampler)
        member(
            [static_sampler, f'[[vk::binding({vk["binding"]}, {space})]]'],
            f'SamplerState {sampler["name"]} : register(s{dx["register"]}, space{space});',
        )

    with open(path, "w", newline="\n") as handle:
        handle.write("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dx", required=True, help="AZSLc --srg --namespace=dx json")
    parser.add_argument("--vk", required=True, help="AZSLc --srg --namespace=vk --unique-idx json")
    parser.add_argument("--srg-name", required=True)
    parser.add_argument("--elements-include", required=True, help="ShaderLib-relative path to the hand-maintained element structs")
    parser.add_argument("--out-slangi", required=True)
    parser.add_argument("--out-manifest", required=True)
    arguments = parser.parse_args()

    dx_doc = json.load(open(arguments.dx))
    vk_doc = json.load(open(arguments.vk))
    model = build_model(dx_doc, vk_doc, arguments.srg_name)

    emit_manifest(model, arguments.out_manifest)
    emit_slangi(model, arguments.elements_include, arguments.out_slangi)
    print(f"{arguments.srg_name}: {len(model['resources'])} resources, {len(model['constants'])} constants, "
          f"{len(model['staticSamplers'])} static samplers -> {arguments.out_slangi}, {arguments.out_manifest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
