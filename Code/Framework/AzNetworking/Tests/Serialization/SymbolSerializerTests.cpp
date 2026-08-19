/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/DeltaSerializer.h>
#include <AzNetworking/Serialization/Internal/SymbolAdmissionPolicy.h>
#include <AzNetworking/Serialization/NetworkInputSerializer.h>
#include <AzNetworking/Serialization/NetworkOutputSerializer.h>
#include <AzNetworking/Serialization/TrackChangedSerializer.h>
#include <AzNetworking/Serialization/TypeValidatingSerializer.h>
#include <AzCore/Console/Console.h>
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

        template <typename InputSerializer>
        AZStd::vector<AZ::u8> EncodeSymbol(AZ::Symbol symbol)
        {
            AZStd::vector<AZ::u8> buffer(AZ::Symbol::MaxLength + sizeof(AZ::u16));
            InputSerializer serializer(buffer.data(), aznumeric_cast<AZ::u32>(buffer.size()));
            EXPECT_TRUE(static_cast<AzNetworking::ISerializer&>(serializer).Serialize(symbol, "Symbol"));
            buffer.resize(serializer.GetSize());
            return buffer;
        }

        AZStd::vector<AZ::u8> EncodeSpelling(const AZStd::string_view spelling)
        {
            AZStd::vector<AZ::u8> buffer(sizeof(AZ::u16) + spelling.size());
            buffer[0] = static_cast<AZ::u8>(spelling.size() >> 8);
            buffer[1] = static_cast<AZ::u8>(spelling.size());
            AZStd::copy(spelling.begin(), spelling.end(), buffer.begin() + sizeof(AZ::u16));
            return buffer;
        }
    } // namespace

    class SymbolSerializerTests
        : public LeakDetectionFixture
    {
    };

    TEST_F(SymbolSerializerTests, RawWireFormatIsBigEndianLengthFollowedByExactBytes)
    {
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol<AzNetworking::NetworkInputSerializer>(
            AZ::Symbol{AZStd::string_view{"abc"}});
        const AZStd::array<AZ::u8, 5> expected = {0x00, 0x03, 'a', 'b', 'c'};

        EXPECT_EQ(encoded.size(), expected.size());
        EXPECT_TRUE(AZStd::equal(encoded.begin(), encoded.end(), expected.begin(), expected.end()));
    }

    TEST_F(SymbolSerializerTests, EmptyAndMaximumValuesHaveBoundedWireSizes)
    {
        const AZStd::vector<AZ::u8> empty = EncodeSymbol<AzNetworking::NetworkInputSerializer>(AZ::Symbol{});
        EXPECT_EQ(empty, (AZStd::vector<AZ::u8>{0x00, 0x00}));

        const AZStd::string maximumSpelling(AZ::Symbol::MaxLength, 'm');
        const AZStd::vector<AZ::u8> maximum = EncodeSymbol<AzNetworking::NetworkInputSerializer>(
            AZ::Symbol{maximumSpelling});
        EXPECT_EQ(maximum.size(), AZ::Symbol::MaxLength + sizeof(AZ::u16));
    }

    TEST_F(SymbolSerializerTests, TrustedLocalRoundTripPreservesCanonicalIdentity)
    {
        const AZ::Symbol source{AZStd::string_view{"TrustedRoundTrip"}};
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol<AzNetworking::NetworkInputSerializer>(source);
        AZ::Symbol destination;
        AzNetworking::NetworkOutputSerializer serializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()));

        EXPECT_TRUE(static_cast<AzNetworking::ISerializer&>(serializer).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination, source);
    }

    TEST_F(SymbolSerializerTests, ExistingOnlyRejectsUnknownWithoutMutationOrInterning)
    {
        const AZStd::string spelling = AZStd::string::format("UnknownExistingOnly_%p", this);
        AZStd::vector<AZ::u8> encoded(sizeof(AZ::u16) + spelling.size());
        encoded[0] = static_cast<AZ::u8>(spelling.size() >> 8);
        encoded[1] = static_cast<AZ::u8>(spelling.size());
        AZStd::copy(spelling.begin(), spelling.end(), encoded.begin() + sizeof(AZ::u16));
        AZ::Symbol destination{AZStd::string_view{"PreservedDestination"}};
        const AZ::Symbol original = destination;
        AzNetworking::Internal::SymbolSerializationContext context{
            AzNetworking::SymbolAdmission::ExistingOnly,
            nullptr};
        AzNetworking::NetworkOutputSerializer serializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()),
            context);

        EXPECT_FALSE(static_cast<AzNetworking::ISerializer&>(serializer).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination, original);

        AZ::Symbol found;
        EXPECT_FALSE(AZ::Internal::FindSymbol(found, spelling));
    }

    TEST_F(SymbolSerializerTests, ExistingOnlyResolvesAlreadyInternedSymbol)
    {
        const AZ::Symbol source{AZStd::string_view{"ExistingOnlyKnown"}};
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol<AzNetworking::NetworkInputSerializer>(source);
        AzNetworking::Internal::SymbolSerializationContext context{
            AzNetworking::SymbolAdmission::ExistingOnly,
            nullptr};
        AZ::Symbol destination;
        AzNetworking::NetworkOutputSerializer serializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()),
            context);

        EXPECT_TRUE(static_cast<AzNetworking::ISerializer&>(serializer).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination, source);
    }

    TEST_F(SymbolSerializerTests, NetworkOriginRequiresPolicyAndAdmitsTransactionally)
    {
        const AZStd::string spelling = AZStd::string::format("NetworkOriginAdmission_%p", this);
        const AZStd::vector<AZ::u8> encoded = EncodeSpelling(spelling);
        AZ::Symbol destination{AZStd::string_view{"AdmissionDestination"}};
        const AZ::Symbol original = destination;
        AzNetworking::Internal::SymbolSerializationContext missingPolicy{
            AzNetworking::SymbolAdmission::NetworkOrigin,
            nullptr};
        AzNetworking::NetworkOutputSerializer rejected(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()),
            missingPolicy);

        EXPECT_FALSE(static_cast<AzNetworking::ISerializer&>(rejected).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination, original);

        AzNetworking::Internal::SymbolAdmissionPolicy policy;
        AzNetworking::Internal::SymbolSerializationContext admittedContext{
            AzNetworking::SymbolAdmission::NetworkOrigin,
            &policy};
        AzNetworking::NetworkOutputSerializer admitted(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()),
            admittedContext);

        EXPECT_TRUE(static_cast<AzNetworking::ISerializer&>(admitted).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination.GetStringView(), spelling);
    }

    TEST_F(SymbolSerializerTests, NetworkOriginLimitRejectsBeforeInterning)
    {
        const AZStd::string firstSpelling = AZStd::string::format("AdmissionFirst_%p", this);
        const AZStd::string secondSpelling = AZStd::string::format("AdmissionSecond_%p", this);
        const AZStd::vector<AZ::u8> firstEncoded = EncodeSpelling(firstSpelling);
        const AZStd::vector<AZ::u8> secondEncoded = EncodeSpelling(secondSpelling);
        AzNetworking::Internal::SymbolAdmissionPolicy policy{1};
        const AzNetworking::Internal::SymbolSerializationContext context{
            AzNetworking::SymbolAdmission::NetworkOrigin,
            &policy};

        AZ::Symbol destination;
        AzNetworking::NetworkOutputSerializer firstSerializer(
            firstEncoded.data(),
            aznumeric_cast<AZ::u32>(firstEncoded.size()),
            context);
        ASSERT_TRUE(static_cast<AzNetworking::ISerializer&>(firstSerializer).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination.GetStringView(), firstSpelling);

        const AZ::Symbol original = destination;
        AzNetworking::NetworkOutputSerializer secondSerializer(
            secondEncoded.data(),
            aznumeric_cast<AZ::u32>(secondEncoded.size()),
            context);
        EXPECT_FALSE(static_cast<AzNetworking::ISerializer&>(secondSerializer).Serialize(destination, "Symbol"));
        EXPECT_EQ(destination, original);

        AZ::Symbol found;
        EXPECT_FALSE(AZ::Internal::FindSymbol(found, secondSpelling));
    }

    TEST_F(SymbolSerializerTests, MalformedLengthAndSpellingPreserveDestination)
    {
        const AZ::Symbol original{AZStd::string_view{"UnchangedOnMalformed"}};
        const AZStd::array<AZ::u8, 2> oversizedLength = {0x04, 0x00};
        const AZStd::array<AZ::u8, 4> malformedUtf8 = {0x00, 0x02, 0xC0, 0xAF};

        for (const auto& input : {AZStd::span<const AZ::u8>{oversizedLength}, AZStd::span<const AZ::u8>{malformedUtf8}})
        {
            AZ::Symbol destination = original;
            AzNetworking::NetworkOutputSerializer serializer(
                input.data(),
                aznumeric_cast<AZ::u32>(input.size()));
            EXPECT_FALSE(static_cast<AzNetworking::ISerializer&>(serializer).Serialize(destination, "Symbol"));
            EXPECT_EQ(destination, original);
        }
    }

    TEST_F(SymbolSerializerTests, TrackChangedComparesFinalSpellingSemantics)
    {
        const AZ::Symbol source{AZStd::string_view{"TrackChangedSymbol"}};
        const AZStd::vector<AZ::u8> encoded = EncodeSymbol<AzNetworking::NetworkInputSerializer>(source);

        AZ::Symbol unchanged = source;
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> unchangedSerializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()));
        EXPECT_TRUE(unchangedSerializer.Serialize(unchanged, "Symbol"));
        EXPECT_FALSE(unchangedSerializer.GetTrackedChangesFlag());

        AZ::Symbol changed{AZStd::string_view{"DifferentSymbol"}};
        AzNetworking::TrackChangedSerializer<AzNetworking::NetworkOutputSerializer> changedSerializer(
            encoded.data(),
            aznumeric_cast<AZ::u32>(encoded.size()));
        EXPECT_TRUE(changedSerializer.Serialize(changed, "Symbol"));
        EXPECT_TRUE(changedSerializer.GetTrackedChangesFlag());
        EXPECT_EQ(changed, source);
    }

    TEST_F(SymbolSerializerTests, DeltaCanCarryOneChangedMaximumSymbol)
    {
        SymbolValue base;
        SymbolValue current{AZ::Symbol{AZStd::string(AZ::Symbol::MaxLength, 'd')}};
        AzNetworking::SerializerDelta delta;
        AzNetworking::DeltaSerializerCreate createSerializer(delta);
        ASSERT_TRUE(createSerializer.CreateDelta(base, current));
        EXPECT_EQ(delta.GetBufferSize(), AZ::Symbol::MaxLength + sizeof(AZ::u16));

        SymbolValue output = base;
        AzNetworking::DeltaSerializerApply applySerializer(delta);
        ASSERT_TRUE(applySerializer.ApplyDelta(output));
        EXPECT_EQ(output.m_symbol, current.m_symbol);
    }
} // namespace UnitTest
