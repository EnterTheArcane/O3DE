/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import OpenParticle.ParticleMeshCommon;

[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
public struct DrawSrgLayout
{
    public uint m_uvStreamTangentBitmask;

    public uint GetTangentAtUv(uint uvIndex)
    {
        return (m_uvStreamTangentBitmask >> (4 * uvIndex)) & 0xF;
    }

    public StructuredBuffer<ParticleMeshInstanceData> m_particleInstanceData;
};
public ParameterBlock<DrawSrgLayout> DrawSrg;

__exported import OpenParticle.ParticleMeshHelpers;
