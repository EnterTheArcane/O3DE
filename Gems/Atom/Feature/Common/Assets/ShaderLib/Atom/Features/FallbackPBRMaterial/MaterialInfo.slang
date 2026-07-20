
/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PBR.Lights.ReflectionProbeData;

// material info structured buffer
public struct MaterialInfo
{
    public float4 m_baseColor;

    public float4 m_irradianceColor;

    public float3 m_emissiveColor;
    public float  m_metallicFactor;

    public float m_roughnessFactor;
    public int m_baseColorImage;
    public int m_normalImage;
    public int m_metallicImage;
    
    public int m_roughnessImage;
    public int m_emissiveImage;
    public uint m_reflectionProbeCubeMapIndex;
    public uint m_pad;

    public ReflectionProbeData m_reflectionProbeData;
    // TODO(slang-port): [[pad_to(16)]] - add explicit padding (unknown member type 'ReflectionProbeData')
};
