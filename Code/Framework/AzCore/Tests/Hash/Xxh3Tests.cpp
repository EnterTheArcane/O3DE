/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Xxh3.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/UnitTest/TestTypes.h>

#include "XxhReferenceVectors.h"

using namespace AZ;

namespace UnitTest
{
    class Hash_Xxh3 : public LeakDetectionFixture
    {
    };

    // Compile-time reference hashes build the reference buffer and hash it entirely through the scalar path.
    // The static assertions and runtime table both use the same reference vectors.
    // Together they verify agreement between the constexpr scalar and runtime SIMD paths.
    template <AZStd::size_t N>
    consteval u64 ConstXxh3(const u64 seed)
    {
        AZStd::array<AZStd::byte, N> buffer{};
        u64 gen = XxhSeedPrime32;
        for (AZStd::size_t i = 0; i < N; ++i)
        {
            buffer[i] = static_cast<AZStd::byte>(static_cast<u8>(gen >> 56));
            gen *= 11400714785074694797ull;
        }
        return AZ::Hash::Xxh3{AZStd::span<const AZStd::byte>{buffer.data(), buffer.size()}, seed}.GetValue();
    }

    template <AZStd::size_t N>
    consteval AZ::Hash::Xxh128 ConstXxh128(const u64 seed)
    {
        AZStd::array<AZStd::byte, N> buffer{};
        u64 gen = XxhSeedPrime32;
        for (AZStd::size_t i = 0; i < N; ++i)
        {
            buffer[i] = static_cast<AZStd::byte>(static_cast<u8>(gen >> 56));
            gen *= 11400714785074694797ull;
        }
        return AZ::Hash::Xxh128{AZStd::span<const AZStd::byte>{buffer.data(), buffer.size()}, seed};
    }

    TEST_F(Hash_Xxh3, Xxh3_ReferenceVectors_Runtime)
    {
        // Lengths > 240 exercise the SIMD long path through the runtime dispatcher.
        for (const Xxh3Vector& v : Xxh3Vectors)
        {
            const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(v.m_len);
            const AZ::Hash::Xxh3 hash{AZStd::span<const AZStd::byte>{buffer.data(), v.m_len}, v.m_seed};
            EXPECT_EQ(hash.GetValue(), v.m_expected) << "XXH3-64 len=" << v.m_len << " seed=" << v.m_seed;
        }
    }

    TEST_F(Hash_Xxh3, Xxh128_ReferenceVectors_Runtime)
    {
        for (const Xxh128Vector& v : Xxh128Vectors)
        {
            const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(v.m_len);
            const AZ::Hash::Xxh128 hash{AZStd::span<const AZStd::byte>{buffer.data(), v.m_len}, v.m_seed};
            EXPECT_EQ(hash.GetLow(), v.m_low) << "XXH3-128 low len=" << v.m_len << " seed=" << v.m_seed;
            EXPECT_EQ(hash.GetHigh(), v.m_high) << "XXH3-128 high len=" << v.m_len << " seed=" << v.m_seed;
        }
    }

    TEST_F(Hash_Xxh3, ReferenceVectors_CompileTime)
    {
        // Empty via the string-literal path.
        static_assert(AZ::Hash::Xxh3{""}.GetValue() == 0x2D06800538D394C2ull);
        static_assert(AZ::Hash::Xxh128{""}.GetLow() == 0x6001C324468D497Full);
        static_assert(AZ::Hash::Xxh128{""}.GetHigh() == 0x99AA06D3014798D8ull);

        // One representative per bucket, including the >240-byte long path, hashed at compile time.
        static_assert(ConstXxh3<1>(0) == 0xC44BDFF4074EECDBull); // 1-3
        static_assert(ConstXxh3<6>(0) == 0x27B56A84CD2D7325ull); // 4-8
        static_assert(ConstXxh3<12>(0) == 0xA713DAF0DFBB77E7ull); // 9-16
        static_assert(ConstXxh3<24>(0) == 0xA3FE70BF9D3510EBull); // 17-128
        static_assert(ConstXxh3<195>(0) == 0xCD94217EE362EC3Aull); // 129-240
        static_assert(ConstXxh3<512>(0) == 0x617E49599013CB6Bull); // long path
        static_assert(ConstXxh3<512>(XxhSeedPrime64) == 0x3CE457DE14C27708ull); // long path, seeded
        static_assert(ConstXxh128<512>(0).GetLow() == 0x617E49599013CB6Bull);
        static_assert(ConstXxh128<512>(0).GetHigh() == 0x18D2D110DCC9BCA1ull);
        SUCCEED();
    }

