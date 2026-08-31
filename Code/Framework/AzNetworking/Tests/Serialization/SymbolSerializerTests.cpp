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
#include <AzNetworking/Serialization/TrackChangedSerializer.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace UnitTest
{
    namespace
    {
        struct SymbolValue final
        {
            bool Serialize(AzNetworking::ISerializer& serializer)
            {
                return serializer.Serialize(m_symbol, "Symbol");
            }

            AZ::Symbol m_symbol;
        };

        class NoOpOutputSerializer final
            : public AzNetworking::NetworkOutputSerializer
        {
        public:
            NoOpOutputSerializer()
                : NetworkOutputSerializer(nullptr, 0)
            {
            }

            bool SerializeBytes(
                uint8_t*,
                uint32_t,
                bool,
                uint32_t&,
                const char*) override
            {
                return true;
            }
        };

        class OversizedOutputSerializer final
            : public AzNetworking::NetworkOutputSerializer
        {
        public:
            OversizedOutputSerializer()
                : NetworkOutputSerializer(nullptr, 0)
            {
            }

            bool SerializeBytes(
                uint8_t*,
                uint32_t bufferCapacity,
                bool,
                uint32_t& outSize,
                const char*) override
            {
                outSize = bufferCapacity + 1;
                return true;
            }
        };

        AZStd::vector<AZ::u8> EncodeSymbol(AZ::Symbol symbol)
        {
            AZStd::vector<AZ::u8> buffer(AZ::Symbol::MaxStringSize + sizeof(AZ::u16));
            AzNetworking::NetworkInputSerializer serializer(
                buffer.data(),
                aznumeric_cast<AZ::u32>(buffer.size()));
            EXPECT_TRUE(static_cast<AzNetworking::ISerializer&>(serializer).Serialize(symbol, "Symbol"));
            buffer.resize(serializer.GetSize());
            return buffer;
        }

        AZStd::vector<AZ::u8> EncodeValue(const AZStd::string_view value)
        {
            AZStd::vector<AZ::u8> buffer(sizeof(AZ::u16) + value.size());
            buffer[0] = static_cast<AZ::u8>(value.size() >> 8);
            buffer[1] = static_cast<AZ::u8>(value.size());
            AZStd::copy(value.begin(), value.end(), buffer.begin() + sizeof(AZ::u16));
            return buffer;
        }

        bool DecodeSymbol(
            const AZStd::span<const AZ::u8> input,
            AZ::Symbol& destination)
        {
            AzNetworking::NetworkOutputSerializer serializer(
                input.data(),
                aznumeric_cast<AZ::u32>(input.size()));
            return static_cast<AzNetworking::ISerializer&>(serializer).Serialize(destination, "Symbol");
        }
    } // namespace

    class SymbolSerializerTests
        : public LeakDetectionFixture
    {
    };

    TEST_F(SymbolSerializerTests, RawWireFormatIsBigEndianLengthFollowedByExactBytes)
    {
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol(AZ::Symbol{"abc"});
        const AZStd::array<AZ::u8, 5> expected = {0x00, 0x03, 'a', 'b', 'c'};

        EXPECT_EQ(encoded.size(), expected.size());
        EXPECT_TRUE(AZStd::equal(encoded.begin(), encoded.end(), expected.begin(), expected.end()));
    }

    TEST_F(SymbolSerializerTests, EmptyAndMaximumValuesHaveExpectedWireSizes)
    {
        const AZStd::vector<AZ::u8> empty = EncodeSymbol(AZ::Symbol{});
        EXPECT_EQ(empty, (AZStd::vector<AZ::u8>{0x00, 0x00}));

        const AZStd::string maximumValue(AZ::Symbol::MaxStringSize, 'm');
        const AZStd::vector<AZ::u8> maximum = EncodeSymbol(AZ::Symbol{maximumValue});
        EXPECT_EQ(maximum.size(), AZ::Symbol::MaxStringSize + sizeof(AZ::u16));
    }

    TEST_F(SymbolSerializerTests, EveryBoundedLengthTransitionUsesExactRawBytes)
    {
        constexpr AZStd::array<size_t, 6> ValueSizes = {0, 1, 255, 256, 1022, 1023};
        for (const size_t valueSize : ValueSizes)
        {
            const AZStd::string value(valueSize, 'v');
            const AZ::Symbol source{value};
            const AZStd::vector<AZ::u8> encoded = EncodeSymbol(source);

            ASSERT_EQ(encoded.size(), valueSize + sizeof(AZ::u16));
            EXPECT_EQ(encoded[0], static_cast<AZ::u8>(valueSize >> 8));
            EXPECT_EQ(encoded[1], static_cast<AZ::u8>(valueSize));
            EXPECT_TRUE(AZStd::equal(value.begin(), value.end(), encoded.begin() + sizeof(AZ::u16)));

            AZ::Symbol destination;
            ASSERT_TRUE(DecodeSymbol(encoded, destination));
            EXPECT_EQ(destination, source);
        }
    }

    TEST_F(SymbolSerializerTests, NoOpAndOversizedCustomOutputPreserveDestination)
    {
        const AZ::Symbol original{"PreservedByCustomSerializer"};

        AZ::Symbol unchanged = original;
        NoOpOutputSerializer noOpSerializer;
        EXPECT_TRUE(static_cast<AzNetworking::ISerializer&>(noOpSerializer).Serialize(unchanged, "Symbol"));
        EXPECT_EQ(unchanged, original);

        AZ::Symbol oversized = original;
        OversizedOutputSerializer oversizedSerializer;
        EXPECT_FALSE(static_cast<AzNetworking::ISerializer&>(oversizedSerializer).Serialize(oversized, "Symbol"));
        EXPECT_FALSE(oversizedSerializer.IsValid());
        EXPECT_EQ(oversized, original);
    }

    TEST_F(SymbolSerializerTests, UnknownIncomingValueIsInternedAndCanonical)
    {
        const AZStd::string value = AZStd::string::format("NetworkValue_%p", this);
        ASSERT_FALSE(AZ::Symbol::Find(value).has_value());
        const AZStd::vector<AZ::u8> encoded = EncodeValue(value);
        AZ::Symbol destination;

        ASSERT_TRUE(DecodeSymbol(encoded, destination));
        EXPECT_EQ(destination.GetStringView(), value);

        const AZStd::optional<AZ::Symbol> found = AZ::Symbol::Find(value);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(destination, *found);
    }

    TEST_F(SymbolSerializerTests, InvalidOrTruncatedInputPreservesDestination)
    {
        const AZ::Symbol original{"UnchangedOnMalformed"};
        const AZStd::array<AZ::u8, 2> oversizedLength = {0x04, 0x00};
        const AZStd::array<AZ::u8, 4> malformedUtf8 = {0x00, 0x02, 0xC0, 0xAF};
        const AZStd::array<AZ::u8, 5> embeddedNull = {0x00, 0x03, 'a', 0x00, 'b'};
        const AZStd::array<AZ::u8, 4> truncated = {0x00, 0x03, 'a', 'b'};

        for (const AZStd::span<const AZ::u8> input :
            {
                AZStd::span<const AZ::u8>{oversizedLength},
                AZStd::span<const AZ::u8>{malformedUtf8},
                AZStd::span<const AZ::u8>{embeddedNull},
                AZStd::span<const AZ::u8>{truncated},
            })
        {
            AZ::Symbol destination = original;
            EXPECT_FALSE(DecodeSymbol(input, destination));
            EXPECT_EQ(destination, original);
        }
    }

    TEST_F(SymbolSerializerTests, EveryRawWireTruncationBoundaryPreservesDestination)
    {
        const AZ::Symbol original{"UnchangedAtEveryBoundary"};
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol(AZ::Symbol{"BoundaryValue"});

        for (size_t truncatedSize = 0; truncatedSize < encoded.size(); ++truncatedSize)
        {
            AZ::Symbol destination = original;
            const AZStd::span<const AZ::u8> truncated{encoded.data(), truncatedSize};
            EXPECT_FALSE(DecodeSymbol(truncated, destination)) << "Truncated size " << truncatedSize;
            EXPECT_EQ(destination, original) << "Truncated size " << truncatedSize;
        }
    }

    TEST_F(SymbolSerializerTests, AllowedControlCharacterRoundTrips)
    {
        constexpr char value[] = {'a', '\n', 'b'};
        const AZ::Symbol source{AZStd::string_view{value, sizeof(value)}};
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol(source);
        AZ::Symbol destination;

        EXPECT_TRUE(DecodeSymbol(encoded, destination));
        EXPECT_EQ(destination, source);
    }

    TEST_F(SymbolSerializerTests, TrackChangedCanDeserializeWithoutSymbolSpecificState)
    {
        const AZ::Symbol source{"TrackChangedSymbol"};
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol(source);

        AZ::Symbol unchanged = source;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> unchangedSerializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()));
        EXPECT_TRUE(unchangedSerializer.Serialize(unchanged, "Symbol"));
        EXPECT_FALSE(unchangedSerializer.GetTrackedChangesFlag());

        AZ::Symbol changed{"DifferentSymbol"};
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> changedSerializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()));
        EXPECT_TRUE(changedSerializer.Serialize(changed, "Symbol"));
        EXPECT_TRUE(changedSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(changed, source);
    }

    TEST_F(SymbolSerializerTests, UnchangedNonEmptyDeltaPreservesValue)
    {
        SymbolValue base{AZ::Symbol{"UnchangedNonEmptyDelta"}};
        SymbolValue current = base;
        AzNetworking::SerializerDelta delta;
        AzNetworking::DeltaSerializerCreate createSerializer(delta);

        ASSERT_TRUE(createSerializer.CreateDelta(base, current));
        ASSERT_EQ(delta.GetNumDirtyBits(), 1);
        EXPECT_FALSE(delta.GetDirtyBit(0));
        EXPECT_EQ(delta.GetBufferSize(), 0);

        SymbolValue output = base;
        AzNetworking::DeltaSerializerApply applySerializer(delta);
        ASSERT_TRUE(applySerializer.ApplyDelta(output));
        EXPECT_EQ(output.m_symbol, base.m_symbol);
    }

    TEST_F(SymbolSerializerTests, DeltaSupportsOnlyValuesThatFitItsExistingPayload)
    {
        SymbolValue base;
        SymbolValue current{AZ::Symbol{AZStd::string(AZ::Symbol::MaxStringSize - 1, 'd')}};
        AzNetworking::SerializerDelta fittingDelta;
        AzNetworking::DeltaSerializerCreate fittingSerializer(fittingDelta);

        ASSERT_TRUE(fittingSerializer.CreateDelta(base, current));
        EXPECT_EQ(fittingDelta.GetBufferSize(), fittingDelta.GetBufferCapacity());

        SymbolValue output = base;
        AzNetworking::DeltaSerializerApply applySerializer(fittingDelta);
        ASSERT_TRUE(applySerializer.ApplyDelta(output));
        EXPECT_EQ(output.m_symbol, current.m_symbol);

        SymbolValue tooLarge{AZ::Symbol{AZStd::string(AZ::Symbol::MaxStringSize, 'x')}};
        AzNetworking::SerializerDelta oversizedDelta;
        AzNetworking::DeltaSerializerCreate oversizedSerializer(oversizedDelta);
        EXPECT_FALSE(oversizedSerializer.CreateDelta(base, tooLarge));
        EXPECT_EQ(oversizedDelta.GetNumDirtyBits(), 0);
        EXPECT_EQ(oversizedDelta.GetBufferSize(), 0);
    }
} // namespace UnitTest
