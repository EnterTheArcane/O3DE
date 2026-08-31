/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/NetworkInputSerializer.h>
#include <AzNetworking/Serialization/NetworkOutputSerializer.h>
#include <AzNetworking/Serialization/TrackChangedSerializer.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/fixed_string.h>
#include <AzCore/std/string/string.h>

#include <cstring>

namespace UnitTest
{
    namespace
    {
        AZStd::vector<uint8_t> EncodeByteString(AZStd::vector<uint8_t>& value)
        {
            AZStd::vector<uint8_t> encoded(value.size() + sizeof(uint32_t));
            AzNetworking::NetworkInputSerializer serializer(encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
            uint32_t valueSize = aznumeric_cast<uint32_t>(value.size());
            EXPECT_TRUE(serializer.SerializeBytes(value.data(), valueSize, true, valueSize, "Value"));
            encoded.resize(serializer.GetSize());
            return encoded;
        }

        template <typename Value>
        AZStd::vector<uint8_t> EncodeValue(Value value)
        {
            AZStd::array<uint8_t, 256> buffer{};
            AzNetworking::NetworkInputSerializer serializer(buffer.data(), aznumeric_cast<uint32_t>(buffer.size()));
            AzNetworking::ISerializer& serializerInterface = serializer;
            EXPECT_TRUE(serializerInterface.Serialize(value, "Value"));
            return AZStd::vector<uint8_t>(buffer.begin(), buffer.begin() + serializer.GetSize());
        }

        template <typename Value>
        bool SerializeValue(AzNetworking::ISerializer& serializer, Value& value)
        {
            return serializer.Serialize(value, "Value");
        }

        template <typename Value, typename Bits>
        Value ValueFromBits(const Bits bits)
        {
            static_assert(sizeof(Value) == sizeof(Bits));
            Value value{};
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        template <typename Value>
        bool HasIdenticalRepresentation(const Value& lhs, const Value& rhs)
        {
            return std::memcmp(&lhs, &rhs, sizeof(Value)) == 0;
        }

        template <typename Value, typename Bits>
        void ExpectFloatingPointRepresentationTracking(const Bits negativeZeroBits, const Bits nanBits, const Bits otherNanBits)
        {
            const Value negativeZero = ValueFromBits<Value>(negativeZeroBits);
            const AZStd::vector<uint8_t> negativeZeroEncoding = EncodeValue(negativeZero);
            Value positiveZero{};
            AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> signedZeroSerializer(
                negativeZeroEncoding.data(), aznumeric_cast<uint32_t>(negativeZeroEncoding.size()));
            ASSERT_TRUE(SerializeValue(signedZeroSerializer, positiveZero));
            EXPECT_TRUE(signedZeroSerializer.GetTrackedChangesFlag());
            EXPECT_TRUE(HasIdenticalRepresentation(positiveZero, negativeZero));

            const Value nan = ValueFromBits<Value>(nanBits);
            const AZStd::vector<uint8_t> nanEncoding = EncodeValue(nan);
            Value identicalNan = nan;
            AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> identicalNanSerializer(
                nanEncoding.data(), aznumeric_cast<uint32_t>(nanEncoding.size()));
            ASSERT_TRUE(SerializeValue(identicalNanSerializer, identicalNan));
            EXPECT_FALSE(identicalNanSerializer.GetTrackedChangesFlag());
            EXPECT_TRUE(HasIdenticalRepresentation(identicalNan, nan));

            Value otherNan = ValueFromBits<Value>(otherNanBits);
            AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> otherNanSerializer(
                nanEncoding.data(), aznumeric_cast<uint32_t>(nanEncoding.size()));
            ASSERT_TRUE(SerializeValue(otherNanSerializer, otherNan));
            EXPECT_TRUE(otherNanSerializer.GetTrackedChangesFlag());
            EXPECT_TRUE(HasIdenticalRepresentation(otherNan, nan));
        }
    } // namespace

    struct TrackChangedSerializerInElement
    {
        bool testBool = false;
        char testChar = 'a';
        int8_t testInt8 = 0;
        int16_t testInt16 = 0;
        int32_t testInt32 = 0;
        int64_t testInt64 = 0;
        uint8_t testUint8 = 0;
        uint16_t testUint16 = 0;
        uint32_t testUint32 = 0;
        uint64_t testUint64 = 0;
        double testDouble = 0.0;
        float testFloat = 0.f;
        AZStd::fixed_string<32> testFixedString = "";

        bool Serialize(AzNetworking::ISerializer& serializer)
        {
            if (!serializer.Serialize(testBool, "TestBool") || !serializer.Serialize(testChar, "TestChar") ||
                !serializer.Serialize(testInt8, "TestInt8") || !serializer.Serialize(testInt16, "TestInt16") ||
                !serializer.Serialize(testInt32, "TestInt32") || !serializer.Serialize(testInt64, "TestInt64") ||
                !serializer.Serialize(testUint8, "TestUint8") || !serializer.Serialize(testUint16, "TestUint16") ||
                !serializer.Serialize(testUint32, "TestUint32") || !serializer.Serialize(testUint64, "TestUint64") ||
                !serializer.Serialize(testDouble, "TestDouble") || !serializer.Serialize(testFloat, "TestFloat") ||
                !serializer.Serialize(testFixedString, "TestFixedString"))
            {
                return false;
            }

            return true;
        }
    };

    struct TrackChangedSerializerOutElement
    {
        bool testBool = true;
        char testChar = 'b';
        int8_t testInt8 = 1;
        int16_t testInt16 = 1;
        int32_t testInt32 = 1;
        int64_t testInt64 = 1;
        uint8_t testUint8 = 1;
        uint16_t testUint16 = 1;
        uint32_t testUint32 = 1;
        uint64_t testUint64 = 1;
        double testDouble = 1.0;
        float testFloat = 1.f;
        AZStd::fixed_string<32> testFixedString = "TestFixedString";

        bool SerializeFixedString(AzNetworking::ISerializer& serializer)
        {
            return serializer.Serialize(testFixedString, "TestFixedString");
        }
    };

    class TrackChangedSerializerTests : public LeakDetectionFixture
    {
    };

    TEST_F(TrackChangedSerializerTests, TestTrackChangedSerializer)
    {
        const size_t Capacity = 2048;
        constexpr uint32_t ExpectedSerializedBytes = 46;
        AZStd::array<uint8_t, Capacity> buffer;

        TrackChangedSerializerInElement inElement;
        AzNetworking::NetworkInputSerializer inSerializer(buffer.data(), static_cast<uint32_t>(buffer.size()));

        EXPECT_TRUE(inElement.Serialize(inSerializer));

        TrackChangedSerializerOutElement outElement;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> trackChangedSerializer(buffer.data(), static_cast<uint32_t>(buffer.size()));

        EXPECT_EQ(trackChangedSerializer.GetSerializerMode(), AzNetworking::SerializerMode::WriteToObject);

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testBool, "TestBool");
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testChar, "TestChar");
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testInt8, "TestInt8", AZStd::numeric_limits<int8_t>::min(), AZStd::numeric_limits<int8_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testInt16, "TestInt16", AZStd::numeric_limits<int16_t>::min(), AZStd::numeric_limits<int16_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testInt32, "TestInt32", AZStd::numeric_limits<int32_t>::min(), AZStd::numeric_limits<int32_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testInt64, "TestInt64", AZStd::numeric_limits<int64_t>::min(), AZStd::numeric_limits<int64_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testUint8, "TestUint8", AZStd::numeric_limits<uint8_t>::min(), AZStd::numeric_limits<uint8_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testUint16, "TestUint16", AZStd::numeric_limits<uint16_t>::min(), AZStd::numeric_limits<uint16_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testUint32, "TestUint32", AZStd::numeric_limits<uint32_t>::min(), AZStd::numeric_limits<uint32_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testUint64, "TestUint64", AZStd::numeric_limits<uint64_t>::min(), AZStd::numeric_limits<uint64_t>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testDouble, "TestDouble", AZStd::numeric_limits<double>::min(), AZStd::numeric_limits<double>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        trackChangedSerializer.Serialize(outElement.testFloat, "TestFloat", AZStd::numeric_limits<float>::min(), AZStd::numeric_limits<float>::max());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        trackChangedSerializer.ClearTrackedChangesFlag();
        EXPECT_TRUE(outElement.SerializeFixedString(trackChangedSerializer));
        EXPECT_TRUE(trackChangedSerializer.IsValid());
        EXPECT_TRUE(trackChangedSerializer.GetTrackedChangesFlag());

