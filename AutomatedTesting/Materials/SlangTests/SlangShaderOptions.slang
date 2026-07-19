/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Shader options in the Atom authoring form: an interface of [AtomOption]-attributed static
// function requirements declares each option once — its name, type, typed default and range —
// and the [AtomOptions] extern struct is implemented by the builder per lowering mode (baked
// constants, specialization constants, or dynamic reads of the ShaderVariantKey fallback) and
// composed at link time. Use-sites are identical in every mode. The [AtomVariantFallback] member
// designates where the runtime writes the key for dynamic option switching.

[AtomShaderResourceGroup(4 /*PerPass*/)]
public struct OptionsShaderResourceGroupLayout
{
    Vector4F m_tintColor;

    [AtomVariantFallback]
    public Vector4U m_shaderVariantKeyFallback;
};
public ParameterBlock<OptionsShaderResourceGroupLayout> OptionsSrg;

public enum DebugModeT
{
    None,
    Uv,
    Solid,
}

public interface IOptions
{
    [AtomOption(true)]
    static bool o_useTint();

    [AtomOption(DebugModeT.None)]
    static DebugModeT o_debugMode();

    [AtomOption(4)] [AtomOptionRange(1, 8)]
    static i32 o_bandCount();
}

[AtomOptions]
public extern struct Options : IOptions;

// Supervariants deliver their ".shader" Definitions as preprocessor macros: the TintBoost
// supervariant amplifies the tint without touching the option layout (which must stay
// identical across every supervariant of a shader).
#ifndef SLANGTEST_TINT_STRENGTH
#define SLANGTEST_TINT_STRENGTH 1
#endif

struct VertexOutput
{
    Vector4F m_position : SV_Position;
    Vector2F m_uv : UV0;
};

VertexOutput MainVS(u32 vertexIndex : SV_VertexID)
{
    // Fullscreen triangle from the vertex index alone; no vertex buffer
    const Vector2F uv = Vector2F(f32((vertexIndex << 1) & 2), f32(vertexIndex & 2));
    VertexOutput output;
    output.m_position = Vector4F(uv * Vector2F(2, -2) + Vector2F(-1, 1), 0, 1);
    output.m_uv = uv;
    return output;
}

struct PixelOutput
{
    Vector4F m_color : SV_Target0;
};

PixelOutput MainPS(VertexOutput input)
{
    Vector4F color = Vector4F(input.m_uv, 0, 1);
    switch (Options.o_debugMode())
    {
    case DebugModeT.Uv:
        color = Vector4F(input.m_uv, 0, 1);
        break;
    case DebugModeT.Solid:
        color = Vector4F(1, 0, 1, 1);
        break;
    default:
        color.z = frac(input.m_uv.x * f32(Options.o_bandCount()));
        break;
    }
    if (Options.o_useTint())
    {
        color *= OptionsSrg.m_tintColor * f32(SLANGTEST_TINT_STRENGTH);
    }

    PixelOutput output;
    output.m_color = color;
    return output;
}
