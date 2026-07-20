/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



#ifndef MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS
#define MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS
#endif

[AtomShaderResourceGroup(SrgBindingSlot.Object)]
public struct ObjectSrgLayout
{
    public row_major float3x4 m_modelToWorld;
    public row_major float3x4 m_modelToWorldInverse; // does not include extents
    public float3 m_outerObbHalfLengths;
    public float3 m_innerObbHalfLengths;
    public bool m_useParallaxCorrection;
    public float m_exposure;
    public TextureCube m_reflectionCubeMap;

    public float4x4 GetWorldMatrix()
    {
        float4x4 modelToWorld = float4x4(
            float4(1, 0, 0, 0),
            float4(0, 1, 0, 0),
            float4(0, 0, 1, 0),
            float4(0, 0, 0, 1));

        modelToWorld[0] = ObjectSrg.m_modelToWorld[0];
        modelToWorld[1] = ObjectSrg.m_modelToWorld[1];
        modelToWorld[2] = ObjectSrg.m_modelToWorld[2];
        return modelToWorld;
    }

    public float4x4 GetWorldMatrixInverse()
    {
        float4x4 modelToWorldInverse = float4x4(
            float4(1, 0, 0, 0),
            float4(0, 1, 0, 0),
            float4(0, 0, 1, 0),
            float4(0, 0, 0, 1));

        modelToWorldInverse[0] = ObjectSrg.m_modelToWorldInverse[0];
        modelToWorldInverse[1] = ObjectSrg.m_modelToWorldInverse[1];
        modelToWorldInverse[2] = ObjectSrg.m_modelToWorldInverse[2];
        return modelToWorldInverse;
    }

    public MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS
};
public ParameterBlock<ObjectSrgLayout> ObjectSrg;
