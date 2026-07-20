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
import re
import sys
from pathlib import Path

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


def scoped_element_types(out_slangi_path, elements_include, namespace):
    """Struct names declared inside `namespace <namespace> { ... }` in the element-structs file.

    AZSL scopes structs declared inside a ShaderResourceGroup to that SRG. When those become
    top-level Slang structs they can collide with a same-named top-level type (e.g. the ViewSrg
    buffer-element ProjectedShadow vs. the shadow-calculator class ProjectedShadow). The element file
    keeps them in `namespace <Srg>Data`; the SRG members that reference them are then qualified so the
    two names stay distinct -- reproducing AZSL's scope. Adding a struct to that namespace is all a
    maintainer does; the qualification here is automatic.
    """
    if not namespace:
        return set()
    elements_path = Path(out_slangi_path).parent / Path(elements_include).name
    try:
        text = elements_path.read_text(encoding="utf-8")
    except OSError:
        return set()

    match = re.search(rf"namespace\s+{re.escape(namespace)}\s*\{{", text)
    if not match:
        return set()
    depth = 0
    start = match.end() - 1
    cursor = start
    while cursor < len(text):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                break
        cursor += 1
    body = text[start + 1 : cursor]
    return set(re.findall(r"\bstruct\s+(\w+)", body))


def make_qualifier(scoped, namespace):
    """A function that prefixes any scoped type name in a type string with `<namespace>::`."""
    if not scoped or not namespace:
        return lambda type_text: type_text
    patterns = [
        (re.compile(rf"(?<![\w:]){re.escape(name)}(?![\w])"), f"{namespace}::{name}")
        for name in scoped
    ]

    def qualify(type_text):
        for pattern, replacement in patterns:
            type_text = pattern.sub(replacement, type_text)
        return type_text

    return qualify


def emit_slangi(model, elements_include, path, functions_include=None, scoped_namespace=None):
    srg = model["srgName"]
    slot = model["frequencyId"]
    lines = [LICENSE_HEADER.rstrip("\n"), ""]
    lines.append(f"// GENERATED by Tools/GenerateSharedSrgSlang.py from the AZSL editorial source -- do not")
    lines.append(f"// hand-edit the pinned bindings. This is the Slang binding-ABI declaration of {srg}; every")
    lines.append(f"// register/space matches what AZSLc assigns so Slang- and AZSL-compiled shaders share the")
    lines.append(f"// one {srg} instance. Verified byte-for-byte by SharedSrgBindingAbiTests.")
    lines.append("")
    lines.append("#pragma once")
    # Import the element-structs module (Slang discourages #include). The functions companion stays
    # a #include because it is injected into the accessor struct body (a fragment, not a module).
    elements_module = elements_include.rsplit(".", 1)[0].strip("/").replace("/", ".")
    lines.append(f"__exported import {elements_module};")
    lines.append("")

    scoped = scoped_element_types(path, elements_include, scoped_namespace)
    qualify = make_qualifier(scoped, scoped_namespace)

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
            lines.append(f"    {qualify(constant['type'])} {constant['name']};")
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
            f'{qualify(resource["type"])} {resource["name"]}{array} : '
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

    emit_accessor(model, lines, functions_include, qualify)

    with open(path, "w", newline="\n") as handle:
        handle.write("\n".join(lines))


