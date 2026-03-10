/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Fnv.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/UnitTest/TestTypes.h>

using namespace AZ;

#define AS_BYTE(c) (static_cast<AZStd::byte>(c))

namespace UnitTest
{
    class Hash_Fnv : public LeakDetectionFixture
    {
    };

    // ReSharper disable CppVariableCanBeMadeConstexpr
    TEST_F(Hash_Fnv, EmptyStringReturnsOffsetBasis)
    {
        static_assert(
            AZ::Hash::Fnv32{AZStd::string_view{}} == AZ::Hash::Fnv32::OffsetBasis,
            "Fnv32 of empty string must equal the offset basis");
        EXPECT_EQ(AZ::Hash::Fnv32{AZStd::string_view{}}, AZ::Hash::Fnv32::OffsetBasis);

        static_assert(
            AZ::Hash::Fnv64{AZStd::string_view{}} == AZ::Hash::Fnv64::OffsetBasis,
            "Fnv64 of empty string must equal the offset basis");
        EXPECT_EQ(AZ::Hash::Fnv64{AZStd::string_view{}}, AZ::Hash::Fnv64::OffsetBasis);
    }

    TEST_F(Hash_Fnv, CompileTimeMatchesRuntime)
    {
        const AZStd::string str{"O3DE"};

        {
            constexpr u32 compileHash = AZ::Hash::Fnv32{"O3DE"};
            const u32 runtimeHash = AZ::Hash::Fnv32{str};
            EXPECT_EQ(compileHash, runtimeHash);
        }

        {
            constexpr u64 compileHash = AZ::Hash::Fnv64{"O3DE"};
            const u64 runtimeHash = AZ::Hash::Fnv64{str};
            EXPECT_EQ(compileHash, runtimeHash);
        }
    }

    TEST_F(Hash_Fnv, DifferentInputsProduceDifferentHashes)
    {
        EXPECT_NE(AZ::Hash::Fnv32{"ABCDEF"}, AZ::Hash::Fnv32{"AB"});
        EXPECT_NE(AZ::Hash::Fnv32{"ABC"}, AZ::Hash::Fnv32{"ABD"});
        EXPECT_NE(AZ::Hash::Fnv32{"ABC"}, AZ::Hash::Fnv32{"abc"});
        EXPECT_NE(AZ::Hash::Fnv32{""}, AZ::Hash::Fnv32{"A"});

        EXPECT_NE(AZ::Hash::Fnv64{"ABCDEF"}, AZ::Hash::Fnv64{"AB"});
        EXPECT_NE(AZ::Hash::Fnv64{"ABC"}, AZ::Hash::Fnv64{"ABD"});
        EXPECT_NE(AZ::Hash::Fnv64{"ABC"}, AZ::Hash::Fnv64{"abc"});
        EXPECT_NE(AZ::Hash::Fnv64{""}, AZ::Hash::Fnv64{"A"});
    }

    TEST_F(Hash_Fnv, ProduceDifferentSizedResults)
    {
        // The lower 32 bits of the 64-bit hash should not coincidentally equal the 32-bit hash
        constexpr u32 hash32 = AZ::Hash::Fnv32{"Test"};
        constexpr u64 hash64 = AZ::Hash::Fnv64{"Test"};
        EXPECT_NE(static_cast<u32>(hash64 & 0xFFFFFFFF), hash32);
    }

    TEST_F(Hash_Fnv, LongStringDoesNotCrashAndIsConsistent)
    {
        AZStd::string longStr;
        for (s32 i = 0; i < 10000; ++i)
        {
            longStr += static_cast<char>('A' + (i % 26));
        }

        {
            const u32 hashA = AZ::Hash::Fnv32{longStr};
            const u32 hashB = AZ::Hash::Fnv32{longStr};
            EXPECT_EQ(hashA, hashB);
        }

        {
            const u64 hashA = AZ::Hash::Fnv64{longStr};
            const u64 hashB = AZ::Hash::Fnv64{longStr};
            EXPECT_EQ(hashA, hashB);
        }
    }

    TEST_F(Hash_Fnv, EmptyDataReturnsOffsetBasis)
    {
        static_assert(
            AZ::Hash::Fnv32{AZStd::span<const AZStd::byte>{}} == AZ::Hash::Fnv32::OffsetBasis,
            "FNV-1a 32 of empty byte span must equal offset basis");
        EXPECT_EQ(AZ::Hash::Fnv32{AZStd::span<const AZStd::byte>{}}, AZ::Hash::Fnv32::OffsetBasis);

        static_assert(
            AZ::Hash::Fnv64{AZStd::span<const AZStd::byte>{}} == AZ::Hash::Fnv64::OffsetBasis,
            "FNV-1a 64 of empty byte span must equal offset basis");
        EXPECT_EQ(AZ::Hash::Fnv64{AZStd::span<const AZStd::byte>{}}, AZ::Hash::Fnv64::OffsetBasis);
    }