        EXPECT_NE(trackChangedSerializer.GetBuffer(), nullptr);
        EXPECT_EQ(trackChangedSerializer.GetCapacity(), Capacity);
        EXPECT_EQ(trackChangedSerializer.GetSize(), ExpectedSerializedBytes);
    }

    TEST_F(TrackChangedSerializerTests, FloatingPointChangesUseExactRepresentation)
    {
        ExpectFloatingPointRepresentationTracking<float>(0x80000000U, 0x7FC00001U, 0x7FC00002U);
        ExpectFloatingPointRepresentationTracking<double>(
            0x8000000000000000ULL, 0x7FF8000000000001ULL, 0x7FF8000000000002ULL);
    }

    TEST_F(TrackChangedSerializerTests, DynamicStringsTrackEqualGrowthAndShrinkage)
    {
        const AZStd::string source{"Serialized string"};
        const AZStd::vector<uint8_t> encoded = EncodeValue(source);

        AZStd::string equal = source;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> equalSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(SerializeValue(equalSerializer, equal));
        EXPECT_TRUE(equalSerializer.IsValid());
        EXPECT_FALSE(equalSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(equal, source);

        AZStd::string grown{"Short"};
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> grownSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(SerializeValue(grownSerializer, grown));
        EXPECT_TRUE(grownSerializer.IsValid());
        EXPECT_TRUE(grownSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(grown, source);

        AZStd::string shrunk{"A destination value longer than the serialized string"};
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> shrunkSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(SerializeValue(shrunkSerializer, shrunk));
        EXPECT_TRUE(shrunkSerializer.IsValid());
        EXPECT_TRUE(shrunkSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(shrunk, source);
    }

    TEST_F(TrackChangedSerializerTests, FixedStringsTrackEqualGrowthAndShrinkage)
    {
        const AZStd::fixed_string<64> source{"Serialized fixed string"};
        const AZStd::vector<uint8_t> encoded = EncodeValue(source);

        AZStd::fixed_string<64> equal = source;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> equalSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(SerializeValue(equalSerializer, equal));
        EXPECT_TRUE(equalSerializer.IsValid());
        EXPECT_FALSE(equalSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(equal, source);

        AZStd::fixed_string<64> grown{"Short"};
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> grownSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(SerializeValue(grownSerializer, grown));
        EXPECT_TRUE(grownSerializer.IsValid());
        EXPECT_TRUE(grownSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(grown, source);

        AZStd::fixed_string<64> shrunk{"A destination value longer than the serialized fixed string"};
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> shrunkSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(SerializeValue(shrunkSerializer, shrunk));
        EXPECT_TRUE(shrunkSerializer.IsValid());
        EXPECT_TRUE(shrunkSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(shrunk, source);
    }

    TEST_F(TrackChangedSerializerTests, FailedScalarSerializationStillTracksMutation)
    {
        AZStd::array<uint8_t, 8> buffer{};
        uint8_t source = 2;
        AzNetworking::NetworkInputSerializer inputSerializer(buffer.data(), aznumeric_cast<uint32_t>(buffer.size()));
        ASSERT_TRUE(inputSerializer.Serialize(source, "Value", uint8_t{0}, uint8_t{255}));

        uint8_t destination = 7;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> serializer(
            buffer.data(), inputSerializer.GetSize());
        EXPECT_FALSE(serializer.Serialize(destination, "Value", uint8_t{0}, uint8_t{1}));
        EXPECT_EQ(destination, source);
        EXPECT_TRUE(serializer.GetTrackedChangesFlag());
        EXPECT_FALSE(serializer.IsValid());
    }

    TEST_F(TrackChangedSerializerTests, FailedByteSerializationStillTracksSizeMutation)
    {
        AZStd::vector<uint8_t> source(size_t{32}, uint8_t{0x42});
        const AZStd::vector<uint8_t> encoded = EncodeByteString(source);
        AZStd::array<uint8_t, 8> destination{};
        uint32_t destinationSize = aznumeric_cast<uint32_t>(destination.size());
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> serializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));

        EXPECT_FALSE(serializer.SerializeBytes(
            destination.data(), aznumeric_cast<uint32_t>(destination.size()), true, destinationSize, "Value"));
        EXPECT_EQ(destinationSize, source.size());
        EXPECT_TRUE(serializer.GetTrackedChangesFlag());
        EXPECT_FALSE(serializer.IsValid());
    }

    TEST_F(TrackChangedSerializerTests, ByteStringsTrackEqualContentContentChangesAndSizeChanges)
    {
        AZStd::vector<uint8_t> source(size_t{32}, uint8_t{0x42});
        const AZStd::vector<uint8_t> encoded = EncodeByteString(source);

        AZStd::vector<uint8_t> equal = source;
        uint32_t equalSize = aznumeric_cast<uint32_t>(equal.size());
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> equalSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(equalSerializer.SerializeBytes(equal.data(), equalSize, true, equalSize, "Value"));
        EXPECT_FALSE(equalSerializer.GetTrackedChangesFlag());

        AZStd::vector<uint8_t> changed = source;
        changed.front() ^= 0xFF;
        uint32_t changedSize = aznumeric_cast<uint32_t>(changed.size());
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> changedSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(changedSerializer.SerializeBytes(changed.data(), changedSize, true, changedSize, "Value"));
        EXPECT_TRUE(changedSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(changed, source);

        AZStd::vector<uint8_t> resized(source.size(), 0);
        uint32_t resizedSize = 8;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> resizedSerializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));
        ASSERT_TRUE(resizedSerializer.SerializeBytes(
            resized.data(), aznumeric_cast<uint32_t>(resized.size()), true, resizedSize, "Value"));
        EXPECT_TRUE(resizedSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(resizedSize, source.size());
        EXPECT_TRUE(AZStd::equal(source.begin(), source.end(), resized.begin()));
    }

    TEST_F(TrackChangedSerializerTests, ByteStringsAboveTheOldSixteenKilobyteLimitAreSupported)
    {
        constexpr size_t ValueSize = 20 * 1024;
        AZStd::vector<uint8_t> source(ValueSize, uint8_t{0xA5});
        const AZStd::vector<uint8_t> encoded = EncodeByteString(source);
        AZStd::vector<uint8_t> destination = source;
        uint32_t destinationSize = aznumeric_cast<uint32_t>(destination.size());
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> serializer(
            encoded.data(), aznumeric_cast<uint32_t>(encoded.size()));

        ASSERT_TRUE(serializer.SerializeBytes(destination.data(), destinationSize, true, destinationSize, "Value"));
        EXPECT_FALSE(serializer.GetTrackedChangesFlag());
        EXPECT_EQ(destination, source);
    }
}
