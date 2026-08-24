/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Diagnostics.h>
#include <Jolt/HairInternal.h>

#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/limits.h>

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Jolt::Test
{
    struct SnapshotArchiveHairOffsets final
    {
        size_t m_nativePositionOffset = 0;
        size_t m_nativeRotationOffset = 0;
        size_t m_positionsDataOffset = 0;
        size_t m_positionsDataByteCount = 0;
        AZ::u64 m_positionsElementCount = 0;
        size_t m_positionsElementCountOffset = 0;
        size_t m_positionsStrideOffset = 0;
        size_t m_worldPositionOffset = 0;
        size_t m_worldRotationOffset = 0;
        size_t m_initializedOffset = 0;
        size_t m_teleportedOffset = 0;
    };

    class StateArchiveV7Cursor final
    {
    public:
        explicit StateArchiveV7Cursor(const AZStd::span<const AZ::u8> data)
            : m_data(data)
        {
        }

        [[nodiscard]]
        bool LocateSingleHairState(SnapshotArchiveHairOffsets& offsets)
        {
            AZ::u32 version = 0;
            AZ::u32 snapshotCount = 0;
            AZ::u8 multipart = 0;
            if (!Skip(sizeof(AZ::u32))
                || !Read(version)
                || !Skip(sizeof(AZ::u64))
                || !Read(snapshotCount)
                || !Read(multipart)
                || !Skip(sizeof(AZ::u64))
                || version != 7
                || snapshotCount != 1
                || multipart != 0
                || !SkipVector(sizeof(AZ::u8)))
            {
                return false;
            }

            for (AZ::u32 occupancyIndex = 0; occupancyIndex < 6; ++occupancyIndex)
            {
                if (!SkipVector(sizeof(AZ::u8)))
                {
                    return false;
                }
            }

            AZ::u32 virtualCharacterTopologyCount = 0;
            if (!Read(virtualCharacterTopologyCount)
                || !SkipElements(virtualCharacterTopologyCount, sizeof(AZ::u32) * 3))
            {
                return false;
            }

            AZ::u32 hairTopologyCount = 0;
            if (!Read(hairTopologyCount)
                || hairTopologyCount != 1
                || !SkipElements(hairTopologyCount, sizeof(AZ::u32) * 8))
            {
                return false;
            }

            AZ::u32 hairStateCount = 0;
            if (!Read(hairStateCount)
                || hairStateCount != 1
                || !SkipElements(3, sizeof(JPH::Real)))
            {
                return false;
            }
            offsets.m_nativePositionOffset = m_offset;
            if (!SkipElements(3, sizeof(JPH::Real))
                || !SkipElements(4, sizeof(float)))
            {
                return false;
            }
            offsets.m_nativeRotationOffset = m_offset;
            if (!SkipElements(4, sizeof(float)))
            {
                return false;
            }

            for (AZ::u32 bufferIndex = 0; bufferIndex < 9; ++bufferIndex)
            {
                SnapshotArchiveHairOffsets* output = nullptr;
                if (bufferIndex == 1)
                {
                    output = &offsets;
                }
                if (!SkipHairBuffer(output))
                {
                    return false;
                }
            }
            if (!Skip(sizeof(AZ::u8)))
            {
                return false;
            }

            AZ::u32 transformCount = 0;
            if (!Read(transformCount)
                || transformCount != 1)
            {
                return false;
            }
            offsets.m_worldPositionOffset = m_offset;
            if (!SkipElements(3, sizeof(double)))
            {
                return false;
            }
            offsets.m_worldRotationOffset = m_offset;
            if (!SkipElements(4, sizeof(float)))
            {
                return false;
            }

            size_t initializedCount = 0;
            if (!ReadVectorDataOffset(sizeof(AZ::u8), offsets.m_initializedOffset, initializedCount)
                || initializedCount != 1)
            {
                return false;
            }

            size_t teleportedCount = 0;
            return ReadVectorDataOffset(sizeof(AZ::u8), offsets.m_teleportedOffset, teleportedCount)
                && teleportedCount == 1
                && offsets.m_positionsDataByteCount >= sizeof(JPH_HairPosition);
        }

    private:
        template<class Type>
        bool Read(Type& value)
        {
            static_assert(std::is_trivially_copyable_v<Type>);
            if (!Skip(sizeof(Type)))
            {
                return false;
            }
            std::memcpy(&value, m_data.data() + m_offset - sizeof(Type), sizeof(Type));
            return true;
        }

        bool ReadVectorDataOffset(
            const size_t elementSize,
            size_t& dataOffset,
            size_t& elementCount)
        {
            AZ::u32 storedElementCount = 0;
            if (!Read(storedElementCount))
            {
                return false;
            }
            dataOffset = m_offset;
            elementCount = storedElementCount;
            return SkipElements(elementCount, elementSize);
        }

        bool SkipHairBuffer(SnapshotArchiveHairOffsets* offsets)
        {
            AZ::u32 dataByteCount = 0;
            if (!Read(dataByteCount))
            {
                return false;
            }
            if (offsets)
            {
                offsets->m_positionsDataOffset = m_offset;
                offsets->m_positionsDataByteCount = dataByteCount;
            }
            if (!Skip(dataByteCount))
            {
                return false;
            }

            if (offsets)
            {
                offsets->m_positionsElementCountOffset = m_offset;
                if (!Read(offsets->m_positionsElementCount))
                {
                    return false;
                }
            }
            else if (!Skip(sizeof(AZ::u64)))
            {
                return false;
            }

            if (offsets)
            {
                offsets->m_positionsStrideOffset = m_offset;
            }
            AZ::u8 present = 0;
            return Skip(sizeof(AZ::u32))
                && Read(present)
                && present <= 1;
        }

        bool SkipVector(const size_t elementSize)
        {
            size_t dataOffset = 0;
            size_t elementCount = 0;
            return ReadVectorDataOffset(elementSize, dataOffset, elementCount);
        }

        bool SkipElements(
            const size_t elementCount,
            const size_t elementSize)
        {
            if (elementSize > 0
                && elementCount > AZStd::numeric_limits<size_t>::max() / elementSize)
            {
                return false;
            }
            return Skip(elementCount * elementSize);
        }

        bool Skip(const size_t byteCount)
        {
            if (m_offset > m_data.size()
                || byteCount > m_data.size() - m_offset)
            {
                return false;
            }
            m_offset += byteCount;
            return true;
        }

        AZStd::span<const AZ::u8> m_data;
        size_t m_offset = 0;
    };

    template<class Type>
    [[nodiscard]]
    bool WriteArchiveValue(
        StateSnapshotArchive& archive,
        const size_t offset,
        const Type& value)
    {
        static_assert(std::is_trivially_copyable_v<Type>);
        if (offset > archive.m_binaryState.size()
            || sizeof(Type) > archive.m_binaryState.size() - offset)
        {
            return false;
        }
        std::memcpy(archive.m_binaryState.data() + offset, &value, sizeof(Type));
        return true;
    }

    inline void RefreshArchiveContentHash(StateSnapshotArchive& archive)
    {
        archive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(archive.m_binaryState));
    }
} // namespace Jolt::Test
