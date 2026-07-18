/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Shader options in the ported-shader authoring form: ATOM_OPTION declares each option once —
// use-sites read it as a bare identifier — and ATOM_VARIANT_FALLBACK designates the
// ShaderResourceGroup member carrying the ShaderVariantKey for runtime option switching. The
// builder discovers the options through reflection and links the per-mode accessor
// implementations; nothing here changes between the dynamic, specialized and baked lowerings.

[AtomShaderResourceGroup(4 /*PerPass*/)]
struct OptionsShaderResourceGroupLayout
{
    Vector4F m_tintColor;
    Vector4U m_shaderVariantKeyFallback;
};
ParameterBlock<OptionsShaderResourceGroupLayout> OptionsSrg;

ATOM_VARIANT_FALLBACK(OptionsSrg, m_shaderVariantKeyFallback)

public enum DebugModeT
{
    None,
    Uv,
    Solid,
}

ATOM_OPTION(bool, o_useTint, true)
ATOM_OPTION(DebugModeT, o_debugMode, DebugModeT.None)
ATOM_OPTION_RANGE(i32, o_bandCount, 4, 1, 8)

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
    switch (o_debugMode)
    {
    case DebugModeT.Uv:
        color = Vector4F(input.m_uv, 0, 1);
        break;
    case DebugModeT.Solid:
        color = Vector4F(1, 0, 1, 1);
        break;
    default:
        color.z = frac(input.m_uv.x * f32(o_bandCount));
        break;
    }
    if (o_useTint)
    {
        color *= OptionsSrg.m_tintColor;
    }

    PixelOutput output;
    output.m_color = color;
    return output;
}
