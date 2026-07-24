/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/City.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/UnitTest/TestTypes.h>

#include "CityReferenceVectors.h"

using namespace AZ;

namespace UnitTest
{
    class Hash_City : public LeakDetectionFixture
    {
    };

    TEST_F(Hash_City, DefaultConstructorIsZero)
    {
        constexpr AZ::Hash::City32 city32;
        static_assert(city32.GetValue() == 0, "Default constructed City32 should be 0");
        EXPECT_EQ(city32.GetValue(), 0u);

        constexpr AZ::Hash::City64 city64;
        static_assert(city64.GetValue() == 0, "Default constructed City64 should be 0");
        EXPECT_EQ(city64.GetValue(), 0u);

        constexpr AZ::Hash::City128 city128;
        static_assert(city128.GetLow() == 0 && city128.GetHigh() == 0, "Default constructed City128 should be 0");
        EXPECT_EQ(city128.GetLow(), 0u);
        EXPECT_EQ(city128.GetHigh(), 0u);
    }

    TEST_F(Hash_City, RawValueConstructor)
    {
        constexpr u32 raw32 = 0xDEADBEEF;
        constexpr AZ::Hash::City32 city32{raw32};
        static_assert(static_cast<u32>(city32) == raw32);
        EXPECT_EQ(static_cast<u32>(city32), raw32);

        constexpr u64 raw64 = 0xDEADBEEF'FEEDFACE;
        constexpr AZ::Hash::City64 city64{raw64};
        static_assert(static_cast<u64>(city64) == raw64);
        EXPECT_EQ(static_cast<u64>(city64), raw64);

        constexpr AZ::Hash::City128 city128{0x0123456789ABCDEFull, 0xFEDCBA9876543210ull};
        static_assert(city128.GetLow() == 0x0123456789ABCDEFull);
        static_assert(city128.GetHigh() == 0xFEDCBA9876543210ull);
        EXPECT_EQ(city128.GetLow(), 0x0123456789ABCDEFull);
        EXPECT_EQ(city128.GetHigh(), 0xFEDCBA9876543210ull);
    }

    TEST_F(Hash_City, EmptyInputMatchesReferenceVectors)
    {
        // Row 0 of Google's reference vectors is the hash of a zero-length input.
        static_assert(AZ::Hash::City64{""}.GetValue() == 0x9AE16A3B2F90404Full);
        static_assert(AZ::Hash::City32{""}.GetValue() == 0xDC56D17Au);
        static_assert(AZ::Hash::City128{""}.GetLow() == 0x3DF09DFC64C09A2Bull);
        static_assert(AZ::Hash::City128{""}.GetHigh() == 0x3CB540C392E51E29ull);

        EXPECT_EQ(AZ::Hash::City64{""}.GetValue(), 0x9AE16A3B2F90404Full);
        EXPECT_EQ(AZ::Hash::City32{""}.GetValue(), 0xDC56D17Au);
        EXPECT_EQ(AZ::Hash::City128{""}.GetLow(), 0x3DF09DFC64C09A2Bull);
        EXPECT_EQ(AZ::Hash::City128{""}.GetHigh(), 0x3CB540C392E51E29ull);
    }

    TEST_F(Hash_City, CompileTimeMatchesRuntime)
    {
        const AZStd::string str{"O3DE hashing helpers"};

        {
            constexpr AZ::Hash::City32 compileHash{"O3DE hashing helpers"};
            const AZ::Hash::City32 runtimeHash{str};
            EXPECT_EQ(compileHash.GetValue(), runtimeHash.GetValue());
        }

        {
            constexpr AZ::Hash::City64 compileHash{"O3DE hashing helpers"};
            const AZ::Hash::City64 runtimeHash{str};
            EXPECT_EQ(compileHash.GetValue(), runtimeHash.GetValue());
        }

        {
            constexpr AZ::Hash::City128 compileHash{"O3DE hashing helpers"};
            const AZ::Hash::City128 runtimeHash{str};
            EXPECT_EQ(compileHash.GetLow(), runtimeHash.GetLow());
            EXPECT_EQ(compileHash.GetHigh(), runtimeHash.GetHigh());
        }
    }

    TEST_F(Hash_City, SpanMatchesStringForAsciiData)
    {
        constexpr u8 data[] = {'A', 'B', 'C'};
        EXPECT_EQ(AZ::Hash::City32{data}.GetValue(), AZ::Hash::City32{"ABC"}.GetValue());
        EXPECT_EQ(AZ::Hash::City64{data}.GetValue(), AZ::Hash::City64{"ABC"}.GetValue());
        EXPECT_EQ(AZ::Hash::City128{data}.GetLow(), AZ::Hash::City128{"ABC"}.GetLow());
        EXPECT_EQ(AZ::Hash::City128{data}.GetHigh(), AZ::Hash::City128{"ABC"}.GetHigh());
    }

    TEST_F(Hash_City, BinaryDataWithNulsIsNotTruncated)
    {
        const u8 data[] = {0x00, 0xFF, 0x42, 0x00, 0x13};
        EXPECT_NE(AZ::Hash::City64{data}.GetValue(), AZ::Hash::City64{AZStd::span<const AZStd::byte>{}}.GetValue());
        EXPECT_NE(AZ::Hash::City32{data}.GetValue(), AZ::Hash::City32{AZStd::span<const AZStd::byte>{}}.GetValue());
    }

    TEST_F(Hash_City, DifferentInputsProduceDifferentHashes)
    {
        EXPECT_NE(AZ::Hash::City64{"ABC"}, AZ::Hash::City64{"ABD"});
        EXPECT_NE(AZ::Hash::City64{"ABC"}, AZ::Hash::City64{"abc"});
        EXPECT_NE(AZ::Hash::City32{"Alpha"}, AZ::Hash::City32{"Beta"});
    }

