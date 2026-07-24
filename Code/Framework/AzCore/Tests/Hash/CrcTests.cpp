/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Crc.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/UnitTest/TestTypes.h>

using namespace AZ;

namespace UnitTest
{
    class Hash_Crc : public LeakDetectionFixture
    {
    };

    TEST_F(Hash_Crc, DefaultConstructorIsZero)
    {
        constexpr AZ::Hash::Crc32 crc32;
        static_assert(crc32.GetValue() == 0, "Default constructed Crc32 should be 0");
        EXPECT_EQ(crc32.GetValue(), 0u);

        constexpr AZ::Hash::Crc64 crc64;
        static_assert(crc64.GetValue() == 0, "Default constructed Crc64 should be 0");
        EXPECT_EQ(crc64.GetValue(), 0u);
    }

    TEST_F(Hash_Crc, EmptyStringIsZero)
    {
        // With init == xorout == all-ones, an empty message hashes to 0.
        static_assert(AZ::Hash::Crc32{""}.GetValue() == 0u);
        static_assert(AZ::Hash::Crc64{""}.GetValue() == 0u);
        EXPECT_EQ(AZ::Hash::Crc32{""}.GetValue(), 0u);
        EXPECT_EQ(AZ::Hash::Crc64{""}.GetValue(), 0u);
    }

    TEST_F(Hash_Crc, CanonicalCheckValue)
    {
        // The standard CRC check value is the CRC of the ASCII string "123456789".
        static_assert(AZ::Hash::Crc32{"123456789"}.GetValue() == 0xCBF43926u, "CRC-32/ISO-HDLC check value");
        static_assert(AZ::Hash::Crc64{"123456789"}.GetValue() == 0x995DC9BBDF1939FAull, "CRC-64/XZ check value");

        EXPECT_EQ(AZ::Hash::Crc32{"123456789"}.GetValue(), 0xCBF43926u);
        EXPECT_EQ(AZ::Hash::Crc64{"123456789"}.GetValue(), 0x995DC9BBDF1939FAull);
    }

    TEST_F(Hash_Crc, KnownVectors)
    {
        // "The quick brown fox jumps over the lazy dog" - CRC-32/ISO-HDLC = 0x414FA339.
        static_assert(AZ::Hash::Crc32{"The quick brown fox jumps over the lazy dog"}.GetValue() == 0x414FA339u);
        EXPECT_EQ(AZ::Hash::Crc32{"The quick brown fox jumps over the lazy dog"}.GetValue(), 0x414FA339u);
    }

    TEST_F(Hash_Crc, LiteralOperator)
    {
        static_assert("123456789"_crc32 == AZ::Hash::Crc32{"123456789"});
        static_assert("123456789"_crc64 == AZ::Hash::Crc64{"123456789"});
        static_assert(static_cast<u32>("123456789"_crc32) == 0xCBF43926u);
        EXPECT_EQ(static_cast<u32>("123456789"_crc32), 0xCBF43926u);
        EXPECT_EQ(static_cast<u64>("123456789"_crc64), 0x995DC9BBDF1939FAull);
    }

    TEST_F(Hash_Crc, CompileTimeMatchesRuntime)
    {
        const AZStd::string str{"O3DE"};

        {
            constexpr u32 compileHash = AZ::Hash::Crc32{"O3DE"};
            const u32 runtimeHash = AZ::Hash::Crc32{str};
            EXPECT_EQ(compileHash, runtimeHash);
        }

        {
            constexpr u64 compileHash = AZ::Hash::Crc64{"O3DE"};
            const u64 runtimeHash = AZ::Hash::Crc64{str};
            EXPECT_EQ(compileHash, runtimeHash);
        }
    }

    TEST_F(Hash_Crc, RawValueConstructorAndConversion)
    {
        constexpr u32 raw32 = 0x12345678;
        constexpr AZ::Hash::Crc32 crc32{raw32};
        static_assert(static_cast<u32>(crc32) == raw32);
        EXPECT_EQ(static_cast<u32>(crc32), raw32);

        constexpr u64 raw64 = 0x123456789ABCDEF0;
        constexpr AZ::Hash::Crc64 crc64{raw64};
        static_assert(static_cast<u64>(crc64) == raw64);
        EXPECT_EQ(static_cast<u64>(crc64), raw64);
    }

