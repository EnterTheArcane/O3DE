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
import "SMAAUtils.slang";

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
    PSOutput OUT = (PSOutput)0;
    
    OUT.m_color = ApplyProvisionalTonemap(PassSrg.m_framebuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord));

    return OUT;
}
