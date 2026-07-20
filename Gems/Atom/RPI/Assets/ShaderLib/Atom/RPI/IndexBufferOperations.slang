/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.RPI.Math;
#include <Atom/RHI/IndexFormat.slang>
__exported import Atom.RPI.VertexBufferOperations;

public uint CalculateIndexLoadOffset(const uint index, const uint startOffset, const uint bufferStride)
{
    return index * bufferStride + startOffset;
}

public uint3 LoadVertexIndices(ByteAddressBuffer buffer, uint byteOffset, IndexFormat format)
{
    switch (format)
    {
    case IndexFormat::Uint16:
        return LoadUint16x3FromBufferUnaligned(buffer, byteOffset);
    case IndexFormat::Uint32:
        return buffer.Load3(byteOffset);
    default:
        return uint3(0, 0, 0);
    }
}
