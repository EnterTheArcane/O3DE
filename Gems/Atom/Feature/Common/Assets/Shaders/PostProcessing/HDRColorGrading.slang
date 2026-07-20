/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import Atom.RPI.Math;


import Atom.Features.PostProcessing.FullscreenPixelInfo;
import Atom.Features.PostProcessing.FullscreenVertex;

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    // get the framebuffer
    Texture2D<float4> m_framebuffer;

    // framebuffer sampler
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;

    float m_colorAdjustmentWeight;
    float m_colorGradingExposure;
    float m_colorGradingContrast;
    float m_colorGradingPreSaturation;
    float m_colorFilterIntensity;
    float m_colorFilterMultiply;
    float4 m_colorFilterSwatch;

    float m_whiteBalanceWeight;
    float m_whiteBalanceKelvin;
    float m_whiteBalanceTint;
    float m_whiteBalanceLuminancePreservation;

    float m_splitToneBalance;
    float m_splitToneWeight;
    float4 m_splitToneShadowsColor;
    float4 m_splitToneHighlightsColor;

    float m_smhShadowsStart;
    float m_smhShadowsEnd;
    float m_smhHighlightsStart;
    float m_smhHighlightsEnd;
    float m_smhWeight;
    float4 m_smhShadowsColor;
    float4 m_smhMidtonesColor;
    float4 m_smhHighlightsColor;

    float3 m_channelMixingRed;
    float3 m_channelMixingGreen;
    float3 m_channelMixingBlue;

    float m_finalAdjustmentWeight;
    float m_colorGradingPostSaturation;
    float m_colorGradingHueShift;
};
ParameterBlock<PassSrgLayout> PassSrg;

import Atom.Features.PostProcessing.HDRColorGradingCommon;

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    // Fetch the pixel color from the input texture
    float3 frameColor = PassSrg.m_framebuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord).rgb;

    OUT.m_color.rgb = ColorGrade(frameColor);
    OUT.m_color.w = 1;

    return OUT;
}
