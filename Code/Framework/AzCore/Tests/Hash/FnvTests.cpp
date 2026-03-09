/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Fnv.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/UnitTest/TestTypes.h>

using namespace AZ;

namespace UnitTest
{
    // Reference values computed with the canonical FNV-1a specification.
    // http://www.isthe.com/chongo/tech/comp/fnv

    class Hash_Fnv : public LeakDetectionFixture
    {
    };

    TEST_F(Hash_Fnv, Fnv1a_32_EmptyString_ReturnsOffsetBasis)
    {
        static_assert(
            AZ::Hash::Fnv1a_32(AZStd::string_view{}) == AZ::Hash::Fnv1a32_OffsetBasis,
            "FNV-1a-32 of empty string must equal the offset basis");
        EXPECT_EQ(AZ::Hash::Fnv1a_32(AZStd::string_view{}), AZ::Hash::Fnv1a32_OffsetBasis);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_EmptyString_ReturnsOffsetBasis)
    {
        static_assert(
            AZ::Hash::Fnv1a_64(AZStd::string_view{}) == AZ::Hash::Fnv1a64_OffsetBasis,
            "FNV-1a-64 of empty string must equal the offset basis");
        EXPECT_EQ(AZ::Hash::Fnv1a_64(AZStd::string_view{}), AZ::Hash::Fnv1a64_OffsetBasis);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_SingleChar)
    {
        static_assert(AZ::Hash::Fnv1a_32("a") == 0xe40c292cu);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("a"), 0xe40c292cu);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_SingleChar)
    {
        static_assert(AZ::Hash::Fnv1a_64("a") == 0xaf63dc4c8601ec8cULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("a"), 0xaf63dc4c8601ec8cULL);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_Foobar)
    {
        static_assert(AZ::Hash::Fnv1a_32("foobar") == 0xbf9cf968u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("foobar"), 0xbf9cf968u);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_Foobar)
    {
        static_assert(AZ::Hash::Fnv1a_64("foobar") == 0x85944171f73967e8ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("foobar"), 0x85944171f73967e8ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_HelloWorld)
    {
        static_assert(AZ::Hash::Fnv1a_32("Hello, World!") == 0x5aecf734u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("Hello, World!"), 0x5aecf734u);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_HelloWorld)
    {
        static_assert(AZ::Hash::Fnv1a_64("Hello, World!") == 0x6ef05bd7cc857c54ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("Hello, World!"), 0x6ef05bd7cc857c54ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_CompileTimeMatchesRuntime)
    {
        constexpr u32 compileTime = AZ::Hash::Fnv1a_32("consistency");
        const AZStd::string runtimeStr("consistency");
        const u32 runTime = AZ::Hash::Fnv1a_32(AZStd::string_view{runtimeStr});
        EXPECT_EQ(compileTime, runTime);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_CompileTimeMatchesRuntime)
    {
        constexpr u64 compileTime = AZ::Hash::Fnv1a_64("consistency");
        const AZStd::string runtimeStr("consistency");
        const u64 runTime = AZ::Hash::Fnv1a_64(AZStd::string_view{runtimeStr});
        EXPECT_EQ(compileTime, runTime);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_DifferentInputs_ProduceDifferentHashes)
    {
        EXPECT_NE(AZ::Hash::Fnv1a_32("abc"), AZ::Hash::Fnv1a_32("abd"));
        EXPECT_NE(AZ::Hash::Fnv1a_32("abc"), AZ::Hash::Fnv1a_32("ABC"));
        EXPECT_NE(AZ::Hash::Fnv1a_32(""), AZ::Hash::Fnv1a_32("a"));
    }

    TEST_F(Hash_Fnv, Fnv1a_64_DifferentInputs_ProduceDifferentHashes)
    {
        EXPECT_NE(AZ::Hash::Fnv1a_64("abc"), AZ::Hash::Fnv1a_64("abd"));
        EXPECT_NE(AZ::Hash::Fnv1a_64("abc"), AZ::Hash::Fnv1a_64("ABC"));
        EXPECT_NE(AZ::Hash::Fnv1a_64(""), AZ::Hash::Fnv1a_64("a"));
    }


    TEST_F(Hash_Fnv, Fnv1a_32_ByteSpan_EmptyData_ReturnsOffsetBasis)
    {
        static_assert(
            AZ::Hash::Fnv1a_32(AZStd::span<const AZStd::byte>{}) == AZ::Hash::Fnv1a32_OffsetBasis,
            "FNV-1a 32 of empty byte span must equal offset basis");
        EXPECT_EQ(AZ::Hash::Fnv1a_32(AZStd::span<const AZStd::byte>{}), AZ::Hash::Fnv1a32_OffsetBasis);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_ByteSpan_EmptyData_ReturnsOffsetBasis)
    {
        static_assert(
            AZ::Hash::Fnv1a_64(AZStd::span<const AZStd::byte>{}) == AZ::Hash::Fnv1a64_OffsetBasis,
            "FNV-1a 64 of empty byte span must equal offset basis");
        EXPECT_EQ(AZ::Hash::Fnv1a_64(AZStd::span<const AZStd::byte>{}), AZ::Hash::Fnv1a64_OffsetBasis);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_ByteSpan_MatchesStringView_ForAsciiData)
    {
        constexpr AZStd::byte abcBytes[] = {
            static_cast<AZStd::byte>('a'),
            static_cast<AZStd::byte>('b'),
            static_cast<AZStd::byte>('c'),
        };
        constexpr u32 byteHash = AZ::Hash::Fnv1a_32(abcBytes);
        constexpr u32 stringHash = AZ::Hash::Fnv1a_32("abc");
        static_assert(byteHash == stringHash, "Byte span hash of ASCII data should match string_view hash");
        EXPECT_EQ(byteHash, stringHash);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_ByteSpan_MatchesStringView_ForAsciiData)
    {
        constexpr AZStd::byte abcBytes[] = {
            static_cast<AZStd::byte>('a'),
            static_cast<AZStd::byte>('b'),
            static_cast<AZStd::byte>('c'),
        };
        constexpr u64 byteHash = AZ::Hash::Fnv1a_64(abcBytes);
        constexpr u64 stringHash = AZ::Hash::Fnv1a_64("abc");
        static_assert(byteHash == stringHash, "Byte span hash of ASCII data should match string_view hash");
        EXPECT_EQ(byteHash, stringHash);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_ByteSpan_BinaryData)
    {
        // Hash data that contains null bytes — must not stop early
        const AZStd::byte data[] = {
            static_cast<AZStd::byte>(0x00),
            static_cast<AZStd::byte>(0xFF),
            static_cast<AZStd::byte>(0x42),
            static_cast<AZStd::byte>(0x00),
            static_cast<AZStd::byte>(0x13),
        };
        const u32 hash = AZ::Hash::Fnv1a_32(data);
        // Just verify it doesn't return offset basis (which would mean the nulls truncated processing)
        EXPECT_NE(hash, AZ::Hash::Fnv1a32_OffsetBasis);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_ByteSpan_BinaryData)
    {
        const AZStd::byte data[] = {
            static_cast<AZStd::byte>(0x00),
            static_cast<AZStd::byte>(0xFF),
            static_cast<AZStd::byte>(0x42),
            static_cast<AZStd::byte>(0x00),
            static_cast<AZStd::byte>(0x13),
        };
        const u64 hash = AZ::Hash::Fnv1a_64(data);
        EXPECT_NE(hash, AZ::Hash::Fnv1a64_OffsetBasis);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_ByteSpan_RuntimeVector)
    {
        // Verify the span overload works with runtime-constructed containers
        AZStd::vector<AZStd::byte> data;
        data.push_back(static_cast<AZStd::byte>('f'));
        data.push_back(static_cast<AZStd::byte>('o'));
        data.push_back(static_cast<AZStd::byte>('o'));
        data.push_back(static_cast<AZStd::byte>('b'));
        data.push_back(static_cast<AZStd::byte>('a'));
        data.push_back(static_cast<AZStd::byte>('r'));
        const u32 hash = AZ::Hash::Fnv1a_32(data);
        EXPECT_EQ(hash, 0xbf9cf968u);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_ByteSpan_RuntimeVector)
    {
        AZStd::vector<AZStd::byte> data;
        data.push_back(static_cast<AZStd::byte>('f'));
        data.push_back(static_cast<AZStd::byte>('o'));
        data.push_back(static_cast<AZStd::byte>('o'));
        data.push_back(static_cast<AZStd::byte>('b'));
        data.push_back(static_cast<AZStd::byte>('a'));
        data.push_back(static_cast<AZStd::byte>('r'));
        const u64 hash = AZ::Hash::Fnv1a_64(data);
        EXPECT_EQ(hash, 0x85944171f73967e8ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_ByteSpan_ConstexprArray)
    {
        constexpr AZStd::array data = {
            static_cast<AZStd::byte>('a'),
            static_cast<AZStd::byte>('b'),
            static_cast<AZStd::byte>('c'),
        };
        constexpr u32 hash = AZ::Hash::Fnv1a_32(data);
        constexpr u32 expected = AZ::Hash::Fnv1a_32("abc");
        static_assert(hash == expected, "Byte span from array should match string_view hash");
        EXPECT_EQ(hash, expected);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_ByteSpan_ConstexprArray)
    {
        constexpr AZStd::array data = {
            static_cast<AZStd::byte>('a'),
            static_cast<AZStd::byte>('b'),
            static_cast<AZStd::byte>('c'),
        };
        constexpr u64 hash = AZ::Hash::Fnv1a_64(data);
        constexpr u64 expected = AZ::Hash::Fnv1a_64("abc");
        static_assert(hash == expected, "Byte span from array should match string_view hash");
        EXPECT_EQ(hash, expected);
    }

    TEST_F(Hash_Fnv, Fnv1a_32And64_ProduceDifferentSizedResults)
    {
        constexpr u32 hash32 = AZ::Hash::Fnv1a_32("test");
        constexpr u64 hash64 = AZ::Hash::Fnv1a_64("test");
        EXPECT_NE(static_cast<u32>(hash64 & 0xFFFFFFFF), hash32);
    }

    TEST_F(Hash_Fnv, Fnv1a_LongString_DoesNotCrashAndIsConsistent)
    {
        // Build a long string at runtime
        AZStd::string longStr;
        for (int i = 0; i < 10000; ++i)
        {
            longStr += static_cast<char>('A' + (i % 26));
        }
        const u32 hash32a = AZ::Hash::Fnv1a_32(AZStd::string_view{longStr});
        const u32 hash32b = AZ::Hash::Fnv1a_32(AZStd::string_view{longStr});
        EXPECT_EQ(hash32a, hash32b);

        const u64 hash64a = AZ::Hash::Fnv1a_64(AZStd::string_view{longStr});
        const u64 hash64b = AZ::Hash::Fnv1a_64(AZStd::string_view{longStr});
        EXPECT_EQ(hash64a, hash64b);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_DefaultConstructor_IsZero)
    {
        constexpr AZ::Hash::Fnv1a32 h;
        static_assert(h.GetValue() == 0u, "Default constructed Fnv1a32Hash should be 0");
        EXPECT_EQ(h.GetValue(), 0u);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_RawValueConstructor)
    {
        constexpr AZ::Hash::Fnv1a32 h(0xbf9cf968u);
        static_assert(h.GetValue() == 0xbf9cf968u);
        static_assert(static_cast<u32>(h) == 0xbf9cf968u);
        EXPECT_EQ(h.GetValue(), 0xbf9cf968u);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_StringViewConstructor_MatchesFreeFunction)
    {
        constexpr AZ::Hash::Fnv1a32 h("foobar");
        constexpr u32 expected = AZ::Hash::Fnv1a_32("foobar");
        static_assert(h.GetValue() == expected);
        static_assert(h == AZ::Hash::Fnv1a32(expected));

        const AZStd::string runtimeStr("foobar");
        const AZ::Hash::Fnv1a32 runtimeH(AZStd::string_view{runtimeStr});
        EXPECT_EQ(runtimeH.GetValue(), 0xbf9cf968u);
        EXPECT_EQ(runtimeH.GetValue(), h.GetValue());
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_ByteSpanConstructor_MatchesFreeFunction)
    {
        constexpr AZStd::byte data[] = {
            static_cast<AZStd::byte>('a'),
            static_cast<AZStd::byte>('b'),
            static_cast<AZStd::byte>('c'),
        };
        constexpr AZ::Hash::Fnv1a32 h{data};
        constexpr u32 expected = AZ::Hash::Fnv1a_32(data);
        static_assert(h.GetValue() == expected);
        EXPECT_EQ(h.GetValue(), expected);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_ComparisonOperators)
    {
        constexpr AZ::Hash::Fnv1a32 a("a");
        constexpr AZ::Hash::Fnv1a32 b("b");
        constexpr AZ::Hash::Fnv1a32 a2("a");

        static_assert(a == a2);
        static_assert(a != b);
        static_assert((a < b) || (a > b));
        static_assert(a <= a2);
        static_assert(a >= a2);

        EXPECT_EQ(a, a2);
        EXPECT_NE(a, b);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_BoolOperator)
    {
        constexpr AZ::Hash::Fnv1a32 zero;
        constexpr AZ::Hash::Fnv1a32 nonZero("x");
        static_assert(!zero, "Default (zero) hash should be falsy via operator!");
        static_assert(!!nonZero, "Non-zero hash should be truthy");
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_ExplicitConversion)
    {
        constexpr AZ::Hash::Fnv1a32 h(0x12345678u);
        constexpr u32 v = static_cast<u32>(h);
        static_assert(v == 0x12345678u);
        EXPECT_EQ(v, 0x12345678u);
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_DefaultConstructor_IsZero)
    {
        constexpr AZ::Hash::Fnv1a64 h;
        static_assert(h.GetValue() == 0ULL, "Default constructed Fnv1a64Hash should be 0");
        EXPECT_EQ(h.GetValue(), 0ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_RawValueConstructor)
    {
        constexpr AZ::Hash::Fnv1a64 h(0x85944171f73967e8ULL);
        static_assert(h.GetValue() == 0x85944171f73967e8ULL);
        static_assert(static_cast<u64>(h) == 0x85944171f73967e8ULL);
        EXPECT_EQ(h.GetValue(), 0x85944171f73967e8ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_StringViewConstructor_MatchesFreeFunction)
    {
        constexpr AZ::Hash::Fnv1a64 h("foobar");
        constexpr u64 expected = AZ::Hash::Fnv1a_64("foobar");
        static_assert(h.GetValue() == expected);
        static_assert(h == AZ::Hash::Fnv1a64(expected));

        const AZStd::string runtimeStr("foobar");
        const AZ::Hash::Fnv1a64 runtimeH(AZStd::string_view{runtimeStr});
        EXPECT_EQ(runtimeH.GetValue(), 0x85944171f73967e8ULL);
        EXPECT_EQ(runtimeH.GetValue(), h.GetValue());
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_ByteSpanConstructor_MatchesFreeFunction)
    {
        constexpr AZStd::byte data[] = {
            static_cast<AZStd::byte>('a'),
            static_cast<AZStd::byte>('b'),
            static_cast<AZStd::byte>('c'),
        };
        constexpr AZ::Hash::Fnv1a64 h{data};
        constexpr u64 expected = AZ::Hash::Fnv1a_64(data);
        static_assert(h.GetValue() == expected);
        EXPECT_EQ(h.GetValue(), expected);
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_ComparisonOperators)
    {
        constexpr AZ::Hash::Fnv1a64 a("a");
        constexpr AZ::Hash::Fnv1a64 b("b");
        constexpr AZ::Hash::Fnv1a64 a2("a");

        static_assert(a == a2);
        static_assert(a != b);
        static_assert((a < b) || (a > b));
        static_assert(a <= a2);
        static_assert(a >= a2);

        EXPECT_EQ(a, a2);
        EXPECT_NE(a, b);
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_BoolOperator)
    {
        constexpr AZ::Hash::Fnv1a64 zero;
        constexpr AZ::Hash::Fnv1a64 nonZero("x");
        static_assert(!zero, "Default (zero) hash should be falsy via operator!");
        static_assert(!!nonZero, "Non-zero hash should be truthy");
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_ExplicitConversion)
    {
        constexpr AZ::Hash::Fnv1a64 h(0x123456789ABCDEF0ULL);
        constexpr u64 v = static_cast<u64>(h);
        static_assert(v == 0x123456789ABCDEF0ULL);
        EXPECT_EQ(v, 0x123456789ABCDEF0ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_AZStdHash_ReturnsExpectedValue)
    {
        constexpr AZ::Hash::Fnv1a32 h("foobar");
        const AZStd::hash<AZ::Hash::Fnv1a32> hasher;
        EXPECT_EQ(hasher(h), static_cast<size_t>(h.GetValue()));
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_AZStdHash_ReturnsExpectedValue)
    {
        constexpr AZ::Hash::Fnv1a64 h("foobar");
        const AZStd::hash<AZ::Hash::Fnv1a64> hasher;
        EXPECT_EQ(hasher(h), h.GetValue());
    }

    TEST_F(Hash_Fnv, Fnv1a_32_CanonicalTestVectors)
    {
        EXPECT_EQ(AZ::Hash::Fnv1a_32(""), 0x811c9dc5u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("a"), 0xe40c292cu);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("b"), 0xe70c2de5u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("c"), 0xe60c2c52u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("d"), 0xe10c2473u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("e"), 0xe00c22e0u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("f"), 0xe30c2799u);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_CanonicalTestVectors)
    {
        EXPECT_EQ(AZ::Hash::Fnv1a_64(""), 0xcbf29ce484222325ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("a"), 0xaf63dc4c8601ec8cULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("b"), 0xaf63df4c8601f1a5ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("c"), 0xaf63de4c8601eff2ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("d"), 0xaf63d94c8601e773ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("e"), 0xaf63d84c8601e5c0ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("f"), 0xaf63db4c8601ead9ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a_32_MultiCharCanonicalVectors)
    {
        EXPECT_EQ(AZ::Hash::Fnv1a_32("fo"), 0x6f631072u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("foo"), 0xa9f37ed7u);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("foob"), 0x3f5076efu);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("fooba"), 0x39aaa18au);
        EXPECT_EQ(AZ::Hash::Fnv1a_32("foobar"), 0xbf9cf968u);
    }

    TEST_F(Hash_Fnv, Fnv1a_64_MultiCharCanonicalVectors)
    {
        EXPECT_EQ(AZ::Hash::Fnv1a_64("fo"), 0x08985907b541d342ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("foo"), 0xdcb27518fed9d577ULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("foob"), 0xdd120e790c2512afULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("fooba"), 0xcac165afa2fef40aULL);
        EXPECT_EQ(AZ::Hash::Fnv1a_64("foobar"), 0x85944171f73967e8ULL);
    }

    TEST_F(Hash_Fnv, Fnv1a32Hash_UsableAsUnorderedMapKey)
    {
        AZStd::unordered_map<AZ::Hash::Fnv1a32, int> map;
        const AZ::Hash::Fnv1a32 keyA("alpha");
        const AZ::Hash::Fnv1a32 keyB("beta");

        map[keyA] = 1;
        map[keyB] = 2;

        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[keyA], 1);
        EXPECT_EQ(map[keyB], 2);

        const AZ::Hash::Fnv1a32 keyA2("alpha");
        map[keyA2] = 42;
        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[keyA], 42);
    }

    TEST_F(Hash_Fnv, Fnv1a64Hash_UsableAsUnorderedMapKey)
    {
        AZStd::unordered_map<AZ::Hash::Fnv1a64, AZStd::string_view> map;
        const AZ::Hash::Fnv1a64 keyA("alpha");
        const AZ::Hash::Fnv1a64 keyB("beta");

        map[keyA] = "first";
        map[keyB] = "second";

        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(map[keyA], "first");
        EXPECT_EQ(map[keyB], "second");
    }
} // namespace UnitTest
