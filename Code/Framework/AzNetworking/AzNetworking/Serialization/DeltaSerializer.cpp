/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/DeltaSerializer.h>
#include <AzNetworking/Serialization/Internal/DecodeContext.h>
#include <AzNetworking/Serialization/NetworkInputSerializer.h>
#include <AzNetworking/Serialization/NetworkOutputSerializer.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <cstring>

namespace AzNetworking
{
    SerializerDelta::SerializerDelta()
        : m_dirtyBits()
    {
        ;
    }

    uint32_t SerializerDelta::GetNumDirtyBits() const
    {
        return m_dirtyBits.GetSize();
    }

    bool SerializerDelta::GetDirtyBit(uint32_t index) const
    {
        return m_dirtyBits.GetBit(index);
    }

    bool SerializerDelta::InsertDirtyBit(bool dirtyBit)
    {
        return m_dirtyBits.PushBack(dirtyBit);
    }

    uint8_t* SerializerDelta::GetBufferPtr()
    {
        if (m_overflowDeltaBytes.empty())
        {
            return m_inlineDeltaBytes.data();
        }
        return m_overflowDeltaBytes.data();
    }

    uint32_t SerializerDelta::GetBufferSize() const
    {
        return m_deltaByteSize;
    }

    uint32_t SerializerDelta::GetBufferCapacity() const
    {
        if (m_overflowDeltaBytes.empty())
        {
            return InlineBufferCapacity;
        }
        return static_cast<uint32_t>(m_overflowDeltaBytes.size());
    }

    bool SerializerDelta::EnsureBufferCapacity(const uint32_t size)
    {
        if (size <= GetBufferCapacity())
        {
            return true;
        }
        if (size > MaxPacketSize)
        {
            return false;
        }

        m_overflowDeltaBytes.resize_no_construct(MaxPacketSize);
        if (m_deltaByteSize > 0)
        {
            std::memcpy(m_overflowDeltaBytes.data(), m_inlineDeltaBytes.data(), m_deltaByteSize);
        }
        return true;
    }

    bool SerializerDelta::SetBufferSize(const uint32_t size)
    {
        if (!EnsureBufferCapacity(size))
        {
            return false;
        }
        m_deltaByteSize = size;
        return true;
    }

    bool SerializerDelta::Serialize(ISerializer& serializer)
    {
        using SizeType = AZ::SizeType<AZ::RequiredBytesForValue<MaxPacketSize>(), false>::Type;

        SizeType size = static_cast<SizeType>(m_deltaByteSize);
        if (!serializer.Serialize(m_dirtyBits, "DirtyBits")
            || !serializer.Serialize(size, "Size"))
        {
            return false;
        }
        if (serializer.GetSerializerMode() == SerializerMode::WriteToObject
            && !SetBufferSize(size))
        {
            serializer.Invalidate();
            return false;
        }

        uint32_t outSize = size;
        if (!serializer.SerializeBytes(GetBufferPtr(), GetBufferCapacity(), false, outSize, "DeltaBytes")
            || outSize != size)
        {
            serializer.Invalidate();
            return false;
        }

        m_deltaByteSize = outSize;
        return true;
    }

    DeltaSerializerCreate::DeltaSerializerCreate(SerializerDelta& delta)
        : m_delta(delta)
    {
        ;
    }

    DeltaSerializerCreate::~DeltaSerializerCreate() = default;

    SerializerMode DeltaSerializerCreate::GetSerializerMode() const
    {
        return SerializerMode::ReadFromObject;
    }

