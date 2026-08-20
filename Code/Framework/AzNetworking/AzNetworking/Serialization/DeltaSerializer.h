/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzNetworking/Serialization/ISerializer.h>
#include <AzNetworking/Serialization/NetworkInputSerializer.h>
#include <AzNetworking/Serialization/NetworkOutputSerializer.h>
#include <AzNetworking/DataStructures/FixedSizeVectorBitset.h>
#include <AzNetworking/DataStructures/ByteBuffer.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace AzNetworking
{
    //! SerializerDelta
    //! Encodes information used by DeltaSerializer to create and apply serialization deltas
    class SerializerDelta
    {
    public:

        SerializerDelta();

        uint32_t GetNumDirtyBits() const;
        bool GetDirtyBit(uint32_t index) const;
        bool InsertDirtyBit(bool dirtyBit);

        uint8_t* GetBufferPtr();
        uint32_t GetBufferSize() const;
        uint32_t GetBufferCapacity() const;

        bool EnsureBufferCapacity(uint32_t size);
        bool SetBufferSize(uint32_t size);

        bool Serialize(ISerializer& serializer);

    private:

        FixedSizeVectorBitset<255> m_dirtyBits;
        static constexpr uint32_t InlineBufferCapacity = 1025;

        AZStd::array<uint8_t, InlineBufferCapacity> m_inlineDeltaBytes;
        AZStd::vector<uint8_t> m_overflowDeltaBytes;
        uint32_t m_deltaByteSize = 0;
    };

    //! A serializer that is used to produce a SerializerDelta between two objects.
    //! This delta can be reapplied to the same base object to reconstruct the second object using 
    //! the DeltaSerializerApply serializer
    //! NOTE: The objects serialized must have a consistent serialization footprint i.e. no changes in branches during serialization
    //! DeltaSerializerCreate instances are single-use.
    class DeltaSerializerCreate
        : public ISerializer
    {
    public:

        DeltaSerializerCreate(SerializerDelta& delta);
        ~DeltaSerializerCreate() override;

        template <typename TYPE>
        bool CreateDelta(TYPE& base, TYPE& current);

        // ISerializer interfaces
        SerializerMode GetSerializerMode() const override;
        bool Serialize(bool& value, const char* name) override;
        bool Serialize(int8_t& value, const char* name, int8_t minValue, int8_t maxValue) override;
        bool Serialize(int16_t& value, const char* name, int16_t minValue, int16_t maxValue) override;
        bool Serialize(int32_t& value, const char* name, int32_t minValue, int32_t maxValue) override;
        bool Serialize(long& value, const char* name, long minValue, long maxValue) override;
        bool Serialize(AZ::s64& value, const char* name, AZ::s64 minValue, AZ::s64 maxValue) override;
        bool Serialize(uint8_t& value, const char* name, uint8_t minValue, uint8_t maxValue) override;
        bool Serialize(uint16_t& value, const char* name, uint16_t minValue, uint16_t maxValue) override;
        bool Serialize(uint32_t& value, const char* name, uint32_t minValue, uint32_t maxValue) override;
        bool Serialize(unsigned long& value, const char* name, unsigned long minValue, unsigned long maxValue) override;
        bool Serialize(AZ::u64& value, const char* name, AZ::u64 minValue, AZ::u64 maxValue) override;
        bool Serialize(float& value, const char* name, float minValue, float maxValue) override;
        bool Serialize(double& value, const char* name, double minValue, double maxValue) override;
        bool SerializeBytes(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name) override;
        bool BeginObject(const char* name) override;
        bool EndObject(const char* name) override;

        const uint8_t* GetBuffer() const override;
        uint32_t GetCapacity() const override;
        uint32_t GetSize() const override;
        void ClearTrackedChangesFlag() override {}
        bool GetTrackedChangesFlag() const override { return false; }
        // ISerializer interfaces

    private:

        DeltaSerializerCreate(const DeltaSerializerCreate&) = delete;
        DeltaSerializerCreate& operator=(const DeltaSerializerCreate&) = delete;

        struct ValueRecord final
        {
            uint64_t m_value = 0;
        };

        static constexpr uint32_t MaxRecordCount = 255;
        static constexpr uint32_t InlineRecordByteCapacity = 1025;

        bool StoreRecordBytes(
            const uint8_t* buffer,
            uint32_t size,
            ValueRecord& record);

        [[nodiscard]]
        const uint8_t* GetRecordBytes() const;

        template <typename T>
        bool SerializeHelper(T& value, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name);

        template <typename T>
        bool SerializeHelperImpl(T& value, uint32_t, bool, uint32_t&, const char* name);
        bool SerializeHelperImpl(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name);

    private:

        SerializerDelta& m_delta;

        bool m_gatheringRecords = false;
        uint32_t m_objectCounter = 0;
        uint32_t m_recordByteSize = 0;
        AZStd::fixed_vector<ValueRecord, MaxRecordCount> m_records;
        AZStd::array<uint8_t, InlineRecordByteCapacity> m_inlineRecordBytes;
        AZStd::vector<uint8_t> m_overflowRecordBytes;
        uint32_t m_deltaByteOffset = 0;
        bool m_hasCreatedDelta = false;
    };

    //! A serializer that is used to apply a SerializerDelta to a base object in order to reconstruct the second object.
    //! NOTE: The objects serialized must have a consistent serialization footprint i.e. no changes in branches during serialization
    //! DeltaSerializerApply instances are single-use.
    class DeltaSerializerApply
        : public ISerializer
    {
    public:

        DeltaSerializerApply(SerializerDelta& delta);
        ~DeltaSerializerApply() override;

        template <typename TYPE>
        bool ApplyDelta(TYPE& output);

        // ISerializer interfaces
        SerializerMode GetSerializerMode() const override;
        bool Serialize(bool& value, const char* name) override;
        bool Serialize(int8_t& value, const char* name, int8_t minValue, int8_t maxValue) override;
        bool Serialize(int16_t& value, const char* name, int16_t minValue, int16_t maxValue) override;
        bool Serialize(int32_t& value, const char* name, int32_t minValue, int32_t maxValue) override;
        bool Serialize(long& value, const char* name, long minValue, long maxValue) override;
        bool Serialize(AZ::s64& value, const char* name, AZ::s64 minValue, AZ::s64 maxValue) override;
        bool Serialize(uint8_t& value, const char* name, uint8_t minValue, uint8_t maxValue) override;
        bool Serialize(uint16_t& value, const char* name, uint16_t minValue, uint16_t maxValue) override;
        bool Serialize(uint32_t& value, const char* name, uint32_t minValue, uint32_t maxValue) override;
        bool Serialize(unsigned long& value, const char* name, unsigned long minValue, unsigned long maxValue) override;
        bool Serialize(AZ::u64& value, const char* name, AZ::u64 minValue, AZ::u64 maxValue) override;
        bool Serialize(float& value, const char* name, float minValue, float maxValue) override;
        bool Serialize(double& value, const char* name, double minValue, double maxValue) override;
        bool SerializeBytes(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name) override;
        bool BeginObject(const char* name) override;
        bool EndObject(const char* name) override;

        const uint8_t* GetBuffer() const override;
        uint32_t GetCapacity() const override;
        uint32_t GetSize() const override;
        void ClearTrackedChangesFlag() override {}
        bool GetTrackedChangesFlag() const override { return false; }
        // ISerializer interfaces

    private:
 
        DeltaSerializerApply(const DeltaSerializerApply&) = delete;
        DeltaSerializerApply& operator=(const DeltaSerializerApply&) = delete;

        template <typename T>
        bool SerializeHelper(T& value, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name);

        template <typename T>
        bool SerializeHelperImpl(T& value, uint32_t, bool, uint32_t&, const char* name);
        bool SerializeHelperImpl(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name);

    private:

        SerializerDelta& m_delta;
        uint32_t m_nextDirtyBit = 0;
        NetworkOutputSerializer m_dataSerializer;
        bool m_hasAppliedDelta = false;
    };
}

#include <AzNetworking/Serialization/DeltaSerializer.inl>