    TEST_F(Hash_Fnv, SpanMatchesStringViewForAsciiData)
    {
        constexpr AZStd::byte data[] = {
            AS_BYTE('A'),
            AS_BYTE('B'),
            AS_BYTE('C'),
        };

        {
            constexpr u32 byteHash = AZ::Hash::Fnv32{data};
            constexpr u32 stringHash = AZ::Hash::Fnv32{"ABC"};
            static_assert(byteHash == stringHash, "Byte span hash of ASCII data should match string hash");
            EXPECT_EQ(byteHash, stringHash);
        }

        {
            constexpr u64 byteHash = AZ::Hash::Fnv64{data};
            constexpr u64 stringHash = AZ::Hash::Fnv64{"ABC"};
            static_assert(byteHash == stringHash, "Byte span hash of ASCII data should match string hash");
            EXPECT_EQ(byteHash, stringHash);
        }
    }

    TEST_F(Hash_Fnv, BinaryDataWithNs)
    {
        // Hash data that contains n bytes must not stop early
        const AZStd::byte data[] = {
            AS_BYTE(0x00),
            AS_BYTE(0xFF),
            AS_BYTE(0x42),
            AS_BYTE(0x00),
            AS_BYTE(0x13),
        };

        {
            const u32 hash = AZ::Hash::Fnv32{data};
            EXPECT_NE(hash, AZ::Hash::Fnv32::OffsetBasis);
        }

        {
            const u64 hash = AZ::Hash::Fnv64{data};
            EXPECT_NE(hash, AZ::Hash::Fnv64::OffsetBasis);
        }
    }

    TEST_F(Hash_Fnv, RuntimeVector)
    {
        // Verify the span overload works with runtime-constructed containers
        AZStd::vector<AZStd::byte> data;
        data.push_back(AS_BYTE('H'));
        data.push_back(AS_BYTE('e'));
        data.push_back(AS_BYTE('l'));
        data.push_back(AS_BYTE('l'));
        data.push_back(AS_BYTE('o'));
        data.push_back(AS_BYTE('!'));

        {
            const u32 hash = AZ::Hash::Fnv32{data};
            EXPECT_EQ(hash, AZ::Hash::Fnv32{"Hello!"});
        }

        {
            const u64 hash = AZ::Hash::Fnv64{data};
            EXPECT_EQ(hash, AZ::Hash::Fnv64{"Hello!"});
        }
    }

    TEST_F(Hash_Fnv, ConstexprArray)
    {
        constexpr AZStd::array data = {
            AS_BYTE('A'),
            AS_BYTE('B'),
            AS_BYTE('C'),
        };

        {
            constexpr u32 hash = AZ::Hash::Fnv32{data};
            constexpr u32 expected = AZ::Hash::Fnv32{"ABC"};
            static_assert(hash == expected, "Byte span from array should match string hash");
            EXPECT_EQ(hash, expected);
        }

        {
            constexpr u64 hash = AZ::Hash::Fnv64{data};
            constexpr u64 expected = AZ::Hash::Fnv64{"ABC"};
            static_assert(hash == expected, "Byte span from array should match string hash");
            EXPECT_EQ(hash, expected);
        }
    }

    TEST_F(Hash_Fnv, DefaultConstructorIsZero)
    {
        {
            constexpr AZ::Hash::Fnv32 hash;
            static_assert(hash.GetValue() == 0, "Default constructed Fnv32 should be 0");
            EXPECT_EQ(hash.GetValue(), 0);
        }

        {
            constexpr AZ::Hash::Fnv64 hash;
            static_assert(hash.GetValue() == 0, "Default constructed Fnv64 should be 0");
            EXPECT_EQ(hash.GetValue(), 0);
        }
    }

    TEST_F(Hash_Fnv, RawValueConstructor)
    {
        {
            constexpr u32 raw = 0xDEADBEEF;
            constexpr AZ::Hash::Fnv32 hash{raw};
            static_assert(hash.GetValue() == raw);
            static_assert(static_cast<u32>(hash) == raw);
            EXPECT_EQ(hash.GetValue(), raw);
            EXPECT_EQ(static_cast<u32>(hash), raw);
        }

        {
            constexpr u64 raw = 0xDEADBEEF'DEADBEEF;
            constexpr AZ::Hash::Fnv64 hash{raw};
            static_assert(hash.GetValue() == raw);
            static_assert(static_cast<u64>(hash) == raw);
            EXPECT_EQ(hash.GetValue(), raw);
            EXPECT_EQ(static_cast<u64>(hash), raw);
        }
    }

