/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



// this PassSrg of the DeferredPass looks almost the same as the PassSrg of the ForwardPass.
// The only difference is that it requires the Visibility-Buffer textures as input
[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
public struct PassSrgLayout
{
    public Texture2D m_brdfMap;
    public Texture2D<float> m_fullscreenShadow;

    // [GFX TODO][ATOM-2012] adapt to multiple shadowmaps
    public Texture2DArray<float> m_directionalLightShadowmap;
    public Texture2DArray<float> m_directionalLightExponentialShadowmap;
    public Texture2DArray<float> m_projectedShadowmaps;
    public Texture2DArray<float> m_projectedExponentialShadowmap;


    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState LinearSampler;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState PointSampler;
    
    public Texture2D<uint4> m_tileLightData;
    public StructuredBuffer<uint> m_lightListRemapped;

    // The Visibility buffer (as implemented in VisibilityBuffer.azsli) is packed into two float4 textures, 
    // mostly because the screenspace derivatives of the barycentrics use one entire float4
    public Texture2D<float4> m_visibilityBuffer_0;
    public Texture2D<float4> m_visibilityBuffer_1;
};
public ParameterBlock<PassSrgLayout> PassSrg;