    TEST_F(Hash_Crc, SpanMatchesStringForAsciiData)
    {
        constexpr u8 data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

        constexpr u32 spanHash32 = AZ::Hash::Crc32{data};
        static_assert(spanHash32 == 0xCBF43926u, "Byte span CRC of ASCII digits should match the string CRC");
        EXPECT_EQ(spanHash32, 0xCBF43926u);

        constexpr u64 spanHash64 = AZ::Hash::Crc64{data};
        static_assert(spanHash64 == 0x995DC9BBDF1939FAull);
        EXPECT_EQ(spanHash64, 0x995DC9BBDF1939FAull);
    }

    TEST_F(Hash_Crc, ConstexprArrayAndRuntimeVector)
    {
        constexpr AZStd::array<u8, 3> arr = {'A', 'B', 'C'};
        constexpr u32 arrHash = AZ::Hash::Crc32{arr};
        static_assert(arrHash == AZ::Hash::Crc32{"ABC"});
        EXPECT_EQ(arrHash, AZ::Hash::Crc32{"ABC"}.GetValue());

        AZStd::vector<u8> vec;
        vec.push_back('A');
        vec.push_back('B');
        vec.push_back('C');
        EXPECT_EQ(AZ::Hash::Crc32{vec}.GetValue(), AZ::Hash::Crc32{"ABC"}.GetValue());
        EXPECT_EQ(AZ::Hash::Crc64{vec}.GetValue(), AZ::Hash::Crc64{"ABC"}.GetValue());
    }

    TEST_F(Hash_Crc, BinaryDataWithNulsIsNotTruncated)
    {
        const u8 data[] = {0x00, 0xFF, 0x42, 0x00, 0x13};
        EXPECT_NE(AZ::Hash::Crc32{data}.GetValue(), 0u);
        EXPECT_NE(AZ::Hash::Crc64{data}.GetValue(), 0u);
    }

    TEST_F(Hash_Crc, DifferentInputsProduceDifferentHashes)
    {
        EXPECT_NE(AZ::Hash::Crc32{"ABC"}, AZ::Hash::Crc32{"ABD"});
        EXPECT_NE(AZ::Hash::Crc32{"ABC"}, AZ::Hash::Crc32{"abc"});
        EXPECT_NE(AZ::Hash::Crc64{"ABC"}, AZ::Hash::Crc64{"abc"});
    }

    TEST_F(Hash_Crc, ComparisonOperators)
    {
        constexpr AZ::Hash::Crc32 a{"A"};
        constexpr AZ::Hash::Crc32 b{"B"};
        constexpr AZ::Hash::Crc32 aDup{"A"};

        static_assert(a == aDup);
        static_assert(a != b);
        static_assert((a < b) || (a > b));
        EXPECT_EQ(a, aDup);
        EXPECT_NE(a, b);
        EXPECT_TRUE((a < b) || (a > b));
    }

    TEST_F(Hash_Crc, BoolOperator)
    {
        constexpr AZ::Hash::Crc32 zero;
        constexpr AZ::Hash::Crc32 nonZero{"X"};
        static_assert(!zero);
        static_assert(!!nonZero);
        EXPECT_FALSE(static_cast<bool>(zero));
        EXPECT_TRUE(static_cast<bool>(nonZero));
    }

    TEST_F(Hash_Crc, StdHashReturnsValue)
    {
        constexpr AZ::Hash::Crc32 hash{"123456789"};
        constexpr AZStd::hash<AZ::Hash::Crc32> hasher;
        EXPECT_EQ(hasher(hash), static_cast<size_t>(hash.GetValue()));
    }

    TEST_F(Hash_Crc, UsableAsUnorderedMapKey)
    {
        AZStd::unordered_map<AZ::Hash::Crc32, s32> map;
        map[AZ::Hash::Crc32{"Alpha"}] = 1;
        map[AZ::Hash::Crc32{"Beta"}] = 2;
        map[AZ::Hash::Crc32{"Alpha"}] = 42; // overwrites

        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[AZ::Hash::Crc32{"Alpha"}], 42);
        EXPECT_EQ(map[AZ::Hash::Crc32{"Beta"}], 2);
    }

    TEST_F(Hash_Crc, DoesNotLowerCaseUnlikeAzCrc32)
    {
        // Unlike AZ::Crc32, this type hashes the exact bytes, so case matters.
        EXPECT_NE(AZ::Hash::Crc32{"CaseSensitive"}, AZ::Hash::Crc32{"casesensitive"});
    }
} // namespace UnitTest