    TEST_F(Hash_City, SeededHashDiffersFromUnseeded)
    {
        constexpr u8 data[] = {'S', 'e', 'e', 'd'};

        const AZ::Hash::City64 unseeded{data};
        const AZ::Hash::City64 seeded{data, 0x1234567890ABCDEFull};
        EXPECT_NE(unseeded.GetValue(), seeded.GetValue());

        constexpr AZ::Hash::City128 seed{0x1111111111111111ull, 0x2222222222222222ull};
        const AZ::Hash::City128 unseeded128{data};
        const AZ::Hash::City128 seeded128{data, seed};
        EXPECT_NE(unseeded128, seeded128);
    }

    TEST_F(Hash_City, ComparisonOperators)
    {
        constexpr AZ::Hash::City64 a{"A"};
        constexpr AZ::Hash::City64 b{"B"};
        constexpr AZ::Hash::City64 aDup{"A"};

        static_assert(a == aDup);
        static_assert(a != b);
        static_assert((a < b) || (a > b));

        EXPECT_EQ(a, aDup);
        EXPECT_NE(a, b);
        EXPECT_TRUE((a < b) || (a > b));
    }

    TEST_F(Hash_City, BoolOperator)
    {
        constexpr AZ::Hash::City64 zero;
        constexpr AZ::Hash::City64 nonZero{"X"};
        static_assert(!zero, "Default (zero) hash should be falsy");
        static_assert(!!nonZero, "Non-zero hash should be truthy");
        EXPECT_FALSE(static_cast<bool>(zero));
        EXPECT_TRUE(static_cast<bool>(nonZero));

        constexpr AZ::Hash::City128 zero128;
        constexpr AZ::Hash::City128 nonZero128{"X"};
        static_assert(!zero128);
        static_assert(!!nonZero128);
        EXPECT_FALSE(static_cast<bool>(zero128));
        EXPECT_TRUE(static_cast<bool>(nonZero128));
    }

    TEST_F(Hash_City, UsableAsUnorderedMapKey)
    {
        AZStd::unordered_map<AZ::Hash::City64, s32> map;
        map[AZ::Hash::City64{"Alpha"}] = 1;
        map[AZ::Hash::City64{"Beta"}] = 2;
        map[AZ::Hash::City64{"Alpha"}] = 42; // overwrites

        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[AZ::Hash::City64{"Alpha"}], 42);
        EXPECT_EQ(map[AZ::Hash::City64{"Beta"}], 2);

        AZStd::unordered_map<AZ::Hash::City128, s32> map128;
        map128[AZ::Hash::City128{"Alpha"}] = 7;
        EXPECT_EQ(map128[AZ::Hash::City128{"Alpha"}], 7);
    }

    // Port of Google's official CityHash correctness suite (google/cityhash, src/city-test.cc).
    // Reproduces the pseudo-random data buffer and the (offset, length) schedule,
    // then checks every portable CityHash variant against the pinned reference vectors.
    // Passing this proves the constexpr port matches Google's algorithm (CityHash v1.1) byte-for-byte.
    TEST_F(Hash_City, ReferenceVectors)
    {
        constexpr u64 k0 = 0xC3A5C85C97CB3127ull;
        constexpr size_t kDataSize = 1u << 20;

        AZStd::vector<AZStd::byte> data(kDataSize);
        {
            u64 a = 9;
            u64 b = 777;
            for (size_t i = 0; i < kDataSize; ++i)
            {
                a += b;
                b += a;
                a = (a ^ (a >> 41)) * k0;
                b = (b ^ (b >> 41)) * k0 + i;
                data[i] = static_cast<AZStd::byte>(static_cast<u8>(b >> 37));
            }
        }

        constexpr u64 kSeed0 = 1234567;
        constexpr u64 kSeed1 = k0;
        constexpr AZ::Hash::City128 kSeed128{kSeed0, kSeed1}; // uint128(low = kSeed0, high = kSeed1)

        auto checkOne = [&](const size_t index, const size_t offset, const size_t len)
        {
            const CityReferenceVector& expected = CityReferenceVectors[index];
            const AZStd::span<const AZStd::byte> chunk{data.data() + offset, len};

            EXPECT_EQ(AZ::Hash::City64{chunk}.GetValue(), expected.m_city64) << "City64 at index " << index;
            EXPECT_EQ(AZ::Hash::City64(chunk, kSeed0).GetValue(), expected.m_city64Seed) << "City64Seed at index " << index;
            EXPECT_EQ(AZ::Hash::City64(chunk, kSeed0, kSeed1).GetValue(), expected.m_city64Seeds)
                << "City64Seeds at index " << index;

            const AZ::Hash::City128 c128{chunk};
            EXPECT_EQ(c128.GetLow(), expected.m_city128Low) << "City128 low at index " << index;
            EXPECT_EQ(c128.GetHigh(), expected.m_city128High) << "City128 high at index " << index;

            const AZ::Hash::City128 c128Seeded{chunk, kSeed128};
            EXPECT_EQ(c128Seeded.GetLow(), expected.m_city128SeedLow) << "City128Seed low at index " << index;
            EXPECT_EQ(c128Seeded.GetHigh(), expected.m_city128SeedHigh) << "City128Seed high at index " << index;

            EXPECT_EQ(AZ::Hash::City32{chunk}.GetValue(), expected.m_city32) << "City32 at index " << index;
        };

        constexpr size_t kTestSize = 300;
        for (size_t i = 0; i + 1 < kTestSize; ++i)
        {
            checkOne(i, i * i, i);
        }
        checkOne(kTestSize - 1, 0, kDataSize);
    }
} // namespace UnitTest
