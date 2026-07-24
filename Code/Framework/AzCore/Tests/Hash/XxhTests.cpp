/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Xxh.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/UnitTest/TestTypes.h>

#include "XxhReferenceVectors.h"

using namespace AZ;

namespace UnitTest
{
    class Hash_Xxh : public LeakDetectionFixture
    {
    };

    TEST_F(Hash_Xxh, Xxh32_ReferenceVectors)
    {
        for (const Xxh32Vector& v : Xxh32Vectors)
        {
            const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(v.m_len);
            const AZ::Hash::Xxh32 hash{AZStd::span<const AZStd::byte>{buffer.data(), v.m_len}, v.m_seed};
            EXPECT_EQ(hash.GetValue(), v.m_expected) << "XXH32 len=" << v.m_len << " seed=" << v.m_seed;
        }
    }

    TEST_F(Hash_Xxh, Xxh64_ReferenceVectors)
    {
        for (const Xxh64Vector& v : Xxh64Vectors)
        {
            const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(v.m_len);
            const AZ::Hash::Xxh64 hash{AZStd::span<const AZStd::byte>{buffer.data(), v.m_len}, v.m_seed};
            EXPECT_EQ(hash.GetValue(), v.m_expected) << "XXH64 len=" << v.m_len << " seed=" << v.m_seed;
        }
    }

    TEST_F(Hash_Xxh, KnownStringVectors_CompileTimeAndRuntime)
    {
        static_assert(AZ::Hash::Xxh32{""}.GetValue() == 0x02CC5D05u);
        static_assert(AZ::Hash::Xxh32{"abc"}.GetValue() == 0x32D153FFu);
        static_assert(AZ::Hash::Xxh64{""}.GetValue() == 0xEF46DB3751D8E999ull);
        static_assert(AZ::Hash::Xxh64{"abc"}.GetValue() == 0x44BC2CF5AD770999ull);

        EXPECT_EQ(AZ::Hash::Xxh32{"abc"}.GetValue(), 0x32D153FFu);
        EXPECT_EQ(AZ::Hash::Xxh64{"abc"}.GetValue(), 0x44BC2CF5AD770999ull);
    }

    TEST_F(Hash_Xxh, DefaultConstructorIsZero)
    {
        constexpr AZ::Hash::Xxh32 x32;
        constexpr AZ::Hash::Xxh64 x64;
        static_assert(x32.GetValue() == 0 && x64.GetValue() == 0);
        EXPECT_EQ(x32.GetValue(), 0u);
        EXPECT_EQ(x64.GetValue(), 0u);
    }

    TEST_F(Hash_Xxh, RawValueConstructorAndConversion)
    {
        constexpr AZ::Hash::Xxh32 x32{0x12345678u};
        constexpr AZ::Hash::Xxh64 x64{0x123456789ABCDEF0ull};
        static_assert(static_cast<u32>(x32) == 0x12345678u);
        static_assert(static_cast<u64>(x64) == 0x123456789ABCDEF0ull);
        EXPECT_EQ(static_cast<u32>(x32), 0x12345678u);
        EXPECT_EQ(static_cast<u64>(x64), 0x123456789ABCDEF0ull);
    }

    TEST_F(Hash_Xxh, CompileTimeMatchesRuntime)
    {
        const AZStd::string str{"O3DE hashing helpers via xxHash"};
        {
            constexpr AZ::Hash::Xxh64 compileHash{"O3DE hashing helpers via xxHash"};
            const AZ::Hash::Xxh64 runtimeHash{str};
            EXPECT_EQ(compileHash.GetValue(), runtimeHash.GetValue());
        }
        {
            constexpr AZ::Hash::Xxh32 compileHash{"O3DE hashing helpers via xxHash"};
            const AZ::Hash::Xxh32 runtimeHash{str};
            EXPECT_EQ(compileHash.GetValue(), runtimeHash.GetValue());
        }
    }

    TEST_F(Hash_Xxh, SeededDiffersFromUnseeded)
    {
        EXPECT_NE((AZ::Hash::Xxh32{"seeded", 0x1234u}), AZ::Hash::Xxh32{"seeded"});
        EXPECT_NE((AZ::Hash::Xxh64{"seeded", 0x1234ull}), AZ::Hash::Xxh64{"seeded"});
    }

    TEST_F(Hash_Xxh, ComparisonAndBoolOperators)
    {
        constexpr AZ::Hash::Xxh64 a{"A"};
        constexpr AZ::Hash::Xxh64 b{"B"};
        constexpr AZ::Hash::Xxh64 aDup{"A"};
        static_assert(a == aDup);
        static_assert(a != b);
        static_assert((a < b) || (a > b));
        EXPECT_EQ(a, aDup);
        EXPECT_NE(a, b);

        constexpr AZ::Hash::Xxh64 zero;
        static_assert(!zero);
        static_assert(!!a);
        EXPECT_FALSE(static_cast<bool>(zero));
        EXPECT_TRUE(static_cast<bool>(a));
    }

    TEST_F(Hash_Xxh, UsableAsUnorderedMapKey)
    {
        AZStd::unordered_map<AZ::Hash::Xxh64, s32> map;
        map[AZ::Hash::Xxh64{"Alpha"}] = 1;
        map[AZ::Hash::Xxh64{"Beta"}] = 2;
        map[AZ::Hash::Xxh64{"Alpha"}] = 42;
        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[AZ::Hash::Xxh64{"Alpha"}], 42);

        constexpr AZStd::hash<AZ::Hash::Xxh64> hasher;
        EXPECT_EQ(hasher(AZ::Hash::Xxh64{"Alpha"}), AZ::Hash::Xxh64{"Alpha"}.GetValue());
    }
} // namespace UnitTest