    bool DeltaSerializerCreate::Serialize(bool& value, const char* name)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(int8_t& value, const char* name, [[maybe_unused]] int8_t minValue, [[maybe_unused]] int8_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(int16_t& value, const char* name, [[maybe_unused]] int16_t minValue, [[maybe_unused]] int16_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(int32_t& value, const char* name, [[maybe_unused]] int32_t minValue, [[maybe_unused]] int32_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(long& value, const char* name, [[maybe_unused]] long minValue, [[maybe_unused]] long maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(AZ::s64& value, const char* name, [[maybe_unused]] AZ::s64 minValue, [[maybe_unused]] AZ::s64 maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(uint8_t& value, const char* name, [[maybe_unused]] uint8_t minValue, [[maybe_unused]] uint8_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(uint16_t& value, const char* name, [[maybe_unused]] uint16_t minValue, [[maybe_unused]] uint16_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(uint32_t& value, const char* name, [[maybe_unused]] uint32_t minValue, [[maybe_unused]] uint32_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(unsigned long& value, const char* name, [[maybe_unused]] unsigned long minValue, [[maybe_unused]] unsigned long maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(AZ::u64& value, const char* name, [[maybe_unused]] AZ::u64 minValue, [[maybe_unused]] AZ::u64 maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(float& value, const char* name, [[maybe_unused]] float minValue, [[maybe_unused]] float maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::Serialize(double& value, const char* name, [[maybe_unused]] double minValue, [[maybe_unused]] double maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerCreate::SerializeBytes(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        const uint32_t objectPosition = m_objectCounter;
        ++m_objectCounter;

        if (outSize > bufferCapacity || (outSize > 0 && !buffer))
        {
            Invalidate();
            return false;
        }

        if (m_gatheringRecords)
        {
            if (m_records.size() >= m_records.capacity())
            {
                Invalidate();
                return false;
            }

            ValueRecord record;
            if (!StoreRecordBytes(buffer, outSize, record))
            {
                Invalidate();
                return false;
            }
            m_records.push_back(record);
            return true;
        }

        if (objectPosition >= m_records.size())
        {
            Invalidate();
            return false;
        }

        const ValueRecord& record = m_records[objectPosition];
        const uint32_t baseOffset = static_cast<uint32_t>(record.m_value);
        const uint32_t baseSize = static_cast<uint32_t>(record.m_value >> 32);
        const bool different = baseSize != outSize
            || (outSize > 0 && std::memcmp(GetRecordBytes() + baseOffset, buffer, outSize) != 0);
        if (!m_delta.InsertDirtyBit(different))
        {
            Invalidate();
            return false;
        }

        if (different && !SerializeHelperImpl(buffer, bufferCapacity, isString, outSize, name))
        {
            return false;
        }
        return true;
    }

    bool DeltaSerializerCreate::StoreRecordBytes(
        const uint8_t* buffer,
        const uint32_t size,
        ValueRecord& record)
    {
        if (size > AZStd::numeric_limits<uint32_t>::max() - m_recordByteSize)
        {
            return false;
        }

        const uint32_t offset = m_recordByteSize;
        const uint32_t newSize = offset + size;
        record.m_value = (static_cast<uint64_t>(size) << 32) | offset;

        if (m_overflowRecordBytes.empty() && newSize <= m_inlineRecordBytes.size())
        {
            if (size > 0)
            {
                std::memcpy(m_inlineRecordBytes.data() + offset, buffer, size);
            }
        }
        else
        {
            if (m_overflowRecordBytes.empty())
            {
                const size_t reserveSize = AZStd::max<size_t>(MaxPacketSize, newSize);
                m_overflowRecordBytes.reserve(reserveSize);
                m_overflowRecordBytes.assign(
                    m_inlineRecordBytes.begin(),
                    m_inlineRecordBytes.begin() + m_recordByteSize);
            }
            m_overflowRecordBytes.resize_no_construct(newSize);
            if (size > 0)
            {
                std::memcpy(m_overflowRecordBytes.data() + offset, buffer, size);
            }
        }

        m_recordByteSize = newSize;
        return true;
    }

    const uint8_t* DeltaSerializerCreate::GetRecordBytes() const
    {
        if (m_overflowRecordBytes.empty())
        {
            return m_inlineRecordBytes.data();
        }

        return m_overflowRecordBytes.data();
    }

    bool DeltaSerializerCreate::BeginObject([[maybe_unused]] const char* name)
    {
        return true;
    }

    bool DeltaSerializerCreate::EndObject([[maybe_unused]] const char* name)
    {
        return true;
    }

    const uint8_t* DeltaSerializerCreate::GetBuffer() const
    {
        return nullptr;
    }

    uint32_t DeltaSerializerCreate::GetCapacity() const
    {
        return 0;
    }

    uint32_t DeltaSerializerCreate::GetSize() const
    {
        return 0;
    }

    template <typename T>
    bool DeltaSerializerCreate::SerializeHelper(T& value, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        static_assert(AZStd::is_trivially_copyable_v<T>);
        static_assert(sizeof(T) <= sizeof(uint64_t));

        const uint32_t objectPosition = m_objectCounter;
        ++m_objectCounter;

        if (m_gatheringRecords)
        {
            if (m_records.size() >= m_records.capacity())
            {
                Invalidate();
                return false;
            }

            ValueRecord record;
            std::memcpy(&record.m_value, &value, sizeof(T));
            m_records.push_back(record);
            return true;
        }

        if (objectPosition >= m_records.size())
        {
            Invalidate();
            return false;
        }

        T baseValue;
        std::memcpy(&baseValue, &m_records[objectPosition].m_value, sizeof(T));
        const bool different = baseValue != value;
        if (!m_delta.InsertDirtyBit(different))
        {
            Invalidate();
            return false;
        }

        return !different || SerializeHelperImpl(value, bufferCapacity, isString, outSize, name);
    }

    template <typename T>
    bool DeltaSerializerCreate::SerializeHelperImpl(T& value, uint32_t, bool, uint32_t&, const char* name)
    {
        const uint64_t requiredCapacity = static_cast<uint64_t>(m_deltaByteOffset) + sizeof(T);
        if (requiredCapacity > AZStd::numeric_limits<uint32_t>::max()
            || !m_delta.EnsureBufferCapacity(static_cast<uint32_t>(requiredCapacity)))
        {
            Invalidate();
            return false;
        }

        NetworkInputSerializer serializer(
            m_delta.GetBufferPtr() + m_deltaByteOffset,
            m_delta.GetBufferCapacity() - m_deltaByteOffset);
        ISerializer& serializerInterface = serializer;
        const bool result = serializerInterface.Serialize(value, name);
        if (!result
            || !serializer.IsValid()
            || !m_delta.SetBufferSize(m_deltaByteOffset + serializer.GetSize()))
        {
            Invalidate();
            return false;
        }
        m_deltaByteOffset += serializer.GetSize();
        return true;
    }

    bool DeltaSerializerCreate::SerializeHelperImpl(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        uint32_t lengthFieldSize = sizeof(uint8_t);
        if (bufferCapacity > AZStd::numeric_limits<uint8_t>::max())
        {
            lengthFieldSize = sizeof(uint16_t);
        }
        if (bufferCapacity > AZStd::numeric_limits<uint16_t>::max())
        {
            lengthFieldSize = sizeof(uint32_t);
        }
        const uint64_t requiredCapacity = static_cast<uint64_t>(m_deltaByteOffset)
            + outSize
            + lengthFieldSize;
        if (requiredCapacity > AZStd::numeric_limits<uint32_t>::max()
            || !m_delta.EnsureBufferCapacity(static_cast<uint32_t>(requiredCapacity)))
        {
            Invalidate();
            return false;
        }

        NetworkInputSerializer serializer(
            m_delta.GetBufferPtr() + m_deltaByteOffset,
            m_delta.GetBufferCapacity() - m_deltaByteOffset);
        const bool result = serializer.SerializeBytes(buffer, bufferCapacity, isString, outSize, name);
        if (!result
            || !serializer.IsValid()
            || !m_delta.SetBufferSize(m_deltaByteOffset + serializer.GetSize()))
        {
            Invalidate();
            return false;
        }
        m_deltaByteOffset += serializer.GetSize();
        return true;
    }

    DeltaSerializerApply::DeltaSerializerApply(SerializerDelta& delta)
        : m_delta(delta)
        , m_dataSerializer(m_delta.GetBufferPtr(), m_delta.GetBufferSize())
    {
        ;
    }

    DeltaSerializerApply::~DeltaSerializerApply()
    {
        ;
    }

    SerializerMode DeltaSerializerApply::GetSerializerMode() const
    {
        return SerializerMode::WriteToObject;
    }

    bool DeltaSerializerApply::Serialize(bool& value, const char* name)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(int8_t& value, const char* name, [[maybe_unused]] int8_t minValue, [[maybe_unused]] int8_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(int16_t& value, const char* name, [[maybe_unused]] int16_t minValue, [[maybe_unused]] int16_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(int32_t& value, const char* name, [[maybe_unused]] int32_t minValue, [[maybe_unused]] int32_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(long& value, const char* name, [[maybe_unused]] long minValue, [[maybe_unused]] long maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(AZ::s64& value, const char* name, [[maybe_unused]] AZ::s64 minValue, [[maybe_unused]] AZ::s64 maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(uint8_t& value, const char* name, [[maybe_unused]] uint8_t minValue, [[maybe_unused]] uint8_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(uint16_t& value, const char* name, [[maybe_unused]] uint16_t minValue, [[maybe_unused]] uint16_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(uint32_t& value, const char* name, [[maybe_unused]] uint32_t minValue, [[maybe_unused]] uint32_t maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(unsigned long& value, const char* name, [[maybe_unused]] unsigned long minValue, [[maybe_unused]] unsigned long maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(AZ::u64& value, const char* name, [[maybe_unused]] AZ::u64 minValue, [[maybe_unused]] AZ::u64 maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(float& value, const char* name, [[maybe_unused]] float minValue, [[maybe_unused]] float maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::Serialize(double& value, const char* name, [[maybe_unused]] double minValue, [[maybe_unused]] double maxValue)
    {
        uint32_t unused = 0;
        return SerializeHelper(value, 0, false, unused, name);
    }

    bool DeltaSerializerApply::SerializeBytes(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        return SerializeHelper(buffer, bufferCapacity, isString, outSize, name);
    }

    bool DeltaSerializerApply::BeginObject([[maybe_unused]] const char *name)
    {
        return true;
    }

    bool DeltaSerializerApply::EndObject([[maybe_unused]] const char *name)
    {
        return true;
    }

    const uint8_t* DeltaSerializerApply::GetBuffer() const
    {
        return nullptr;
    }

    uint32_t DeltaSerializerApply::GetCapacity() const
    {
        return 0;
    }

    uint32_t DeltaSerializerApply::GetSize() const
    {
        return 0;
    }

    template <typename T>
    bool DeltaSerializerApply::SerializeHelper(T& value, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        // If we have run out of delta records, something has gone wrong
        if (m_nextDirtyBit >= m_delta.GetNumDirtyBits())
        {
            Invalidate();
            return false;
        }

        const bool hasRecord = m_delta.GetDirtyBit(m_nextDirtyBit);
        ++m_nextDirtyBit;

        // No record in the delta for this field, just skip it
        if (!hasRecord)
        {
            return true; // This isn't an error
        }

        // There is a record, so serialize the value out of the delta
        return SerializeHelperImpl(value, bufferCapacity, isString, outSize, name);
    }

    template <typename T>
    bool DeltaSerializerApply::SerializeHelperImpl(T& value, uint32_t, bool, uint32_t&, const char* name)
    {
        Internal::DecodeForwardScope decodeScope(*this, m_dataSerializer);
        ISerializer& serializer = m_dataSerializer;
        const bool result = serializer.Serialize(value, name);
        if (!result)
        {
            Invalidate();
        }
        return result;
    }

    bool DeltaSerializerApply::SerializeHelperImpl(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        Internal::DecodeForwardScope decodeScope(*this, m_dataSerializer);
        ISerializer& serializer = m_dataSerializer;
        const bool result = serializer.SerializeBytes(buffer, bufferCapacity, isString, outSize, name);
        if (!result)
        {
            Invalidate();
        }
        return result;
    }
}
