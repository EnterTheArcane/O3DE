/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// [slang-port] removed: #include <Atom/Features/SrgSemantics.azsli> has no Slang counterpart

#include <Atom/Features/ColorManagement/TransformColor.slangi>

// If true pixels with an alpha value of less than 0.5 are clipped
[AtomOption]
public extern bool o_alphaTest();

// If true both the texture color and diffuse color are converted from linear to sRGB color space
[AtomOption]
public extern bool o_srgbWrite();

// Indicates how to use the second texture indexed by a vertex (if at all)
public enum Modulate
{
    None,
    Alpha,
    AlphaAndColor,
};

[AtomOption(Modulate.None)]
public extern Modulate o_modulate();

// Each vertex can select one or two of the 16 textures bound
// We use an array of textures and a bitmask that indicates whether to use clamp
// sampler (bit set) or wrap. The nth bit has the correct sampler value for the nth texture
[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
struct InstanceSrgLayout
{
    row_major float4x4 m_worldToProj;
    uint m_isClamp;
    Texture2D m_texture[16];

    [AtomStaticSampler("Point", "Point", "Point", "Wrap", "Wrap", "Wrap", 16, "Never", "Filter", "TransparentBlack")]
    SamplerState m_wrapSampler;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 16, "Never", "Filter", "TransparentBlack")]
    SamplerState m_clampSampler;
};
ParameterBlock<InstanceSrgLayout> InstanceSrg;

struct VSInput
{
    float2 m_position : POSITION;
    float4 m_color : COLOR0;
    float2 m_uv : TEXCOORD0;
    uint2  m_flags : BLENDINDICES;
};

struct VSOutput
{
    float4 m_position : SV_Position;
    float4 m_color : COLOR0;
    float2 m_uv : TEXCOORD0;
    nointerpolation uint m_texIndex : COLOR1;
    nointerpolation uint m_texHasColorChannel : COLOR2;
    nointerpolation uint m_texIndex2 : COLOR3;
};

VSOutput MainVS(VSInput IN)
{
    float4x4 worldToProj = InstanceSrg.m_worldToProj;
    float4 posPS = mul(worldToProj, float4(IN.m_position, 1.0f, 1.0f));

    VSOutput OUT;
    OUT.m_position = posPS;
    OUT.m_color = IN.m_color;
    OUT.m_uv = IN.m_uv;
    OUT.m_texIndex = IN.m_flags.x & 0x00FF;
    OUT.m_texHasColorChannel = ((IN.m_flags.x & 0xFF00) > 0) ? 1 : 0;
    OUT.m_texIndex2 = IN.m_flags.y & 0x00FF;
    return OUT;
};

struct PSOutput
{
    float4 m_color : SV_Target0;
};

float4 SampleTriangleTexture(uint texIndex, float2 uv)
{
    if ((InstanceSrg.m_isClamp & (1U << texIndex)) != 0)
    {
        return InstanceSrg.m_texture[NonUniformResourceIndex(texIndex)].Sample(InstanceSrg.m_clampSampler, uv);
    }
    else
    {
        return InstanceSrg.m_texture[NonUniformResourceIndex(texIndex)].Sample(InstanceSrg.m_wrapSampler, uv);
    }
}

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    float4 baseTex = SampleTriangleTexture(IN.m_texIndex, IN.m_uv.xy);
    float4 inDiffuse = IN.m_color;

    // If the texture does not have a color channel then the alpha channel will be in the R channel of the R8 texture
    baseTex = (IN.m_texHasColorChannel) ? baseTex : float4(1.0f, 1.0f, 1.0f, baseTex.x);
    float4 resColor = baseTex * inDiffuse;

    if (o_alphaTest())
    {
        clip(resColor.w - 0.5);
    }

    // Should use srgb anytime after tonemapping
    if (o_srgbWrite())
    {
        resColor.xyz = LinearSrgb_To_Srgb(resColor.xyz);
    }

    // If the o_modulate option is not None it means that the verts have two texture indicies. The second texture is used to
    // mask the first. This is used for gradient masks.
    if (o_modulate() == Modulate::Alpha)
    {
        float4 maskTexAlpha = SampleTriangleTexture(IN.m_texIndex2, IN.m_uv.xy);
        resColor.w *= maskTexAlpha.w;

        if (o_alphaTest())
        {
            // This is a rare case that would only happen if a gradient mask is used inside the mask primitive for a stencil mask
            clip(resColor.w - 0.5);
        }
    }
    else if (o_modulate() == Modulate::AlphaAndColor)
    {
        float4 maskTex = SampleTriangleTexture(IN.m_texIndex2, IN.m_uv.xy);
        resColor *= maskTex.w;

        if (o_alphaTest())
        {
            // This is a rare case that would only happen if a gradient mask is used inside the mask primitive for a stencil mask
            clip(resColor.w - 0.5);
        }
    }

    OUT.m_color = resColor;

    return OUT;
};
