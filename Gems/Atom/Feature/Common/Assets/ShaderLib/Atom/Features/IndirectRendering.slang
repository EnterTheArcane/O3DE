/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// At the moment all RHI APIs share the same struct layout for the commands.
// This could be become a per API header if needed in the future.

public struct DrawIndexedIndirectCommand
{
    public uint m_indexCountPerInstance;
    public uint m_instanceCount;
    public uint m_startIndexLocation;
    public int m_baseVertexLocation;
    public uint m_startInstanceLocation;
};

public struct DrawIndirectCommand
{
    public uint m_vertexCountPerInstance;
    public uint m_instanceCount;
    public uint m_startVertexLocation;
    public uint m_startInstanceLocation;
};

public struct DispatchIndirectCommand
{
    public uint m_threadGroupCountX;
    public uint m_threadGroupCountY;
    public uint m_threadGroupCountZ;
};

public struct DispatchRaysIndirectCommand
{
    public uint m_width;
    public uint m_height;
    public uint m_depth;
};

public struct VertexViewIndirectCommand
{
    public uint4 m_vertexArguments;
};

public struct IndexViewIndirectCommand
{
    public uint4 m_indexArguments;
};
