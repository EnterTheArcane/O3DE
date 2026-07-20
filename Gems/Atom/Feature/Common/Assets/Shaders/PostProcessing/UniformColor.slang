/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


import Atom.Features.PostProcessing.FullscreenPixelInfo;
#include <Atom/Features/PostProcessing/FullscreenVertex.slang>

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    float4 m_color;
};
ParameterBlock<PassSrgLayout> PassSrg;

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT = (PSOutput)0;
    OUT.m_color = PassSrg.m_color;
    return OUT;
}