    TEST_F(Hash_Xxh3, Xxh128LowMatchesXxh3ForLongInputs)
    {
        for (u32 len : {241u, 512u, 2048u})
        {
            const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(len);
            const AZStd::span<const AZStd::byte> span{buffer.data(), len};
            EXPECT_EQ(AZ::Hash::Xxh128{span}.GetLow(), AZ::Hash::Xxh3{span}.GetValue()) << "len=" << len;
        }
    }

    TEST_F(Hash_Xxh3, DefaultConstructorIsZero)
    {
        constexpr AZ::Hash::Xxh3 x64;
        constexpr AZ::Hash::Xxh128 x128;
        static_assert(x64.GetValue() == 0);
        static_assert(x128.GetLow() == 0 && x128.GetHigh() == 0);
        EXPECT_EQ(x64.GetValue(), 0u);
        EXPECT_EQ(x128.GetLow(), 0u);
        EXPECT_EQ(x128.GetHigh(), 0u);
    }

    TEST_F(Hash_Xxh3, RawValueConstructors)
    {
        constexpr AZ::Hash::Xxh3 x64{0xDEADBEEFFEEDFACEull};
        static_assert(static_cast<u64>(x64) == 0xDEADBEEFFEEDFACEull);
        EXPECT_EQ(static_cast<u64>(x64), 0xDEADBEEFFEEDFACEull);

        constexpr AZ::Hash::Xxh128 x128{0x0123456789ABCDEFull, 0xFEDCBA9876543210ull};
        static_assert(x128.GetLow() == 0x0123456789ABCDEFull && x128.GetHigh() == 0xFEDCBA9876543210ull);
        EXPECT_EQ(x128.GetLow(), 0x0123456789ABCDEFull);
        EXPECT_EQ(x128.GetHigh(), 0xFEDCBA9876543210ull);
    }

    TEST_F(Hash_Xxh3, CompileTimeMatchesRuntimeAcrossBuckets)
    {
        // For each representative length, the runtime (SIMD-dispatched) hash of the reference buffer
        // must equal the compile-time (scalar) hash of the same bytes. Covers a short, mid and long bucket.
        // The long case (512) is the end-to-end proof that the SIMD path equals the scalar path.
        auto checkLen = [](const u32 len, const u64 compileHash)
        {
            const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(len);
            const AZ::Hash::Xxh3 runtimeHash{AZStd::span<const AZStd::byte>{buffer.data(), len}};
            EXPECT_EQ(runtimeHash.GetValue(), compileHash) << "len=" << len;
        };
        checkLen(12, ConstXxh3<12>(0)); // 9-16 scalar bucket
        checkLen(195, ConstXxh3<195>(0)); // 129-240 scalar bucket
        checkLen(512, ConstXxh3<512>(0)); // long path: runtime SIMD vs compile-time scalar
    }

    TEST_F(Hash_Xxh3, SeededDiffersFromUnseeded)
    {
        const AZStd::vector<AZStd::byte> buffer = MakeReferenceBuffer(300);
        const AZStd::span<const AZStd::byte> span{buffer.data(), 300};
        EXPECT_NE((AZ::Hash::Xxh3{span, 0x12345ull}.GetValue()), AZ::Hash::Xxh3{span}.GetValue());
        EXPECT_NE((AZ::Hash::Xxh128{span, 0x12345ull}), AZ::Hash::Xxh128{span});
    }

    TEST_F(Hash_Xxh3, ComparisonAndBoolOperators)
    {
        constexpr AZ::Hash::Xxh3 a{"A"};
        constexpr AZ::Hash::Xxh3 b{"B"};
        constexpr AZ::Hash::Xxh3 aDup{"A"};
        static_assert(a == aDup);
        static_assert(a != b);
        static_assert((a < b) || (a > b));
        EXPECT_EQ(a, aDup);
        EXPECT_NE(a, b);

        constexpr AZ::Hash::Xxh3 zero;
        static_assert(!zero && !!a);
        EXPECT_FALSE(static_cast<bool>(zero));

        constexpr AZ::Hash::Xxh128 c128;
        constexpr AZ::Hash::Xxh128 nz128{"X"};
        static_assert(!c128 && !!nz128);
        EXPECT_TRUE(static_cast<bool>(nz128));
    }

    TEST_F(Hash_Xxh3, UsableAsUnorderedMapKey)
    {
        AZStd::unordered_map<AZ::Hash::Xxh3, s32> map;
        map[AZ::Hash::Xxh3{"Alpha"}] = 1;
        map[AZ::Hash::Xxh3{"Beta"}] = 2;
        map[AZ::Hash::Xxh3{"Alpha"}] = 42;
        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[AZ::Hash::Xxh3{"Alpha"}], 42);

        AZStd::unordered_map<AZ::Hash::Xxh128, s32> map128;
        map128[AZ::Hash::Xxh128{"Alpha"}] = 7;
        EXPECT_EQ(map128[AZ::Hash::Xxh128{"Alpha"}], 7);
    }
} // namespace UnitTest
