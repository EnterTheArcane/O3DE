/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// [slang-port] removed: #include <Atom/Features/SrgSemantics.azsli> has no Slang counterpart

#include <Atom/Features/PostProcessing/FullscreenPixelInfo.slangi>
#include <Atom/Features/PostProcessing/FullscreenVertex.slangi>
#include <Atom/Features/PostProcessing/PostProcessUtil.slangi>
#include <Atom/Features/PostProcessing/Shapers.slangi>

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2D<float4> m_colorTexture;
    Texture3D<float4> m_lut;
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
    
    int m_shaperType;
    float m_shaperBias;
    float m_shaperScale;
};
ParameterBlock<PassSrgLayout> PassSrg;

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    float2 uvCoord = float2(IN.m_texCoord.x, IN.m_texCoord.y);
    float3 color = PassSrg.m_colorTexture.Sample(PassSrg.LinearSampler, uvCoord).rgb;

    ShaperType shaperType = (ShaperType)PassSrg.m_shaperType;

    float3 finalColor = ApplyShaperLookupTable(shaperType, color, PassSrg.m_shaperBias, PassSrg.m_shaperScale, PassSrg.m_lut, PassSrg.LinearSampler);
    OUT.m_color.rgb = finalColor;
    OUT.m_color.a = 1.0;

    return OUT;
}
