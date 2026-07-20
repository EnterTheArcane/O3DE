/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
public struct PassSrgLayout
{
    // [GFX TODO][ATOM-2012] adapt to multiple shadowmaps
    public Texture2DArray<float> m_directionalLightShadowmap;
    public Texture2DArray<float> m_directionalLightExponentialShadowmap;
    public Texture2DArray<float> m_projectedShadowmaps;
    public Texture2DArray<float> m_projectedExponentialShadowmap;
    public Texture2D m_brdfMap;
    public Texture2D<float> m_fullscreenShadow;

    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState LinearSampler;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState PointSampler;
    
    public Texture2D<uint4> m_tileLightData;
    public StructuredBuffer<uint> m_lightListRemapped;
    public Texture2D<float> m_linearDepthTexture;

    // Whether the m_linearDepthTexture is linear depth or original depth
    // This is useful for render pipeline which doesn't have a pass to convert original depth to linear depth
    // We can use m_linearDepthTexture for original depth and set this flag to true. 
    // And the shaders can calculate linear depth themselves
    public uint m_isOriginalDepth;
};
public ParameterBlock<PassSrgLayout> PassSrg;




