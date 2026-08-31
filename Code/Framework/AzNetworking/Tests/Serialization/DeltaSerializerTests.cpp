/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/DeltaSerializer.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

#include <cmath>
#include <cstring>

namespace UnitTest
{
    struct DeltaDataElement
    {
        AzNetworking::PacketId m_packetId = AzNetworking::InvalidPacketId;
        bool m_isValid = true;
        char m_charKey = 'a';
        uint8_t m_bitfield = 0;
        uint16_t m_subId = 0;
        uint32_t m_id = 0;
        uint64_t m_sequence = 0;
        int8_t m_offset = 0;
        int16_t m_index = 0;
        AZ::TimeMs m_timeMs = AZ::Time::ZeroTimeMs;
        double m_frequency = 0;
        float m_blendFactor = 0.f;
        AZStd::vector<int> m_growVector, m_shrinkVector;
        AZStd::fixed_string<32> m_name = "DeltaElem";

        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            if (!serializer.Serialize(m_isValid, "Valid")
             || !serializer.Serialize(m_charKey, "CharKey")
             || !serializer.Serialize(m_packetId, "PacketId")
             || !serializer.Serialize(m_bitfield, "Bitfield")
             || !serializer.Serialize(m_subId, "SubId")
             || !serializer.Serialize(m_id, "Id")
             || !serializer.Serialize(m_sequence, "Sequence")
             || !serializer.Serialize(m_offset, "Offset")
             || !serializer.Serialize(m_index, "Index")
             || !serializer.Serialize(m_timeMs, "TimeMs")
             || !serializer.Serialize(m_frequency, "Frequency")
             || !serializer.Serialize(m_blendFactor, "BlendFactor")
             || !serializer.Serialize(m_growVector, "GrowVector")
             || !serializer.Serialize(m_shrinkVector, "ShrinkVector")
             || !serializer.Serialize(m_name, "Name"))
            {
                return false;
            }

