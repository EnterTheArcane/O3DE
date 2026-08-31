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

    void SerializerDelta::Reset()
    {
        m_dirtyBits.Clear();
        m_deltaBytes.Resize(0);
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

    DeltaSerializerCreate::~DeltaSerializerCreate() = default;

    SerializerMode DeltaSerializerCreate::GetSerializerMode() const
    {
        return SerializerMode::ReadFromObject;
    }

    bool DeltaSerializerCreate::Serialize(bool& value, const char* name)
    {
        return SerializeHelper(value, RecordKind::Boolean, WireWidth::OneByte, name);
    }

    bool DeltaSerializerCreate::Serialize(int8_t& value, const char* name, int8_t minValue, int8_t maxValue)
    {
        return SerializeHelper(value, RecordKind::Int8, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(int16_t& value, const char* name, int16_t minValue, int16_t maxValue)
    {
        return SerializeHelper(value, RecordKind::Int16, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(int32_t& value, const char* name, int32_t minValue, int32_t maxValue)
    {
        return SerializeHelper(value, RecordKind::Int32, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(long& value, const char* name, long minValue, long maxValue)
    {
        return SerializeHelper(value, RecordKind::Long, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(AZ::s64& value, const char* name, AZ::s64 minValue, AZ::s64 maxValue)
    {
        return SerializeHelper(value, RecordKind::Int64, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(uint8_t& value, const char* name, uint8_t minValue, uint8_t maxValue)
    {
        return SerializeHelper(value, RecordKind::Uint8, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(uint16_t& value, const char* name, uint16_t minValue, uint16_t maxValue)
    {
        return SerializeHelper(value, RecordKind::Uint16, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(uint32_t& value, const char* name, uint32_t minValue, uint32_t maxValue)
    {
        return SerializeHelper(value, RecordKind::Uint32, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(unsigned long& value, const char* name, unsigned long minValue, unsigned long maxValue)
    {
        return SerializeHelper(value, RecordKind::UnsignedLong, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(AZ::u64& value, const char* name, AZ::u64 minValue, AZ::u64 maxValue)
    {
        return SerializeHelper(value, RecordKind::Uint64, GetBoundedWireWidth(minValue, maxValue), name);
    }

    bool DeltaSerializerCreate::Serialize(
        float& value,
        const char* name,
        [[maybe_unused]] float minValue,
        [[maybe_unused]] float maxValue)
    {
        return SerializeHelper(value, RecordKind::Float, WireWidth::FourBytes, name);
    }

    bool DeltaSerializerCreate::Serialize(
        double& value,
        const char* name,
        [[maybe_unused]] double minValue,
        [[maybe_unused]] double maxValue)
    {
        return SerializeHelper(value, RecordKind::Double, WireWidth::EightBytes, name);
    }

    bool DeltaSerializerCreate::SerializeBytes(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        if (!IsValid() || outSize > bufferCapacity || (outSize > 0 && buffer == nullptr))
        {
            return FailCreate();
        }

        const uint32_t objectPosition = m_objectCounter;
        ++m_objectCounter;
        const uint8_t metadata = MakeRecordMetadata(
            RecordKind::Bytes,
            GetWireWidth(bufferCapacity),
            isString);

        if (m_gatheringRecords)
        {
            if (m_recordCount >= MaxRecordCount)
            {
                return FailCreate();
            }

            uint64_t recordPayload = 0;
            if (!StoreRecordBytes(buffer, outSize, recordPayload))
            {
                return FailCreate();
            }
            m_recordPayloads[m_recordCount] = recordPayload;
            m_recordMetadata[m_recordCount] = metadata;
            ++m_recordCount;
            return true;
        }

        if (objectPosition >= m_recordCount || m_recordMetadata[objectPosition] != metadata)
        {
            return FailCreate();
        }

        const uint64_t recordPayload = m_recordPayloads[objectPosition];
        const uint32_t baseOffset = static_cast<uint32_t>(recordPayload);
        const uint32_t baseSize = static_cast<uint32_t>(recordPayload >> 32);
        if (baseOffset > m_recordByteSize || baseSize > m_recordByteSize - baseOffset)
        {
            return FailCreate();
        }

        bool different = baseSize != outSize;
        if (!different && outSize > 0)
        {
            different = std::memcmp(GetRecordBytes() + baseOffset, buffer, outSize) != 0;
        }
        if (!m_delta.InsertDirtyBit(different))
        {
            return FailCreate();
        }

        if (different && !SerializeHelperImpl(buffer, bufferCapacity, isString, outSize, name))
        {
            return FailCreate();
        }
        return true;
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

    DeltaSerializerCreate::WireWidth DeltaSerializerCreate::GetWireWidth(uint64_t valueRange)
    {
        if (valueRange <= AZStd::numeric_limits<uint8_t>::max())
        {
            return WireWidth::OneByte;
        }
        if (valueRange <= AZStd::numeric_limits<uint16_t>::max())
        {
            return WireWidth::TwoBytes;
        }
        if (valueRange <= AZStd::numeric_limits<uint32_t>::max())
        {
            return WireWidth::FourBytes;
        }
        return WireWidth::EightBytes;
    }

    template <typename T>
    DeltaSerializerCreate::WireWidth DeltaSerializerCreate::GetBoundedWireWidth(T minValue, T maxValue)
    {
        const uint64_t valueRange = static_cast<uint64_t>(maxValue) - static_cast<uint64_t>(minValue);
        return GetWireWidth(valueRange);
    }

    uint8_t DeltaSerializerCreate::MakeRecordMetadata(RecordKind kind, WireWidth wireWidth, bool isString)
    {
        static_assert(static_cast<uint8_t>(RecordKind::Bytes) < 16);
        uint8_t metadata = static_cast<uint8_t>(kind);
        metadata |= static_cast<uint8_t>(static_cast<uint8_t>(wireWidth) << WireWidthShift);
        if (isString)
        {
            metadata |= StringFlag;
        }
        return metadata;
    }

    bool DeltaSerializerCreate::FailCreate()
    {
        Invalidate();
        m_delta.Reset();
        ClearRecordState();
        return false;
    }

    void DeltaSerializerCreate::ClearRecordState()
    {
        m_gatheringRecords = false;
        m_objectCounter = 0;
        m_recordCount = 0;
        m_recordByteSize = 0;
        m_overflowRecordBytes.clear();
    }

    bool DeltaSerializerCreate::StoreRecordBytes(const uint8_t* buffer, uint32_t size, uint64_t& recordPayload)
    {
        if (size > AZStd::numeric_limits<uint32_t>::max() - m_recordByteSize)
        {
            return false;
        }

        const uint32_t offset = m_recordByteSize;
        const uint32_t newSize = offset + size;
        recordPayload = (static_cast<uint64_t>(size) << 32) | offset;

        if (newSize <= m_inlineRecordBytes.size())
        {
            if (size > 0)
            {
                std::memcpy(m_inlineRecordBytes.data() + offset, buffer, size);
            }
        }
        else
        {
            if (newSize > m_overflowRecordBytes.max_size())
            {
                return false;
            }

            const bool firstOverflow = m_overflowRecordBytes.empty();
            m_overflowRecordBytes.resize_no_construct(newSize);
            if (firstOverflow && offset > 0)
            {
                std::memcpy(m_overflowRecordBytes.data(), m_inlineRecordBytes.data(), offset);
            }
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
        if (m_recordByteSize <= m_inlineRecordBytes.size())
        {
            return m_inlineRecordBytes.data();
        }
        return m_overflowRecordBytes.data();
    }

    template <typename T>
    bool DeltaSerializerCreate::SerializeHelper(T& value, RecordKind kind, WireWidth wireWidth, const char* name)
    {
        static_assert(AZStd::is_trivially_copyable_v<T>);
        static_assert(sizeof(T) <= sizeof(uint64_t));

        if (!IsValid())
        {
            return FailCreate();
        }

        const uint32_t objectPosition = m_objectCounter;
        ++m_objectCounter;
        const uint8_t metadata = MakeRecordMetadata(kind, wireWidth, false);

        if (m_gatheringRecords)
        {
            if (m_recordCount >= MaxRecordCount)
            {
                return FailCreate();
            }

            uint64_t recordPayload = 0;
            std::memcpy(&recordPayload, &value, sizeof(T));
            m_recordPayloads[m_recordCount] = recordPayload;
            m_recordMetadata[m_recordCount] = metadata;
            ++m_recordCount;
            return true;
        }

        if (objectPosition >= m_recordCount || m_recordMetadata[objectPosition] != metadata)
        {
            return FailCreate();
        }

        T baseValue{};
        std::memcpy(&baseValue, &m_recordPayloads[objectPosition], sizeof(T));
        const bool different = std::memcmp(&baseValue, &value, sizeof(T)) != 0;
        if (!m_delta.InsertDirtyBit(different))
        {
            return FailCreate();
        }

        if (different && !SerializeHelperImpl(value, name))
        {
            return FailCreate();
        }
        return true;
    }

    template <typename T>
    bool DeltaSerializerCreate::SerializeHelperImpl(T& value, const char* name)
    {
        ISerializer& serializer = m_dataSerializer;
        return serializer.Serialize(value, name);
    }

    bool DeltaSerializerCreate::SerializeHelperImpl(
        uint8_t* buffer,
        uint32_t bufferCapacity,
        bool isString,
        uint32_t& outSize,
        const char* name)
    {
        ISerializer& serializer = m_dataSerializer;
        return serializer.SerializeBytes(buffer, bufferCapacity, isString, outSize, name);
    }

    DeltaSerializerApply::DeltaSerializerApply(SerializerDelta& delta)
        : m_delta(delta)
        , m_dataSerializer(m_delta.GetBufferPtr(), m_delta.GetBufferSize())
    {
        ;
    }

    DeltaSerializerApply::~DeltaSerializerApply() = default;

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
        if (!SerializeHelperImpl(value, bufferCapacity, isString, outSize, name))
        {
            Invalidate();
            return false;
        }
        return true;
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
