/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


import Atom.Features.PostProcessing.FullscreenPixelInfo;
import Atom.Features.PostProcessing.FullscreenVertex;
import Atom.Features.PostProcessing.PostProcessUtil;

import Atom.Features.ColorManagement.TransformColor;

////////////////////////////////////////////////////////////////////////////////

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2D<float4> m_framebuffer;
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
};
ParameterBlock<PassSrgLayout> PassSrg;

////////////////////////////////////////////////////////////////////////////////
// A entry point of pixel shader.
PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    float3 color = PassSrg.m_framebuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord).rgb;
    OUT.m_color.rgb = TransformColor(color, ColorSpaceId::ACEScg, ColorSpaceId::SRGB);
    OUT.m_color.w = 1;

    return OUT;
}
