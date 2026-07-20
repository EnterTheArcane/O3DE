/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

// The SceneSrg free functions, ported field-for-field from the AZSL editorial source
// (ShaderResourceGroups/SceneSrg.azsli). Textually included into the SceneSrgAccessor body by the
// generated SceneSrg.slang, so the member references below resolve through the accessor's forwarding
// properties. Add a function here whenever a shader calls a new SceneSrg::Fn().

float4x4 GetObjectToWorldMatrix(uint objectId)
{
    return float4x4(
        m_objectToWorldBuffer[objectId].m_transform[0],
        m_objectToWorldBuffer[objectId].m_transform[1],
        m_objectToWorldBuffer[objectId].m_transform[2],
        float4(0, 0, 0, 1));
}

float3x3 GetObjectToWorldInverseTransposeMatrix(uint objectId)
{
    return float3x3(
        m_objectToWorldInverseTransposeBuffer[objectId].m_transform[0].xyz,
        m_objectToWorldInverseTransposeBuffer[objectId].m_transform[1].xyz,
        m_objectToWorldInverseTransposeBuffer[objectId].m_transform[2].xyz
    );
}

float4x4 GetObjectToWorldMatrixPrev(uint objectId)
{
    return float4x4(
        m_objectToWorldHistoryBuffer[objectId].m_transform[0],
        m_objectToWorldHistoryBuffer[objectId].m_transform[1],
        m_objectToWorldHistoryBuffer[objectId].m_transform[2],
        float4(0, 0, 0, 1));
}
