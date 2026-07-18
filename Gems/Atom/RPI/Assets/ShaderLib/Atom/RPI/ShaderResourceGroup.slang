/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// The Atom ShaderResourceGroup authoring vocabulary for Slang.
//
// A ShaderResourceGroup is authored as a struct tagged [AtomShaderResourceGroup(bindingSlot)]
// wrapped in a ParameterBlock whose instance name is the runtime SRG name:
//
//     import Atom.RPI.ShaderResourceGroup;
//
//     [AtomShaderResourceGroup(2 /*PerMaterial binding slot*/)]
//     struct MaterialShaderResourceGroup
//     {
//         Texture2D<Vector4F> m_diffuse;
//
//         [AtomStaticSampler(16 /*maxAnisotropy*/, "Wrap" /*addressMode*/, "Linear" /*filterMode*/, "Never" /*comparisonFunc*/, "TransparentBlack" /*borderColor*/)]
//         SamplerState m_sampler;
//
//         Vector4F m_color; // loose members land in the implicit SRG-constants constant buffer
//     };
//     ParameterBlock<MaterialShaderResourceGroup> MaterialSrg;
//
// The shader builder reads these attributes back through Slang reflection to recover binding
// slots, static-sampler descriptors and the variant-key fallback designation that AZSL expressed
// with dedicated syntax. Slang user attributes take positional arguments only.

//! Declares a struct as an Atom ShaderResourceGroup with the given binding slot (frequency).
[__AttributeUsage(_AttributeTargets.Struct)]
public struct AtomShaderResourceGroupAttribute
{
    public int bindingSlot;
};

//! Groups a global-scope declaration into a logical ShaderResourceGroup. Used where the
//! ParameterBlock form cannot express the layout — notably the Bindless group, whose members are
//! global unsized arrays (Slang allows at most one unsized member inside a struct).
[__AttributeUsage(_AttributeTargets.Var)]
public struct AtomShaderResourceGroupMemberAttribute
{
    public string shaderResourceGroupName;
    public int bindingSlot;
};

//! Declares a SamplerState member as an immutable static sampler with the given state.
//! String arguments name RHI enum values: addressMode = Wrap|Mirror|Clamp|Border|MirrorOnce,
//! filterMode = Point|Linear, comparisonFunc = Never|Less|Equal|LessEqual|Greater|NotEqual|GreaterEqual|Always,
//! borderColor = TransparentBlack|OpaqueBlack|OpaqueWhite.
[__AttributeUsage(_AttributeTargets.Var)]
public struct AtomStaticSamplerAttribute
{
    public int maxAnisotropy;
    public string addressMode;
    public string filterMode;
    public string comparisonFunc;
    public string borderColor;
};

//! Applied by the ATOM_VARIANT_FALLBACK macro to the exported fallback-key getter it generates:
//! names the ShaderResourceGroup (the ParameterBlock instance) and the uint4 member inside it
//! that carry the 128-bit ShaderVariantKey fallback. The getter's body reads that member, so the
//! member's existence and type are enforced by the compiler rather than by builder validation.
[__AttributeUsage(_AttributeTargets.Function)]
public struct AtomVariantFallbackAttribute
{
    public string shaderResourceGroupName;
    public string memberName;
};

//! Applied by the ATOM_OPTION / ATOM_OPTION_RANGE macros to the generated accessor declaration
//! of one shader option. Carries the authored option type and default value as source text; the
//! shader builder discovers options by walking module declarations for functions with this
//! attribute (accessor name = AtomOptionImpl_<option name>).
[__AttributeUsage(_AttributeTargets.Function)]
public struct AtomOptionAttribute
{
    public string typeName;
    public string defaultValue;
};

//! Applied by the ATOM_OPTION_RANGE macro: the inclusive value range of an integer shader option.
[__AttributeUsage(_AttributeTargets.Function)]
public struct AtomOptionRangeAttribute
{
    public int minValue;
    public int maxValue;
};

//! Declares a struct as the root-constants block of the shader.
[__AttributeUsage(_AttributeTargets.Struct)]
public struct AtomRootConstantsAttribute
{
    public int reserved;
};