    TEST_F(Hash_Fnv, StringConstructor)
    {
        {
            // Compile-time from string literal
            constexpr AZ::Hash::Fnv32 hash{"Hello!"};
            static_assert(hash.GetValue() == 0xAA21C9DE);
            EXPECT_EQ(hash.GetValue(), 0xAA21C9DE);

            // Runtime from string view
            const AZStd::string str{"Hello!"};
            const AZ::Hash::Fnv32 runtimeHash{str};
            EXPECT_EQ(runtimeHash.GetValue(), hash.GetValue());
        }

        {
            constexpr AZ::Hash::Fnv64 hash{"Hello!"};
            static_assert(hash.GetValue() == 0x9224FCE07C59FABE);
            EXPECT_EQ(hash.GetValue(), 0x9224FCE07C59FABE);

            const AZStd::string str{"Hello!"};
            const AZ::Hash::Fnv64 runtimeHash{str};
            EXPECT_EQ(runtimeHash.GetValue(), hash.GetValue());
        }
    }

    TEST_F(Hash_Fnv, ComparisonOperators)
    {
        {
            constexpr AZ::Hash::Fnv32 a{"A"};
            constexpr AZ::Hash::Fnv32 b{"B"};
            constexpr AZ::Hash::Fnv32 aDup{"A"};

            static_assert(a == aDup);
            static_assert(a != b);
            static_assert((a < b) || (a > b));
            static_assert(a <= aDup);
            static_assert(a >= aDup);

            EXPECT_EQ(a, aDup);
            EXPECT_NE(a, b);
            EXPECT_TRUE((a < b) || (a > b));
            EXPECT_LE(a, aDup);
            EXPECT_GE(a, aDup);
        }

        {
            constexpr AZ::Hash::Fnv64 a{"A"};
            constexpr AZ::Hash::Fnv64 b{"B"};
            constexpr AZ::Hash::Fnv64 aDup{"A"};

            static_assert(a == aDup);
            static_assert(a != b);
            static_assert((a < b) || (a > b));
            static_assert(a <= aDup);
            static_assert(a >= aDup);

            EXPECT_EQ(a, aDup);
            EXPECT_NE(a, b);
            EXPECT_TRUE((a < b) || (a > b));
            EXPECT_LE(a, aDup);
            EXPECT_GE(a, aDup);
        }
    }

    TEST_F(Hash_Fnv, BoolOperator)
    {
        {
            constexpr AZ::Hash::Fnv32 zero;
            constexpr AZ::Hash::Fnv32 nonZero{"X"};
            static_assert(!zero, "Default (zero) hash should be falsy");
            static_assert(!!nonZero, "Non-zero hash should be truthy");
            EXPECT_FALSE(static_cast<bool>(zero));
            EXPECT_TRUE(static_cast<bool>(nonZero));
        }

        {
            constexpr AZ::Hash::Fnv64 zero;
            constexpr AZ::Hash::Fnv64 nonZero{"X"};
            static_assert(!zero, "Default (zero) hash should be falsy");
            static_assert(!!nonZero, "Non-zero hash should be truthy");
            EXPECT_FALSE(static_cast<bool>(zero));
            EXPECT_TRUE(static_cast<bool>(nonZero));
        }
    }

    // ReSharper disable CppRedundantCastExpression
    TEST_F(Hash_Fnv, ExplicitIntegerConversion)
    {
        {
            constexpr AZ::Hash::Fnv32 hash{0x12345678};
            constexpr u32 value = static_cast<u32>(hash);
            static_assert(value == 0x12345678);
            EXPECT_EQ(value, 0x12345678);
        }

        {
            constexpr AZ::Hash::Fnv64 hash{0x123456789ABCDEF0};
            constexpr u64 value = static_cast<u64>(hash);
            static_assert(value == 0x123456789ABCDEF0);
            EXPECT_EQ(value, 0x123456789ABCDEF0);
        }
    }

    TEST_F(Hash_Fnv, StdHashReturnsExpectedValue)
    {
        {
            constexpr AZ::Hash::Fnv32 hash{"Hello!"};
            const AZStd::hash<AZ::Hash::Fnv32> hasher;
            EXPECT_EQ(hasher(hash), static_cast<size_t>(hash.GetValue()));
        }

        {
            constexpr AZ::Hash::Fnv64 hash{"Hello!"};
            const AZStd::hash<AZ::Hash::Fnv64> hasher;
            EXPECT_EQ(hasher(hash), hash.GetValue());
        }
    }

