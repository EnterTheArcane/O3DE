/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import OpenParticle.ParticleCommon;

public struct ParticleMeshInstanceData
{
    public float4 m_offset;
    public float4 m_color;
    public float4 m_initRotation;
    public float4 m_rotationQuat;
    public float4 m_scale;
    public uint4  m_hasInstanceData;
};
