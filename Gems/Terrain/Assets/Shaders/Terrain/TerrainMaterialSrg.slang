/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



[AtomShaderResourceGroup(SrgBindingSlot.Material)]
public struct TerrainMaterialSrgLayout
{
    [AtomStaticSampler("Linear", "Linear", "Linear", "Wrap", "Wrap", "Wrap", 16, "Never", "Filter", "TransparentBlack")]
    public SamplerState m_detailSampler;

    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Wrap", 16, "Never", "Filter", "TransparentBlack")]
    public SamplerState m_macroSampler;

    // Base Color
    public float3 m_baseColor;

    // Detail Material Properties
    public float m_detailTextureMultiplier;
    public float m_detailFadeDistance;
    public float m_detailFadeLength;
};
public ParameterBlock<TerrainMaterialSrgLayout> TerrainMaterialSrg;
