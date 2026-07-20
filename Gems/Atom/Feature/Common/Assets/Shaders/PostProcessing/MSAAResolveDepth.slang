/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


import Atom.Features.PostProcessing.FullscreenVertex;
import Atom.Features.PostProcessing.PostProcessUtil;

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2DMS<float> m_depthTexture;
};
ParameterBlock<PassSrgLayout> PassSrg;


struct PSOutput
{
    float m_depth : SV_Depth;
};


PSOutput MainPS(VSOutput IN)
{
    uint width = 0;
    uint height = 0;
    uint numberOfSamples = 0;
    PassSrg.m_depthTexture.GetDimensions(width, height, numberOfSamples);

    int2 coord = int2(width * IN.m_texCoord.x, height * IN.m_texCoord.y);

    PSOutput OUT;
  
    int sampleIndex = 0;
    OUT.m_depth = PassSrg.m_depthTexture.Load(coord, sampleIndex);

    return OUT;
}
