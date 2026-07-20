/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
public struct ObjectSrgLayout
{
    public float4 m_color;
    public row_major float3x4 m_modelToWorld;
    public row_major float3x3 m_normalMatrix;
    public row_major float4x4 m_viewProjectionOverride;
    public float m_pointSize;

    //! Returns the matrix for transforming points from Object Space to World Space.
    public float4x4 GetWorldMatrix()
    {
        float4x4 modelToWorld = float4x4(
            float4(1, 0, 0, 0),
            float4(0, 1, 0, 0),
            float4(0, 0, 1, 0),
            float4(0, 0, 0, 1));

        modelToWorld[0] = m_modelToWorld[0];
        modelToWorld[1] = m_modelToWorld[1];
        modelToWorld[2] = m_modelToWorld[2];
        return modelToWorld;
    }

    //! Returns the inverse-transpose of the world matrix.
    //! Commonly used to transform normals while supporting non-uniform scale.
    public float3x3 GetWorldMatrixInverseTranspose()
    {
        return m_normalMatrix;
    }
};
public ParameterBlock<ObjectSrgLayout> ObjectSrg;