            return true;
        }
    };

    struct DeltaDataContainer
    {
        AZStd::string m_containerName;
        AZStd::array<DeltaDataElement, 32> m_container;

        // This logic is modeled after NetworkInputArray serialization in the Multiplayer Gem
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            // Always serialize the full first element
            if(!m_container[0].Serialize(serializer))
            {
                return false;
            }

            for (uint32_t i = 1; i < m_container.size(); ++i)
            {
                if (serializer.GetSerializerMode() == AzNetworking::SerializerMode::WriteToObject)
                {
                    AzNetworking::SerializerDelta deltaSerializer;
                    // Read out the delta
                    if (!deltaSerializer.Serialize(serializer))
                    {
                        return false;
                    }

                    // Start with previous value
                    m_container[i] = m_container[i - 1];
                    // Then apply delta
                    AzNetworking::DeltaSerializerApply applySerializer(deltaSerializer);
                    if (!applySerializer.ApplyDelta(m_container[i]))
                    {
                        return false;
                    }
                }
                else
                {
                    AzNetworking::SerializerDelta deltaSerializer;
                    // Create the delta
                    AzNetworking::DeltaSerializerCreate createSerializer(deltaSerializer);
                    if (!createSerializer.CreateDelta(m_container[i - 1], m_container[i]))
                    {
                        return false;
                    }

                    // Then write out the delta
                    if (!deltaSerializer.Serialize(serializer))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        // This logic is modeled after NetworkInputArray serialization in the Multiplayer Gem
        bool SerializeNoDelta(AzNetworking::ISerializer& serializer)
        {
            for (uint32_t i = 0; i < m_container.size(); ++i)
            {
                if(!m_container[i].Serialize(serializer))
                {
                    return false;
                }
            }

            return true;
        }
    };

    struct SingleUint32Value final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_value, "Value", m_minValue, m_maxValue);
        }

        uint32_t m_value = 0;
        uint32_t m_minValue = AZStd::numeric_limits<uint32_t>::min();
        uint32_t m_maxValue = AZStd::numeric_limits<uint32_t>::max();
    };

    struct TwoFieldValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_first, "First")
                && serializer.Serialize(m_second, "Second");
        }

        uint32_t m_first = 0;
        uint32_t m_second = 0;
    };

    struct TwoByteValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_first, "First")
                && serializer.Serialize(m_second, "Second");
        }

        uint8_t m_first = 0;
        uint8_t m_second = 0;
    };

    struct FloatingPointValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_float, "Float")
                && serializer.Serialize(m_double, "Double");
        }

        float m_float = 0.0f;
        double m_double = 0.0;
    };

    struct BooleanVectorValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_values, "Values");
        }

        AZStd::vector<bool> m_values;
    };

    float FloatFromBits(const uint32_t bits)
    {
        float value{};
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    double DoubleFromBits(const uint64_t bits)
    {
        double value{};
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    struct ScalarKindsValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_boolean, "Boolean")
                && serializer.Serialize(m_int8, "Int8")
                && serializer.Serialize(m_int16, "Int16")
                && serializer.Serialize(m_int32, "Int32")
                && serializer.Serialize(m_long, "Long")
                && serializer.Serialize(m_int64, "Int64")
                && serializer.Serialize(m_uint8, "Uint8")
                && serializer.Serialize(m_uint16, "Uint16")
                && serializer.Serialize(m_uint32, "Uint32")
                && serializer.Serialize(m_unsignedLong, "UnsignedLong")
                && serializer.Serialize(m_uint64, "Uint64")
                && serializer.Serialize(m_float, "Float")
                && serializer.Serialize(m_double, "Double");
        }

        bool m_boolean = false;
        int8_t m_int8 = 0;
        int16_t m_int16 = 0;
        int32_t m_int32 = 0;
        long m_long = 0;
        AZ::s64 m_int64 = 0;
        uint8_t m_uint8 = 0;
        uint16_t m_uint16 = 0;
        uint32_t m_uint32 = 0;
        unsigned long m_unsignedLong = 0;
        AZ::u64 m_uint64 = 0;
        float m_float = 0.0f;
        double m_double = 0.0;
    };

    struct FloatValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(m_value, "Value");
        }

        float m_value = 0.0f;
    };

    struct BytePairValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            uint32_t firstSize = static_cast<uint32_t>(m_first.size());
            uint32_t secondSize = static_cast<uint32_t>(m_second.size());
            return serializer.SerializeBytes(
                       m_first.data(),
                       static_cast<uint32_t>(m_first.size()),
                       false,
                       firstSize,
                       "First")
                && serializer.SerializeBytes(
                       m_second.data(),
                       static_cast<uint32_t>(m_second.size()),
                       false,
                       secondSize,
                       "Second");
        }

        AZStd::array<uint8_t, 768> m_first{};
        AZStd::array<uint8_t, 768> m_second{};
    };

    struct SharedByteValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            uint32_t size = 4;
            const bool result = serializer.SerializeBytes(m_bytes, size, false, size, "Bytes");
            if (result && m_mutateAfterSerialize)
            {
                m_bytes[0] ^= 0xFF;
            }
            return result;
        }

        uint8_t* m_bytes = nullptr;
        bool m_mutateAfterSerialize = false;
    };

    struct RecordCountValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            for (uint32_t index = 0; index < m_count; ++index)
            {
                if (!serializer.Serialize(m_values[index], "Value"))
                {
                    return false;
                }
            }
            return true;
        }

        AZStd::array<uint8_t, 256> m_values{};
        uint32_t m_count = 0;
    };

    struct FailingValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            if (!serializer.Serialize(m_first, "First"))
            {
                return false;
            }
            if (m_failAfterFirst)
            {
                return false;
            }
            return serializer.Serialize(m_second, "Second");
        }

        uint32_t m_first = 0;
        uint32_t m_second = 0;
        bool m_failAfterFirst = false;
    };

    struct InvalidByteValue final
    {
        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            uint32_t size = 1;
            return serializer.SerializeBytes(nullptr, 1, false, size, "Invalid");
        }
    };

    struct VariantRecordValue final
    {
        enum class Mode
        {
            Uint8Width,
            Uint16Width,
            FloatKind,
            RawBytes,
            StringBytes,
        };

        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            switch (m_mode)
            {
            case Mode::Uint8Width:
                return serializer.Serialize(m_uint32, "Value", 0, 255);
            case Mode::Uint16Width:
                return serializer.Serialize(m_uint32, "Value", 0, 256);
            case Mode::FloatKind:
                return serializer.Serialize(m_float, "Value");
            case Mode::RawBytes:
            {
                uint32_t size = 1;
                return serializer.SerializeBytes(m_bytes.data(), 1, false, size, "Value");
            }
            case Mode::StringBytes:
            {
                uint32_t size = 1;
                return serializer.SerializeBytes(m_bytes.data(), 1, true, size, "Value");
            }
            }
            return false;
        }

        Mode m_mode = Mode::Uint8Width;
        uint32_t m_uint32 = 0;
        float m_float = 0.0f;
        AZStd::array<uint8_t, 1> m_bytes{};
    };

    class DeltaSerializerTests
        : public UnitTest::LeakDetectionFixture
    {
    };

    static constexpr float BLEND_FACTOR_SCALE = 1.1f;
    static constexpr uint32_t TIME_SCALE = 10;

    DeltaDataContainer TestDeltaContainer()
    {
        DeltaDataContainer testContainer;
        AZStd::vector<int> growVector, shrinkVector;
        growVector.resize(testContainer.m_container.size());
        shrinkVector.resize(testContainer.m_container.size());

        testContainer.m_containerName = "TestContainer";
        for (uint8_t i = 0; i < testContainer.m_container.size(); ++i)
        {
            testContainer.m_container[i].m_packetId = AzNetworking::PacketId(i);
            testContainer.m_container[i].m_id = i;
            testContainer.m_container[i].m_bitfield = i;
            testContainer.m_container[i].m_subId = i;
            testContainer.m_container[i].m_sequence = i;
            testContainer.m_container[i].m_charKey = i;
            testContainer.m_container[i].m_offset = i;
            testContainer.m_container[i].m_index = i;
            testContainer.m_container[i].m_frequency = i;
            testContainer.m_container[i].m_timeMs = AZ::TimeMs(i * TIME_SCALE);
            testContainer.m_container[i].m_blendFactor = BLEND_FACTOR_SCALE * i;
            growVector[i] = i;
            testContainer.m_container[i].m_growVector = growVector;
            shrinkVector[testContainer.m_container.size() - i - 1] = i;
            testContainer.m_container[i].m_shrinkVector = shrinkVector;
        }

        return testContainer;
    }

    AZStd::vector<uint8_t> EncodeDelta(AzNetworking::SerializerDelta& delta)
    {
        AZStd::array<uint8_t, 2048> buffer{};
        AzNetworking::NetworkInputSerializer serializer(buffer.data(), static_cast<uint32_t>(buffer.size()));
        EXPECT_TRUE(delta.Serialize(serializer));
        return AZStd::vector<uint8_t>(buffer.begin(), buffer.begin() + serializer.GetSize());
    }

    template <size_t Size>
    void SetDeltaPayload(AzNetworking::SerializerDelta& delta, const AZStd::array<uint8_t, Size>& payload)
    {
        delta.SetBufferSize(Size);
        ASSERT_EQ(delta.GetBufferSize(), Size);
        std::memcpy(delta.GetBufferPtr(), payload.data(), Size);
    }

    TEST_F(DeltaSerializerTests, ScalarKindsRoundTripWithoutPerRecordAllocation)
    {
        ScalarKindsValue base;
        ScalarKindsValue current;
        current.m_boolean = true;
        current.m_int8 = -1;
        current.m_int16 = -2;
        current.m_int32 = -3;
        current.m_long = -4;
        current.m_int64 = -5;
        current.m_uint8 = 1;
        current.m_uint16 = 2;
        current.m_uint32 = 3;
        current.m_unsignedLong = 4;
        current.m_uint64 = 5;
        current.m_float = 6.5f;
        current.m_double = 7.5;

        AzNetworking::SerializerDelta delta;
        AzNetworking::DeltaSerializerCreate createSerializer(delta);
        ASSERT_TRUE(createSerializer.CreateDelta(base, current));
        ASSERT_EQ(delta.GetNumDirtyBits(), 13);
        for (uint32_t index = 0; index < delta.GetNumDirtyBits(); ++index)
        {
            EXPECT_TRUE(delta.GetDirtyBit(index));
        }

        ScalarKindsValue output = base;
        AzNetworking::DeltaSerializerApply applySerializer(delta);
        ASSERT_TRUE(applySerializer.ApplyDelta(output));
        EXPECT_EQ(output.m_boolean, current.m_boolean);
        EXPECT_EQ(output.m_int8, current.m_int8);
        EXPECT_EQ(output.m_int16, current.m_int16);
        EXPECT_EQ(output.m_int32, current.m_int32);
        EXPECT_EQ(output.m_long, current.m_long);
        EXPECT_EQ(output.m_int64, current.m_int64);
        EXPECT_EQ(output.m_uint8, current.m_uint8);
        EXPECT_EQ(output.m_uint16, current.m_uint16);
        EXPECT_EQ(output.m_uint32, current.m_uint32);
        EXPECT_EQ(output.m_unsignedLong, current.m_unsignedLong);
        EXPECT_EQ(output.m_uint64, current.m_uint64);
        EXPECT_EQ(output.m_float, current.m_float);
        EXPECT_EQ(output.m_double, current.m_double);
    }

    TEST_F(DeltaSerializerTests, CreateRejectsRecordKindWidthAndStringModeMismatches)
    {
        const AZStd::array<AZStd::pair<VariantRecordValue::Mode, VariantRecordValue::Mode>, 3> mismatches = {
            AZStd::pair{VariantRecordValue::Mode::Uint8Width, VariantRecordValue::Mode::Uint16Width},
            AZStd::pair{VariantRecordValue::Mode::Uint8Width, VariantRecordValue::Mode::FloatKind},
            AZStd::pair{VariantRecordValue::Mode::RawBytes, VariantRecordValue::Mode::StringBytes},
        };

        for (const auto& mismatch : mismatches)
        {
            VariantRecordValue base;
            base.m_mode = mismatch.first;
            VariantRecordValue current;
            current.m_mode = mismatch.second;
            AzNetworking::SerializerDelta delta;
            AzNetworking::DeltaSerializerCreate createSerializer(delta);

            EXPECT_FALSE(createSerializer.CreateDelta(base, current));
            EXPECT_EQ(delta.GetNumDirtyBits(), 0);
            EXPECT_EQ(delta.GetBufferSize(), 0);
        }
    }

    TEST_F(DeltaSerializerTests, RecordLimitAccepts255FieldsAndRejects256WithoutResidue)
    {
        RecordCountValue base;
        base.m_count = 255;
        RecordCountValue current;
        current.m_count = 255;
        for (uint32_t index = 0; index < current.m_count; ++index)
        {
            current.m_values[index] = static_cast<uint8_t>(index + 1);
        }

        AzNetworking::SerializerDelta maximumDelta;
        AzNetworking::DeltaSerializerCreate maximumSerializer(maximumDelta);
        ASSERT_TRUE(maximumSerializer.CreateDelta(base, current));
        EXPECT_EQ(maximumDelta.GetNumDirtyBits(), 255);

        base.m_count = 256;
        current.m_count = 256;
        AzNetworking::SerializerDelta oversizedDelta;
        AzNetworking::DeltaSerializerCreate oversizedSerializer(oversizedDelta);
        EXPECT_FALSE(oversizedSerializer.CreateDelta(base, current));
        EXPECT_EQ(oversizedDelta.GetNumDirtyBits(), 0);
        EXPECT_EQ(oversizedDelta.GetBufferSize(), 0);
    }

    TEST_F(DeltaSerializerTests, BaseByteSnapshotSurvivesOverflow)
    {
        BytePairValue overflowBase;
        BytePairValue overflowCurrent = overflowBase;
        overflowCurrent.m_second.front() = 1;
        AzNetworking::SerializerDelta overflowDelta;
        AzNetworking::DeltaSerializerCreate overflowSerializer(overflowDelta);
        ASSERT_TRUE(overflowSerializer.CreateDelta(overflowBase, overflowCurrent));
        EXPECT_FALSE(overflowDelta.GetDirtyBit(0));
        EXPECT_TRUE(overflowDelta.GetDirtyBit(1));

        BytePairValue overflowOutput = overflowBase;
        AzNetworking::DeltaSerializerApply overflowApply(overflowDelta);
        ASSERT_TRUE(overflowApply.ApplyDelta(overflowOutput));
        EXPECT_EQ(overflowOutput.m_second, overflowCurrent.m_second);
    }

    TEST_F(DeltaSerializerTests, BaseByteSnapshotSurvivesAliasedSourceMutation)
    {
        AZStd::array<uint8_t, 4> sharedBytes{1, 2, 3, 4};
        SharedByteValue aliasBase{sharedBytes.data(), true};
        SharedByteValue aliasCurrent{sharedBytes.data(), false};
        AzNetworking::SerializerDelta aliasDelta;
        AzNetworking::DeltaSerializerCreate aliasSerializer(aliasDelta);
        ASSERT_TRUE(aliasSerializer.CreateDelta(aliasBase, aliasCurrent));
        ASSERT_TRUE(aliasDelta.GetDirtyBit(0));

        AZStd::array<uint8_t, 4> outputBytes{1, 2, 3, 4};
        SharedByteValue aliasOutput{outputBytes.data(), false};
        AzNetworking::DeltaSerializerApply aliasApply(aliasDelta);
        ASSERT_TRUE(aliasApply.ApplyDelta(aliasOutput));
        EXPECT_EQ(outputBytes, sharedBytes);
    }

    TEST_F(DeltaSerializerTests, CreateAndApplyAreSingleUseAndFailuresClearCreatedDelta)
    {
        TwoFieldValue base;
        TwoFieldValue current{1, 2};
        AzNetworking::SerializerDelta delta;
        AzNetworking::DeltaSerializerCreate createSerializer(delta);
        ASSERT_TRUE(createSerializer.CreateDelta(base, current));
        EXPECT_FALSE(createSerializer.CreateDelta(base, current));
        EXPECT_EQ(delta.GetNumDirtyBits(), 0);
        EXPECT_EQ(delta.GetBufferSize(), 0);

        FailingValue failingBase;
        FailingValue failingCurrent;
        failingCurrent.m_first = 1;
        failingCurrent.m_failAfterFirst = true;
        AzNetworking::SerializerDelta failingDelta;
        AzNetworking::DeltaSerializerCreate failingSerializer(failingDelta);
        EXPECT_FALSE(failingSerializer.CreateDelta(failingBase, failingCurrent));
        EXPECT_EQ(failingDelta.GetNumDirtyBits(), 0);
        EXPECT_EQ(failingDelta.GetBufferSize(), 0);

        InvalidByteValue invalidBase;
        InvalidByteValue invalidCurrent;
        AzNetworking::SerializerDelta invalidDelta;
        AzNetworking::DeltaSerializerCreate invalidSerializer(invalidDelta);
        EXPECT_FALSE(invalidSerializer.CreateDelta(invalidBase, invalidCurrent));
        EXPECT_EQ(invalidDelta.GetNumDirtyBits(), 0);
        EXPECT_EQ(invalidDelta.GetBufferSize(), 0);

        AzNetworking::SerializerDelta applyDelta;
        AzNetworking::DeltaSerializerCreate applyCreate(applyDelta);
        ASSERT_TRUE(applyCreate.CreateDelta(base, current));
        TwoFieldValue output = base;
        AzNetworking::DeltaSerializerApply applySerializer(applyDelta);
        ASSERT_TRUE(applySerializer.ApplyDelta(output));
        EXPECT_FALSE(applySerializer.ApplyDelta(output));
    }

    TEST_F(DeltaSerializerTests, ApplyRejectsRecordAndPayloadUnderOrOverConsumption)
    {
        TwoByteValue base;
        TwoByteValue current{1, 2};

        AzNetworking::SerializerDelta trailingBitDelta;
        AzNetworking::DeltaSerializerCreate trailingBitCreate(trailingBitDelta);
        ASSERT_TRUE(trailingBitCreate.CreateDelta(base, current));
        ASSERT_TRUE(trailingBitDelta.InsertDirtyBit(false));
        TwoByteValue trailingBitOutput = base;
        AzNetworking::DeltaSerializerApply trailingBitApply(trailingBitDelta);
        EXPECT_FALSE(trailingBitApply.ApplyDelta(trailingBitOutput));

        AzNetworking::SerializerDelta trailingPayloadDelta;
        AzNetworking::DeltaSerializerCreate trailingPayloadCreate(trailingPayloadDelta);
        ASSERT_TRUE(trailingPayloadCreate.CreateDelta(base, current));
        const uint32_t payloadSize = trailingPayloadDelta.GetBufferSize();
        ASSERT_LT(payloadSize, trailingPayloadDelta.GetBufferCapacity());
        trailingPayloadDelta.GetBufferPtr()[payloadSize] = 0;
        trailingPayloadDelta.SetBufferSize(payloadSize + 1);
        TwoByteValue trailingPayloadOutput = base;
        AzNetworking::DeltaSerializerApply trailingPayloadApply(trailingPayloadDelta);
        EXPECT_FALSE(trailingPayloadApply.ApplyDelta(trailingPayloadOutput));

        AzNetworking::SerializerDelta shortPayloadDelta;
        ASSERT_TRUE(shortPayloadDelta.InsertDirtyBit(true));
        const AZStd::array<uint8_t, 1> shortPayload{0};
        SetDeltaPayload(shortPayloadDelta, shortPayload);
        TwoFieldValue shortPayloadOutput;
        AzNetworking::DeltaSerializerApply shortPayloadApply(shortPayloadDelta);
        EXPECT_FALSE(shortPayloadApply.ApplyDelta(shortPayloadOutput));

        EXPECT_FALSE(EncodeDelta(trailingPayloadDelta).empty());
    }

    TEST_F(DeltaSerializerTests, DeltaArray)
    {
        DeltaDataContainer inContainer = TestDeltaContainer();
        AZStd::array<uint8_t, 4096> buffer;
        AzNetworking::NetworkInputSerializer inSerializer(buffer.data(), static_cast<uint32_t>(buffer.size()));

        // Always serialize the full first element
        EXPECT_TRUE(inContainer.Serialize(inSerializer));

        DeltaDataContainer outContainer;
        AzNetworking::NetworkOutputSerializer outSerializer(buffer.data(), static_cast<uint32_t>(buffer.size()));

        EXPECT_TRUE(outContainer.Serialize(outSerializer));

        for (uint32_t i = 0; i < outContainer.m_container.size(); ++i)
        {
            EXPECT_EQ(inContainer.m_container[i].m_isValid, outContainer.m_container[i].m_isValid);
            EXPECT_EQ(inContainer.m_container[i].m_blendFactor, outContainer.m_container[i].m_blendFactor);
            EXPECT_EQ(inContainer.m_container[i].m_id, outContainer.m_container[i].m_id);
            EXPECT_EQ(inContainer.m_container[i].m_subId, outContainer.m_container[i].m_subId);
            EXPECT_EQ(inContainer.m_container[i].m_sequence, outContainer.m_container[i].m_sequence);
            EXPECT_EQ(inContainer.m_container[i].m_offset, outContainer.m_container[i].m_offset);
            EXPECT_EQ(inContainer.m_container[i].m_bitfield, outContainer.m_container[i].m_bitfield);
            EXPECT_EQ(inContainer.m_container[i].m_charKey, outContainer.m_container[i].m_charKey);
            EXPECT_EQ(inContainer.m_container[i].m_index, outContainer.m_container[i].m_index);
            EXPECT_EQ(inContainer.m_container[i].m_frequency, outContainer.m_container[i].m_frequency);
            EXPECT_EQ(inContainer.m_container[i].m_packetId, outContainer.m_container[i].m_packetId);
            EXPECT_EQ(inContainer.m_container[i].m_timeMs, outContainer.m_container[i].m_timeMs);
            EXPECT_EQ(inContainer.m_container[i].m_growVector, outContainer.m_container[i].m_growVector);
            EXPECT_EQ(inContainer.m_container[i].m_shrinkVector, outContainer.m_container[i].m_shrinkVector);
            EXPECT_EQ(inContainer.m_container[i].m_name, outContainer.m_container[i].m_name);
        }
    }

    TEST_F(DeltaSerializerTests, BooleanVectorRoundTripsThroughGenericContainerSerializer)
    {
        AZStd::vector<bool> input{true, false, true, true, false};
        AZStd::array<uint8_t, 64> buffer{};
        AzNetworking::NetworkInputSerializer inputSerializer(buffer.data(), aznumeric_cast<uint32_t>(buffer.size()));
        AzNetworking::ISerializer& inputSerializerInterface = inputSerializer;
        ASSERT_TRUE(inputSerializerInterface.Serialize(input, "Values"));

        AZStd::vector<bool> output(input.size(), false);
        AzNetworking::NetworkOutputSerializer outputSerializer(buffer.data(), inputSerializer.GetSize());
        AzNetworking::ISerializer& outputSerializerInterface = outputSerializer;
        ASSERT_TRUE(outputSerializerInterface.Serialize(output, "Values"));
        EXPECT_EQ(output, input);
    }

    TEST_F(DeltaSerializerTests, FloatingPointPayloadsRoundTripBitExactly)
    {
        FloatingPointValue positiveZero;
        FloatingPointValue negativeZero{
            FloatFromBits(0x80000000u),
            DoubleFromBits(0x8000000000000000ull),
        };

        AzNetworking::SerializerDelta zeroDelta;
        AzNetworking::DeltaSerializerCreate zeroCreate(zeroDelta);
        ASSERT_TRUE(zeroCreate.CreateDelta(positiveZero, negativeZero));
        ASSERT_EQ(zeroDelta.GetNumDirtyBits(), 2);
        EXPECT_TRUE(zeroDelta.GetDirtyBit(0));
        EXPECT_TRUE(zeroDelta.GetDirtyBit(1));

        FloatingPointValue zeroOutput = positiveZero;
        AzNetworking::DeltaSerializerApply zeroApply(zeroDelta);
        ASSERT_TRUE(zeroApply.ApplyDelta(zeroOutput));
        EXPECT_EQ(std::memcmp(&zeroOutput.m_float, &negativeZero.m_float, sizeof(float)), 0);
        EXPECT_EQ(std::memcmp(&zeroOutput.m_double, &negativeZero.m_double, sizeof(double)), 0);

        FloatingPointValue nanValue{
            FloatFromBits(0x7FC12345u),
            DoubleFromBits(0x7FF8123456789ABCull),
        };
        FloatingPointValue sameNanValue = nanValue;
        AzNetworking::SerializerDelta cleanNanDelta;
        AzNetworking::DeltaSerializerCreate cleanNanCreate(cleanNanDelta);
        ASSERT_TRUE(cleanNanCreate.CreateDelta(nanValue, sameNanValue));
        ASSERT_EQ(cleanNanDelta.GetNumDirtyBits(), 2);
        EXPECT_FALSE(cleanNanDelta.GetDirtyBit(0));
        EXPECT_FALSE(cleanNanDelta.GetDirtyBit(1));

        FloatingPointValue differentNanValue{
            FloatFromBits(0x7FC54321u),
            DoubleFromBits(0x7FF8ABCDEF012345ull),
        };
        AzNetworking::SerializerDelta dirtyNanDelta;
        AzNetworking::DeltaSerializerCreate dirtyNanCreate(dirtyNanDelta);
        ASSERT_TRUE(dirtyNanCreate.CreateDelta(nanValue, differentNanValue));
        EXPECT_TRUE(dirtyNanDelta.GetDirtyBit(0));
        EXPECT_TRUE(dirtyNanDelta.GetDirtyBit(1));

        FloatingPointValue nanOutput = nanValue;
        AzNetworking::DeltaSerializerApply dirtyNanApply(dirtyNanDelta);
        ASSERT_TRUE(dirtyNanApply.ApplyDelta(nanOutput));
        EXPECT_EQ(std::memcmp(&nanOutput.m_float, &differentNanValue.m_float, sizeof(float)), 0);
        EXPECT_EQ(std::memcmp(&nanOutput.m_double, &differentNanValue.m_double, sizeof(double)), 0);
    }

    TEST_F(DeltaSerializerTests, BooleanVectorDeltaPreservesCleanAndDirtyElements)
    {
        BooleanVectorValue base{{true, false, true, false}};
        BooleanVectorValue current{{true, true, true, false}};
        AzNetworking::SerializerDelta delta;
        AzNetworking::DeltaSerializerCreate create(delta);
        ASSERT_TRUE(create.CreateDelta(base, current));

        BooleanVectorValue output = base;
        AzNetworking::DeltaSerializerApply apply(delta);
        ASSERT_TRUE(apply.ApplyDelta(output));
        EXPECT_EQ(output.m_values, current.m_values);
    }

    TEST_F(DeltaSerializerTests, DeltaSerializerCreateUnused)
    {
        // Every function here should return a constant value regardless of inputs
        AzNetworking::SerializerDelta deltaSerializer;
        AzNetworking::DeltaSerializerCreate createSerializer(deltaSerializer);

        EXPECT_EQ(createSerializer.GetCapacity(), 0);
        EXPECT_EQ(createSerializer.GetSize(), 0);
        EXPECT_EQ(createSerializer.GetBuffer(), nullptr);
        EXPECT_EQ(createSerializer.GetSerializerMode(), AzNetworking::SerializerMode::ReadFromObject);

        createSerializer.ClearTrackedChangesFlag(); //NO-OP
        EXPECT_FALSE(createSerializer.GetTrackedChangesFlag());
        EXPECT_TRUE(createSerializer.BeginObject("CreateSerializer"));
        EXPECT_TRUE(createSerializer.EndObject("CreateSerializer"));
    }

    TEST_F(DeltaSerializerTests, DeltaArraySize)
    {
        DeltaDataContainer deltaContainer = TestDeltaContainer();
        DeltaDataContainer noDeltaContainer = TestDeltaContainer();

        AZStd::array<uint8_t, 4096> deltaBuffer;
        AzNetworking::NetworkInputSerializer deltaSerializer(deltaBuffer.data(), static_cast<uint32_t>(deltaBuffer.size()));
        AZStd::array<uint8_t, 4096> noDeltaBuffer;
        AzNetworking::NetworkInputSerializer noDeltaSerializer(noDeltaBuffer.data(), static_cast<uint32_t>(noDeltaBuffer.size()));

        EXPECT_TRUE(deltaContainer.Serialize(deltaSerializer));
        EXPECT_FALSE(noDeltaContainer.SerializeNoDelta(noDeltaSerializer)); // Should run out of space
        EXPECT_EQ(noDeltaSerializer.GetCapacity(), noDeltaSerializer.GetSize()); // Verify that the serializer filled up
        EXPECT_FALSE(noDeltaSerializer.IsValid()); // and that it is no longer valid due to lack of space
    }

    TEST_F(DeltaSerializerTests, DeltaSerializerApplyUnused)
    {
        // Every function here should return a constant value regardless of inputs
        AzNetworking::SerializerDelta deltaSerializer;
        AzNetworking::DeltaSerializerApply applySerializer(deltaSerializer);

        EXPECT_EQ(applySerializer.GetCapacity(), 0);
        EXPECT_EQ(applySerializer.GetSize(), 0);
        EXPECT_EQ(applySerializer.GetBuffer(), nullptr);
        EXPECT_EQ(applySerializer.GetSerializerMode(), AzNetworking::SerializerMode::WriteToObject);

        applySerializer.ClearTrackedChangesFlag(); //NO-OP
        EXPECT_FALSE(applySerializer.GetTrackedChangesFlag());
        EXPECT_TRUE(applySerializer.BeginObject("CreateSerializer"));
        EXPECT_TRUE(applySerializer.EndObject("CreateSerializer"));
    }
}
