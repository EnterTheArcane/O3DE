/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/DeltaSerializer.h>
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
        , m_deltaBytes()
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
        return m_deltaBytes.GetBuffer();
    }

    uint32_t SerializerDelta::GetBufferSize() const
    {
        return static_cast<uint32_t>(m_deltaBytes.GetSize());
    }

    uint32_t SerializerDelta::GetBufferCapacity() const
    {
        return static_cast<uint32_t>(m_deltaBytes.GetCapacity());
    }

    void SerializerDelta::SetBufferSize(uint32_t size)
    {
        m_deltaBytes.Resize(size);
    }

    bool SerializerDelta::Serialize(ISerializer& serializer)
    {
        return serializer.Serialize(m_dirtyBits, "DirtyBits")
            && serializer.Serialize(m_deltaBytes, "DeltaBytes");
    }

    DeltaSerializerCreate::DeltaSerializerCreate(SerializerDelta& delta)
        : m_delta(delta)
        , m_dataSerializer(m_delta.GetBufferPtr(), m_delta.GetBufferCapacity())
    {
        ;
    }

    DeltaSerializerCreate::DeltaSerializerCreate(
        SerializerDelta& delta,
        const Internal::SymbolSerializationContext& symbolSerializationContext)
        : ISerializer(&symbolSerializationContext)
        , m_delta(delta)
        , m_dataSerializer(
            m_delta.GetBufferPtr(),
            m_delta.GetBufferCapacity(),
            symbolSerializationContext)
    {
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
        ISerializer& ser = m_dataSerializer; // Use interface since it fills in defaulted type info parameters
        return ser.Serialize(value, name);
    }

    bool DeltaSerializerCreate::SerializeHelperImpl(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        ISerializer& ser = m_dataSerializer; // Use interface since it fills in defaulted type info parameters
        return ser.SerializeBytes(buffer, bufferCapacity, isString, outSize, name);
    }

    DeltaSerializerApply::DeltaSerializerApply(SerializerDelta& delta)
        : m_delta(delta)
        , m_dataSerializer(m_delta.GetBufferPtr(), m_delta.GetBufferSize())
    {
        ;
    }

    DeltaSerializerApply::DeltaSerializerApply(
        SerializerDelta& delta,
        const Internal::SymbolSerializationContext& symbolSerializationContext)
        : ISerializer(&symbolSerializationContext)
        , m_delta(delta)
        , m_dataSerializer(
            m_delta.GetBufferPtr(),
            m_delta.GetBufferSize(),
            symbolSerializationContext)
    {
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
        ISerializer& ser = m_dataSerializer; // Use interface since it fills in defaulted type info parameters
        return ser.Serialize(value, name);
    }

    bool DeltaSerializerApply::SerializeHelperImpl(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        ISerializer& ser = m_dataSerializer; // Use interface since it fills in defaulted type info parameters
        return ser.SerializeBytes(buffer, bufferCapacity, isString, outSize, name);
    }
}
