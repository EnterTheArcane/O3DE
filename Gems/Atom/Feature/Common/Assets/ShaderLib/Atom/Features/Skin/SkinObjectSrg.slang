/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

__exported import Atom.Features.PBR.Lights.ReflectionProbeData;

#ifndef MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS
#define MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS
#endif

[AtomShaderResourceGroup(SrgBindingSlot.Object)]
public struct ObjectSrgLayout
{
    public uint m_objectId;
    public int m_materialTypeId;
    public int m_materialInstanceId;

    public uint m_wrinkle_mask_count;
    public float4 m_wrinkle_mask_weights[4];
    public Texture2D m_wrinkle_masks[16];

    public float GetWrinkleMaskWeight(uint index)
    {
        return m_wrinkle_mask_weights[index / 4][index % 4];
    }

    public ReflectionProbeData m_reflectionProbeData;
    public TextureCube m_reflectionProbeCubeMap;

    public MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS
    uint m_lightingChannelMask;
};
public ParameterBlock<ObjectSrgLayout> ObjectSrg;
