/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// VkClusterAccelerationStructureClusterFlagBitsNV
public enum class RayTracingClasClusterFlags
{
    AllowDisableOpacityMicromaps = 1,
};

// VkClusterAccelerationStructureIndexFormatFlagBitsNV
public enum class RayTracingClasGeometryFlags
{
    CullDisable = 1,
    NoDuplicateAnyHitInvocation = 2,
    Opaque = 4,
};

// VkClusterAccelerationStructureIndexFormatFlagBitsNV
public enum class RayTracingClasIndexFormat
{
    UINT8 = 1,
    UINT16 = 2,
    UINT32 = 4,
};

// VkClusterAccelerationStructureBuildTriangleClusterInfoNV
// Bit-packed fields expanded such that each member can be written to individually.
public struct RayTracingClasBuildTriangleClusterInfoExpanded
{
    public uint                        m_clusterID;
    public RayTracingClasClusterFlags  m_clusterFlags;
    public uint                        m_triangleCount;
    public uint                        m_vertexCount;
    public uint                        m_positionTruncateBitCount;
    public RayTracingClasIndexFormat   m_indexType;
    public RayTracingClasIndexFormat   m_opacityMicromapIndexType;
    public uint                        m_baseGeometryIndex;
    public RayTracingClasGeometryFlags m_geometryFlags;
    public uint                        m_indexBufferStride;
    public uint                        m_vertexBufferStride;
    public uint                        m_geometryIndexAndFlagsBufferStride;
    public uint                        m_opacityMicromapIndexBufferStride;
    public uint64_t                    m_indexBufferAddress;
    public uint64_t                    m_vertexBufferAddress;
    public uint64_t                    m_geometryIndexAndFlagsBufferAddress;
    public uint64_t                    m_opacityMicromapArrayAddress;
    public uint64_t                    m_opacityMicromapIndexBufferAddress;
};

// VkClusterAccelerationStructureBuildTriangleClusterInfoNV
// Packed struct which matches the size of the Vulkan struct such that it can be used directly for calls to the Vulkan API.
public struct RayTracingClasBuildTriangleClusterInfo
{
    public uint     m_clusterID;
    public uint     m_clusterFlags;
    public uint     m_vertexAndIndexConfiguration;
    public uint     m_baseGeometryIndexAndGeometryFlags;
    public uint     m_indexAndVertexBufferStride;
    public uint     m_geometryIndexAndOpacityIndexIndexBufferStride;
    public uint64_t m_indexBufferAddress;
    public uint64_t m_vertexBufferAddress;
    public uint64_t m_geometryIndexAndFlagsBufferAddress;
    public uint64_t m_opacityMicromapArrayAddress;
    public uint64_t m_opacityMicromapIndexBufferAddress;
};

// Stores per-cluster buffer information, similar to the MeshInfo struct (MeshInfo.azsli).
// When a cluster is hit in a ray tracing hit shader, these values are used instead of the values from the MeshInfo struct to read the
// vertex data.
public struct RayTracingClasClusterOffsetInfo
{
    public uint m_indexBindlessReadIndex;
    public uint m_indexOffset;
    public uint m_indexStride;

    public uint m_positionBindlessReadIndex;
    public uint m_positionOffset;
    public uint m_positionStride;

    public uint m_normalBindlessReadIndex;
    public uint m_normalOffset;
    public uint m_normalStride;

    public uint m_uv0BindlessReadIndex;
    public uint m_uv0Offset;
    public uint m_uv0Stride;

    public uint m_uv1BindlessReadIndex;
    public uint m_uv1Offset;
    public uint m_uv1Stride;

    public uint m_tangentBindlessReadIndex;
    public uint m_tangentOffset;
    public uint m_tangentStride;

    public uint m_bitangentBindlessReadIndex;
    public uint m_bitangentOffset;
    public uint m_bitangentStride;
};

public inline RayTracingClasBuildTriangleClusterInfo RayTracingClasConvertBuildTriangleClusterInfo(
    RayTracingClasBuildTriangleClusterInfoExpanded sourceInfo)
{
    RayTracingClasBuildTriangleClusterInfo targetInfo;

    // Since bit-packed struct fields (eg. "uint m_triangleCount : 9;") cannot be used in azsl, the struct data nees to be manually packed
    // into the format required by the graphics API.

    targetInfo.m_clusterID = sourceInfo.m_clusterID;
    targetInfo.m_clusterFlags = (uint)sourceInfo.m_clusterFlags;
    targetInfo.m_vertexAndIndexConfiguration =
        (sourceInfo.m_triangleCount                   & 0x1FF) |        // 9 bits [0-8]
        ((sourceInfo.m_vertexCount                    & 0x1FF) <<  9) | // 9 bits [9-17]
        ((sourceInfo.m_positionTruncateBitCount       & 0x3F)  << 18) | // 6 bits [18-23]
        (((uint)sourceInfo.m_indexType                & 0xF)   << 24) | // 4 bits [24-27]
        (((uint)sourceInfo.m_opacityMicromapIndexType & 0xF)   << 28);  // 4 bits [28-31]
    targetInfo.m_baseGeometryIndexAndGeometryFlags =
        (sourceInfo.m_baseGeometryIndex    & 0xFFFFFF) |  // 24 bits [0-23]
                                                          //  5 bits [24-28] (reserved)
        (((uint)sourceInfo.m_geometryFlags & 0x7) << 29); //  3 bits [29-31]
    targetInfo.m_indexAndVertexBufferStride =
        (sourceInfo.m_indexBufferStride   & 0xFFFF) |        // 16 bits [0-15]
        ((sourceInfo.m_vertexBufferStride & 0xFFFF) << 16);  // 16 bits [16-31]
    targetInfo.m_geometryIndexAndOpacityIndexIndexBufferStride =
        (sourceInfo.m_geometryIndexAndFlagsBufferStride & 0xFFFF) |       // 16 bits [0-15]
        ((sourceInfo.m_opacityMicromapIndexBufferStride & 0xFFFF) << 16); // 16 bits [16-31]
    targetInfo.m_indexBufferAddress = sourceInfo.m_indexBufferAddress;
    targetInfo.m_vertexBufferAddress = sourceInfo.m_vertexBufferAddress;
    targetInfo.m_geometryIndexAndFlagsBufferAddress = sourceInfo.m_geometryIndexAndFlagsBufferAddress;
    targetInfo.m_opacityMicromapArrayAddress = sourceInfo.m_opacityMicromapArrayAddress;
    targetInfo.m_opacityMicromapIndexBufferAddress = sourceInfo.m_opacityMicromapIndexBufferAddress;

    return targetInfo;
}
