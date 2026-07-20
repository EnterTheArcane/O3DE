/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
  
#pragma once


#ifndef USE_CUSTOM_DRAW_SRG
#define USE_CUSTOM_DRAW_SRG 0
#endif

#ifndef USE_DRAWSRG_MESHLOD_MESHINDEX
#define USE_DRAWSRG_MESHLOD_MESHINDEX 0
#endif

#ifndef MATERIAL_PIPELINE_DRAW_SRG_MEMBERS
#define MATERIAL_PIPELINE_DRAW_SRG_MEMBERS
#endif

#if USE_CUSTOM_DRAW_SRG
#else
[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
public struct DrawSrgLayout
{
    // This SRG is unique per draw packet
    public uint m_uvStreamTangentBitmask;

    public uint m_meshInfoIndex;

#if USE_DRAWSRG_MESHLOD_MESHINDEX
    // See MeshDrawPacket.h
    public uint m_modelLodMeshIndex;
#endif

    public uint GetTangentAtUv(uint uvIndex)
    {
        return (m_uvStreamTangentBitmask >> (4 * uvIndex)) & 0xF;
    }

    public MATERIAL_PIPELINE_DRAW_SRG_MEMBERS
};
public ParameterBlock<DrawSrgLayout> DrawSrg;
#endif
