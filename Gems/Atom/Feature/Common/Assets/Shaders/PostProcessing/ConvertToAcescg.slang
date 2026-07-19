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
#include <Atom/Features/PostProcessing/Aces.slangi>

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2D<float4> m_framebuffer;
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
};
ParameterBlock<PassSrgLayout> PassSrg;

PSOutput MainPS(VSOutput IN)
{
    PSOutput Out;

    float3 color = PassSrg.m_framebuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord).rgb;
    // Convert to ACEScg color space
    float3 acescg = mul(XYZToAP1Mat, mul(D65ToD60Cat, mul(SRGBToXYZMat, color.rgb)));
    Out.m_color.rgb = acescg;
    Out.m_color.a = 1.0;

    return Out;
}
