/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


import Atom.Features.PostProcessing.FullscreenPixelInfo;
#include <Atom/Features/PostProcessing/FullscreenVertex.slang>
import Atom.Features.PostProcessing.PostProcessUtil;
import Atom.Features.PostProcessing.Aces;

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2D<float4> m_color;
    Texture3D<float4> m_lut;
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
    float m_shaperBias;
    float m_shaperScale;
};
ParameterBlock<PassSrgLayout> PassSrg;

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    float2 uvCoord = float2(IN.m_texCoord.x, IN.m_texCoord.y);
    float4 acescg = PassSrg.m_color.Sample(PassSrg.LinearSampler, uvCoord);
    float3 color = mul(XYZToSRGBMat, mul(D60ToD65Cat, mul(AP1ToXYZMat, acescg.rgb)));
    float3 lutCoordinate = log(color.rgb)/log(2.0) * PassSrg.m_shaperScale + PassSrg.m_shaperBias;

    OUT.m_color = PassSrg.m_lut.Sample(PassSrg.LinearSampler, lutCoordinate);
    OUT.m_color.a = 1.0f;

    return OUT;
}
