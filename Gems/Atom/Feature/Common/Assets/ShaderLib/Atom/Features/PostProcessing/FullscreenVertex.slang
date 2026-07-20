/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

__exported import Atom.Features.PostProcessing.FullscreenVertexInfo;
__exported import Atom.Features.PostProcessing.FullscreenVertexUtil;

// Generates a fullscreen triangle from pipeline provided vertex id 
public VSOutput MainVS(VSInput input)
{
    VSOutput OUT;

    float4 posTex = GetVertexPositionAndTexCoords(input.m_vertexID);
    
    OUT.m_texCoord = float2(posTex.z, posTex.w);
    OUT.m_position = float4(posTex.x, posTex.y, 0.0, 1.0);

    return OUT;
}
