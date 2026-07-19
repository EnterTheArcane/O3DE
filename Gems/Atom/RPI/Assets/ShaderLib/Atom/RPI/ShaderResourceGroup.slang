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
//         [AtomStaticSampler("Linear", "Linear", "Linear", "Wrap", "Wrap", "Wrap", 16 /*maxAnisotropy*/, "Never", "Filter", "TransparentBlack")]
//         SamplerState m_sampler;
//
//         Vector4F m_color; // loose members land in the implicit SRG-constants constant buffer
//     };
//     ParameterBlock<MaterialShaderResourceGroup> MaterialSrg;
//
// The shader builder reads these attributes back through Slang reflection to recover binding
// slots, static-sampler descriptors and the variant-key fallback designation that AZSL expressed
// with dedicated syntax. Slang user attributes take positional arguments only.

//! The binding slot (frequency) of a ShaderResourceGroup, mirroring the native RPI::SrgBindingSlot
//! namespace (Atom/RPI.Reflect/Base.h) one-for-one: same names, same values, and an open set of
//! constants rather than a closed enum. The values also match the AZSL SRG_* frequencies in
//! Atom/Features/SrgSemantics.azsli, so [AtomShaderResourceGroup(SrgBindingSlot.Pass)] reads like the
//! C++ SrgBindingSlot::Pass instead of a bare slot number.
namespace SrgBindingSlot
{
    public static const int Draw = 0;
    public static const int Object = 1;
    public static const int Material = 2;
    public static const int SubPass = 3;
    public static const int Pass = 4;
    public static const int View = 5;
    public static const int Scene = 6;
    public static const int Bindless = 7;
}

//! Declares a struct as an Atom ShaderResourceGroup with the given binding slot (frequency). The
//! argument is an SrgBindingSlot; it coerces onto the integer slot the builder reads back.
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

//! Declares a SamplerState member as an immutable static sampler with the given state. String
//! arguments name RHI enum values: filter{Min,Mag,Mip} = Point|Linear; address{U,V,W} =
//! Wrap|Mirror|Clamp|Border|MirrorOnce; comparisonFunc =
//! Never|Less|Equal|LessEqual|Greater|NotEqual|GreaterEqual|Always; reductionType =
//! Filter|Comparison|Minimum|Maximum; borderColor = TransparentBlack|OpaqueBlack|OpaqueWhite.
//! maxAnisotropy > 1 enables anisotropic filtering. The full state is spelled out (not the shorter
//! AZSL sampler block) because a shared-SRG static sampler contributes to the layout hash and must
//! match AZSLc's SamplerState field-for-field. mip LOD min/max/bias stay at RHI defaults (0, 15, 0).
[__AttributeUsage(_AttributeTargets.Var)]
public struct AtomStaticSamplerAttribute
{
    public string filterMin;
    public string filterMag;
    public string filterMip;
    public string addressU;
    public string addressV;
    public string addressW;
    public int maxAnisotropy;
    public string comparisonFunc;
    public string reductionType;
    public string borderColor;
};

// SHADER OPTIONS
//
// Options are authored as an interface of static function requirements plus an extern struct the
// builder implements per lowering mode (baked constants, specialization constants, or dynamic
// reads of the ShaderVariantKey fallback) and composes at link time — use-sites are identical in
// every mode:
//
//     public interface IOptions
//     {
//         [AtomOption(true)]
//         static bool o_useTint();
//
//         [AtomOption(QualityT.Medium)]        // typed default: enum literals coerce to int
//         static QualityT o_quality();
//
//         [AtomOption(4)] [AtomOptionRange(1, 8)]
//         static i32 o_sampleCount();
//     }
//     [AtomOptions]
//     public extern struct Options : IOptions;
//
//     ... if (Options.o_useTint()) { ... }
//
// Dynamic lowering additionally needs one uint4 ShaderResourceGroup member designated as the
// ShaderVariantKey fallback, in a public ParameterBlock:
//
//     [AtomVariantFallback]
//     public Vector4U m_shaderVariantKeyFallback;

//! Marks the extern struct whose link-time implementation the builder generates: the anchor the
//! option discovery starts from. The struct must conform to exactly one interface of
//! [AtomOption]-attributed static function requirements.
[__AttributeUsage(_AttributeTargets.Struct)]
public struct AtomOptionsAttribute {};

//! Declares one interface requirement as a shader option and carries its default. The argument
//! is typed and compile-checked: bool and enum literals coerce onto the int field (an enum
//! default is its case index).
[__AttributeUsage(_AttributeTargets.Function)]
public struct AtomOptionAttribute
{
    public int defaultValue;
};

//! The inclusive value range of an integer shader option.
[__AttributeUsage(_AttributeTargets.Function)]
public struct AtomOptionRangeAttribute
{
    public int min;
    public int max;
};

//! Overrides the runtime name of a shader option — the name the ShaderOptionGroupLayout,
//! .materialtype option references and shader variant lists use. Ports keep their legacy AZSL
//! option names without carrying the prefix into the Slang API:
//!
//!     [AtomOption(false)] [AtomOptionAlias("o_shadeAll")]
//!     static bool shadeAll();     // runtime name o_shadeAll, use-sites Options.shadeAll()
//!
//! Without an alias, the runtime name is the method name.
[__AttributeUsage(_AttributeTargets.Function)]
public struct AtomOptionAliasAttribute
{
    public string runtimeName;
};

//! Designates a uint4 ShaderResourceGroup member as the 128-bit ShaderVariantKey fallback the
//! dynamic option lowering reads. The member's ParameterBlock (and its element struct) must be
//! public so the generated implementation can read it across the module boundary.
[__AttributeUsage(_AttributeTargets.Var)]
public struct AtomVariantFallbackAttribute {};

//! Declares a struct as the root-constants block of the shader.
[__AttributeUsage(_AttributeTargets.Struct)]
public struct AtomRootConstantsAttribute
{
    public int reserved;
};
