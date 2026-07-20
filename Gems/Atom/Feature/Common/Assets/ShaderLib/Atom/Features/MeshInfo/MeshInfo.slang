/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

public enum MeshInfoFlags
{
    SkinnedMesh            = 1,  // true if the mesh is animated
    ClusterGeometryBuffers = 2,  // true if each cluster should have its own geometry buffer
};


// This struct needs to be padded to 16 byte in both azsl and c++
public struct MeshInfo
{
    // Buffer indices                 // 16 byte (float4) offsets
    public int m_indexBufferIndex;           // [0.0]
    public int m_positionBufferIndex;        // [0.1]
    public int m_normalBufferIndex;          // [0.2]
    public int m_uv0BufferIndex;             // [0.3]
    public int m_uv1BufferIndex;             // [1.0]
    public int m_tangentBufferIndex;         // [1.1]
    public int m_bitangentBufferIndex;       // [1.2]
    public int m_clusterBufferIndex;         // [1.3]

    // Buffer start offsets
    public uint m_indexBufferByteOffset;     // [2.0]
    public uint m_positionBufferByteOffset;  // [2.1]
    public uint m_normalBufferByteOffset;    // [2.2]
    public uint m_uv0BufferByteOffset;       // [2.3]
    public uint m_uv1BufferByteOffset;       // [3.0]
    public uint m_tangentBufferByteOffset;   // [3.1]
    public uint m_bitangentBufferByteOffset; // [3.2]

    // Buffer strides
    public uint m_indexBufferByteStride;     // [3.3]
    public uint m_positionBufferByteStride;  // [4.0]
    public uint m_normalBufferByteStride;    // [4.1]
    public uint m_uv0BufferByteStride;       // [4.2]
    public uint m_uv1BufferByteStride;       // [4.3]
    public uint m_tangentBufferByteStride;   // [5.0]
    public uint m_bitangentBufferByteStride; // [5.1]

    // Buffer formats
    public uint m_indexFormat;               // [5.2]
    public uint m_positionFormat;            // [5.3]
    public uint m_normalFormat;              // [6.0]
    public uint m_uv0Format;                 // [6.1]
    public uint m_uv1Format;                 // [6.2]
    public uint m_tangentFormat;             // [6.3]
    public uint m_bitangentFormat;           // [7.0]

    // Other mesh parameters
    public uint m_objectIdForTransform;      // [7.1]
    public uint m_lightingChannels;          // [7.2]
    public uint m_materialTypeId;            // [7.3]
    public uint m_materialInstanceId;        // [8.0]
    public uint m_uvStreamTangentBitmask;    // [8.1]
    public uint m_flags;                     // [8.2]

    public int m_padding;                    // [8.3]
};
