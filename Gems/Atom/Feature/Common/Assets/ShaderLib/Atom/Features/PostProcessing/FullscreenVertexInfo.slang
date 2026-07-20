/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


public struct VSInput
{
    public uint m_vertexID : SV_VertexID;
};

public struct VSOutput
{
    public float4 m_position : SV_Position;
    public float2 m_texCoord : TEXCOORD0;
};