def emit_accessor(model, lines, functions_include, qualify=lambda type_text: type_text):
    """Append the access-sugar layer over the pinned globals.

    The pinned globals above are the binding ABI and must stay byte-identical (SharedSrgBindingAbiTests
    hashes them), so the ergonomics that let ported shaders write `ViewSrg.m_x` / `ViewSrg.GetFarZ()`
    are layered on top rather than baked in: a stateless `static` accessor whose properties forward to
    the pinned globals. Because it is `static` and holds no resources of its own, it contributes no
    binding -- reflection still sees only the pinned globals, so the ABI is unchanged.

    Resource/sampler properties reach their same-named file-scope global through the leading `::`
    global-scope qualifier (a same-name reference without it would recurse into the property). Constant
    properties forward into the implicit SRG constant buffer. Free functions the SRG defined in AZSL
    are hand-maintained in the companion file and textually included into the accessor body.
    """
    srg = model["srgName"]
    accessor = f"{srg}Accessor"
    constant_buffer = f"{srg}_SRGConstantBuffer"

    lines.append("")
    lines.append(f"// Access sugar: lets shaders read this shared SRG as `{srg}.m_x` / `{srg}.Fn()`, matching")
    lines.append(f"// AZSL's `{srg}::m_x` / `{srg}::Fn()`. Every property forwards to a pinned global above; the")
    lines.append(f"// accessor is stateless and `static`, so it adds no binding and leaves the ABI untouched.")
    lines.append(f"struct {accessor}")
    lines.append("{")

    for constant in model["constants"]:
        name = constant["name"]
        lines.append(
            f"    property {qualify(constant['type'])} {name} {{ get {{ return {constant_buffer}.{name}; }} }}"
        )
    for resource in model["resources"]:
        name = resource["name"]
        array = f"[{resource['count']}]" if resource["count"] > 1 else ""
        lines.append(
            f"    property {qualify(resource['type'])} {name}{array} {{ get {{ return ::{name}; }} }}"
        )
    for sampler in model["staticSamplers"]:
        name = sampler["name"]
        lines.append(f"    property SamplerState {name} {{ get {{ return ::{name}; }} }}")

    if functions_include:
        lines.append("")
        lines.append(f"    // Free functions the SRG defined in AZSL (hand-maintained companion).")
        lines.append(f'    #include "{functions_include}"')

    lines.append("};")
    lines.append(f"static {accessor} {srg};")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    # Full generate: reflect the assembled AZSL SRG with AZSLc (per API), emit manifest + .slangi.
    parser.add_argument("--dx", help="AZSLc --srg --namespace=dx json")
    parser.add_argument("--vk", help="AZSLc --srg --namespace=vk --unique-idx json")
    # Regenerate only the .slangi from the checked-in manifest (no AZSLc needed) -- use this when only
    # the emitted Slang shape changes (e.g. the accessor layer), so the ABI model itself is unchanged.
    parser.add_argument("--from-manifest", help="regenerate the .slangi from a checked-in .abi.json manifest")
    parser.add_argument("--srg-name", help="required with --dx/--vk (taken from the manifest otherwise)")
    parser.add_argument("--elements-include", required=True, help="ShaderLib-relative path to the hand-maintained element structs")
    parser.add_argument("--functions-include", help="ShaderLib-relative path to the hand-maintained SRG free functions, if any")
    parser.add_argument("--scoped-namespace", help="namespace in the elements file holding SRG-inline structs to qualify (e.g. ViewSrgData)")
    parser.add_argument("--out-slangi", required=True)
    parser.add_argument("--out-manifest", help="required with --dx/--vk")
    arguments = parser.parse_args()

    if arguments.from_manifest:
        model = json.load(open(arguments.from_manifest))
        emit_slangi(model, arguments.elements_include, arguments.out_slangi, arguments.functions_include, arguments.scoped_namespace)
    else:
        if not (arguments.dx and arguments.vk and arguments.srg_name and arguments.out_manifest):
            parser.error("--dx, --vk, --srg-name and --out-manifest are required unless --from-manifest is given")
        dx_doc = json.load(open(arguments.dx))
        vk_doc = json.load(open(arguments.vk))
        model = build_model(dx_doc, vk_doc, arguments.srg_name)
        emit_manifest(model, arguments.out_manifest)
        emit_slangi(model, arguments.elements_include, arguments.out_slangi, arguments.functions_include, arguments.scoped_namespace)

    print(f"{model['srgName']}: {len(model['resources'])} resources, {len(model['constants'])} constants, "
          f"{len(model['staticSamplers'])} static samplers -> {arguments.out_slangi}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