    TEST_F(Hash_Fnv, CanonicalTestVectors)
    {
        // Single-character vectors from the FNV reference
        EXPECT_EQ(AZ::Hash::Fnv32{""}, 0x811C9DC5);
        EXPECT_EQ(AZ::Hash::Fnv32{"A"}, 0xC40BF6CC);
        EXPECT_EQ(AZ::Hash::Fnv32{"B"}, 0xC70BFB85);
        EXPECT_EQ(AZ::Hash::Fnv32{"C"}, 0xC60BF9F2);
        EXPECT_EQ(AZ::Hash::Fnv32{"D"}, 0xC10BF213);
        EXPECT_EQ(AZ::Hash::Fnv32{"E"}, 0xC00BF080);
        EXPECT_EQ(AZ::Hash::Fnv32{"F"}, 0xC30BF539);

        EXPECT_EQ(AZ::Hash::Fnv64{""}, 0xCBF29CE484222325);
        EXPECT_EQ(AZ::Hash::Fnv64{"A"}, 0xAF63FC4C860222EC);
        EXPECT_EQ(AZ::Hash::Fnv64{"B"}, 0xAF63FF4C86022805);
        EXPECT_EQ(AZ::Hash::Fnv64{"C"}, 0xAF63FE4C86022652);
        EXPECT_EQ(AZ::Hash::Fnv64{"D"}, 0xAF63F94C86021DD3);
        EXPECT_EQ(AZ::Hash::Fnv64{"E"}, 0xAF63F84C86021C20);
        EXPECT_EQ(AZ::Hash::Fnv64{"F"}, 0xAF63FB4C86022139);

        // Multi-character progressive vectors
        EXPECT_EQ(AZ::Hash::Fnv32{"H"}, 0xCD0C04F7);
        EXPECT_EQ(AZ::Hash::Fnv32{"Ho"}, 0x61EB3B48);
        EXPECT_EQ(AZ::Hash::Fnv32{"How"}, 0x644E442D);
        EXPECT_EQ(AZ::Hash::Fnv32{"Howd"}, 0x30357EEB);
        EXPECT_EQ(AZ::Hash::Fnv32{"Howdy"}, 0x76363FD6);

        EXPECT_EQ(AZ::Hash::Fnv64{"H"}, 0xAF64054C86023237);
        EXPECT_EQ(AZ::Hash::Fnv64{"Ho"}, 0x9275907B5BB8B88);
        EXPECT_EQ(AZ::Hash::Fnv64{"How"}, 0x49684719CDAEE24D);
        EXPECT_EQ(AZ::Hash::Fnv64{"Howd"}, 0x6B12F9D8802A4BAB);
        EXPECT_EQ(AZ::Hash::Fnv64{"Howdy"}, 0x1B8A5CE1C7DED5D6);
    }

    TEST_F(Hash_Fnv, UsableAsUnorderedMapKey)
    {
        {
            AZStd::unordered_map<AZ::Hash::Fnv32, s32> map;
            const AZ::Hash::Fnv32 keyA{"Alpha"};
            const AZ::Hash::Fnv32 keyB{"Beta"};

            map[keyA] = 1;
            map[keyB] = 2;

            EXPECT_EQ(map.size(), 2u);
            EXPECT_EQ(map[keyA], 1);
            EXPECT_EQ(map[keyB], 2);

            // Inserting with an equivalent key overwrites, not duplicates
            const AZ::Hash::Fnv32 duplicateKey{"Alpha"};
            map[duplicateKey] = 42;
            EXPECT_EQ(map.size(), 2u);
            EXPECT_EQ(map[keyA], 42);
        }

        {
            AZStd::unordered_map<AZ::Hash::Fnv64, s32> map;
            const AZ::Hash::Fnv64 keyA{"Alpha"};
            const AZ::Hash::Fnv64 keyB{"Beta"};

            map[keyA] = 1;
            map[keyB] = 2;

            EXPECT_EQ(map.size(), 2u);
            EXPECT_EQ(map[keyA], 1);
            EXPECT_EQ(map[keyB], 2);

            // Inserting with an equivalent key overwrites, not duplicates
            const AZ::Hash::Fnv64 duplicateKey{"Alpha"};
            map[duplicateKey] = 42;
            EXPECT_EQ(map.size(), 2u);
            EXPECT_EQ(map[keyA], 42);
        }
    }
} // namespace UnitTest

#undef AS_BYTE
