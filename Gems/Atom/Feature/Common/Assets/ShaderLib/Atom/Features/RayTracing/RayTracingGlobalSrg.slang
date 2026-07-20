/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
public struct RayTracingGlobalSrgLayout
{
    public Texture2DMS<float>    m_depth;
    public Texture2DMS<float4>   m_normal;       // RGB10 = Normal (Encoded), A2 = Flags
    public Texture2DMS<float4>   m_specularF0;   // RGB8 = SpecularF0, A8 = Roughness
    public Texture2D<float2>     m_brdfMap;
    public RWTexture2D<float4>   m_hitCoords;
    public RWTexture2D<float4>   m_fallbackColor;
    public RWTexture2D<float4>   m_fallbackAlbedo;
    public RWTexture2D<float4>   m_fallbackPosition;
    public RWTexture2D<float4>   m_fallbackNormal;

    public float m_invOutputScale;
    public float m_maxRayLength;
    public float m_maxRoughness;
    public uint m_reflectionMethod;
    public bool  m_rayTraceFallbackSpecular;
    // TODO(slang-port): [[pad_to(16)]] - add explicit padding (unknown member type 'Texture2DMS<float>')

    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState LinearSampler;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState PointSampler;
};
public ParameterBlock<RayTracingGlobalSrgLayout> RayTracingGlobalSrg;
