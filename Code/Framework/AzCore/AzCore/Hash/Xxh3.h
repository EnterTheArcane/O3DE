/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Portions of this file independently reimplement the XXH3_64bits and XXH3_128bits algorithms from Yann Collet's xxHash.
// The xxHash reference implementation is distributed under the following BSD 2-Clause license:
//
// xxHash - Extremely Fast Hash algorithm
// Copyright (C) 2012-2023 Yann Collet
//
// BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following disclaimer
//       in the documentation and/or other materials provided with the
//       distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <AzCore/Hash/Internal/ByteReader.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/RTTI/TypeInfoSimple.h>

#include <bit>

namespace AZ
{
    class ReflectContext;
}

namespace AZ::Hash::Private::Xxh3
{
    inline constexpr u32 Prime32_1 = 0x9E3779B1u;
    inline constexpr u32 Prime32_2 = 0x85EBCA77u;
    inline constexpr u32 Prime32_3 = 0xC2B2AE3Du;
    inline constexpr u64 Prime64_1 = 0x9E3779B185EBCA87ull;
    inline constexpr u64 Prime64_2 = 0xC2B2AE3D27D4EB4Full;
    inline constexpr u64 Prime64_3 = 0x165667B19E3779F9ull;
    inline constexpr u64 Prime64_4 = 0x85EBCA77C2B2AE63ull;
    inline constexpr u64 Prime64_5 = 0x27D4EB2F165667C5ull;
    inline constexpr u64 PrimeMx_1 = 0x165667919E3779F9ull;
    inline constexpr u64 PrimeMx_2 = 0x9FB21C651E98DF25ull;

    inline constexpr AZStd::size_t StripeLen = 64;

    inline constexpr AZStd::array<u8, 192> Secret = {
        0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
        0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
        0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
        0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
        0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
        0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
        0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
        0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
        0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
        0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
        0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
        0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
    };

    // Portable 64x64 -> 128 multiply (32x32 schoolbook), returned as a u128 {a = low, b = high}.
    // Avoids __uint128_t (absent on MSVC) and _umul128 (not constexpr), so it is valid in a constant expression on every compiler.
    [[nodiscard]]
    constexpr u128 Mult64to128(const u64 a, const u64 b) noexcept
    {
        const u64 loLo = static_cast<u64>(static_cast<u32>(a)) * static_cast<u64>(static_cast<u32>(b));
        const u64 hiLo = static_cast<u64>(static_cast<u32>(a >> 32)) * static_cast<u64>(static_cast<u32>(b));
        const u64 loHi = static_cast<u64>(static_cast<u32>(a)) * static_cast<u64>(static_cast<u32>(b >> 32));
        const u64 hiHi = static_cast<u64>(static_cast<u32>(a >> 32)) * static_cast<u64>(static_cast<u32>(b >> 32));
        const u64 cross = (loLo >> 32) + (hiLo & 0xFFFFFFFFull) + loHi;
        const u64 high = (hiLo >> 32) + (cross >> 32) + hiHi;
        const u64 low = (cross << 32) | (loLo & 0xFFFFFFFFull);
        return u128{.a = low, .b = high};
    }

    [[nodiscard]]
    constexpr u64 Mul128Fold(const u64 a, const u64 b) noexcept
    {
        const auto [low, high] = Mult64to128(a, b);
        return low ^ high;
    }

    [[nodiscard]]
    constexpr u32 Bswap32(const u32 value) noexcept
    {
        return (value << 24)
            | ((value & 0x0000FF00u) << 8)
            | ((value & 0x00FF0000u) >> 8)
            | (value >> 24);
    }

    [[nodiscard]]
    constexpr u64 Bswap64(const u64 value) noexcept
    {
        return (value << 56)
            | ((value & 0x000000000000FF00ull) << 40)
            | ((value & 0x0000000000FF0000ull) << 24)
            | ((value & 0x00000000FF000000ull) << 8)
            | ((value & 0x000000FF00000000ull) >> 8)
            | ((value & 0x0000FF0000000000ull) >> 24)
            | ((value & 0x00FF000000000000ull) >> 40)
            | (value >> 56);
    }

    [[nodiscard]]
    constexpr u64 Avalanche(u64 x) noexcept
    {
        x ^= x >> 37;
        x *= PrimeMx_1;
        x ^= x >> 32;
        return x;
    }

    [[nodiscard]]
    constexpr u64 Avalanche64(u64 x) noexcept
    {
        x ^= x >> 33;
        x *= Prime64_2;
        x ^= x >> 29;
        x *= Prime64_3;
        x ^= x >> 32;
        return x;
    }

    [[nodiscard]]
    constexpr u64 Rrmxmx(u64 value, const u64 length) noexcept
    {
        value ^= std::rotl(value, 49) ^ std::rotl(value, 24);
        value *= PrimeMx_2;
        value ^= (value >> 35) + length;
        value *= PrimeMx_2;
        value ^= value >> 28;
        return value;
    }

    // Reads a 64/32-bit little-endian word from the default secret at a byte offset.
    [[nodiscard]]
    constexpr u64 SecretLE64(const AZStd::size_t off) noexcept
    {
        return ReadLE64(Secret.data() + off);
    }

    [[nodiscard]]
    constexpr u32 SecretLE32(const AZStd::size_t off) noexcept
    {
        return ReadLE32(Secret.data() + off);
    }

    // Mixes a 16-byte data chunk with a 16-byte secret segment and the seed (default-secret path, <=240 bytes).
    template <typename T>
    [[nodiscard]]
    constexpr u64 MixStep(
        const T* data,
        const AZStd::size_t off,
        const AZStd::size_t secretOff,
        const u64 seed) noexcept
    {
        const u64 d0 = ReadLE64(data + off);
        const u64 d1 = ReadLE64(data + off + 8);
        return Mul128Fold(d0 ^ (SecretLE64(secretOff) + seed), d1 ^ (SecretLE64(secretOff + 8) - seed));
    }

    // Derives the per-hash secret from the seed (identity copy when seed == 0).
    [[nodiscard]]
    constexpr AZStd::array<u8, Secret.size()> InitCustomSecret(const u64 seed) noexcept
    {
        AZStd::array<u8, Secret.size()> out = Secret;
        if (seed != 0)
        {
            for (AZStd::size_t i = 0; i < 12; ++i)
            {
                const u64 lo = SecretLE64(16 * i) + seed;
                const u64 hi = SecretLE64(16 * i + 8) - seed;
                for (AZStd::size_t b = 0; b < 8; ++b)
                {
                    out[16 * i + b] = static_cast<u8>(lo >> (b * 8));
                    out[16 * i + 8 + b] = static_cast<u8>(hi >> (b * 8));
                }
            }
        }
        return out;
    }

    inline constexpr u64 InitAcc[8] = {
        Prime32_3, Prime64_1, Prime64_2, Prime64_3, Prime64_4, Prime32_2, Prime64_5, Prime32_1,
    };

    // Scalar per-stripe accumulation (canonical definition that the SIMD kernels replicate exactly).
    template <typename T>
    constexpr void Accumulate512Scalar(u64 (&acc)[8], const T* stripe, const u8* secret) noexcept
    {
        for (AZStd::size_t i = 0; i < 8; ++i)
        {
            const u64 dataVal = ReadLE64(stripe + i * 8);
            const u64 val = dataVal ^ ReadLE64(secret + i * 8);
            acc[i ^ 1] += dataVal;
            acc[i] += static_cast<u64>(static_cast<u32>(val)) * static_cast<u64>(val >> 32);
        }
    }

    // Scalar end-of-block scramble (uses the last 64 bytes of the secret).
    constexpr void ScrambleScalar(u64 (&acc)[8], const u8* secret) noexcept
    {
        const u8* scrambleSecret = secret + (Secret.size() - StripeLen);
        for (AZStd::size_t i = 0; i < 8; ++i)
        {
            acc[i] ^= acc[i] >> 47;
            acc[i] ^= ReadLE64(scrambleSecret + i * 8);
            acc[i] *= Prime32_1;
        }
    }

    [[nodiscard]]
    constexpr u64 FinalMerge(
        const u64 (&acc)[8],
        u64 result,
        const u8* secret,
        const AZStd::size_t secretOff) noexcept
    {
        for (AZStd::size_t i = 0; i < 4; ++i)
        {
            result += Mul128Fold(
                acc[i * 2] ^ ReadLE64(secret + secretOff + i * 16),
                acc[i * 2 + 1] ^ ReadLE64(secret + secretOff + i * 16 + 8));
        }
        return Avalanche(result);
    }

    // Scalar long-path hash (>240 bytes) producing {low, high}. The low half alone is the XXH3-64 result.
    template <typename T>
    [[nodiscard]]
    constexpr u128 HashLongScalar(
        const T* data,
        const AZStd::size_t len,
        const u64 seed,
        const bool want128) noexcept
    {
        const AZStd::array<u8, Secret.size()> secretStore = InitCustomSecret(seed);
        const u8* secret = secretStore.data();

        u64 acc[8] = {
            InitAcc[0], InitAcc[1], InitAcc[2], InitAcc[3], InitAcc[4], InitAcc[5], InitAcc[6], InitAcc[7],
        };

        constexpr AZStd::size_t stripesPerBlock = (Secret.size() - StripeLen) / 8; // 16
        constexpr AZStd::size_t blockSize = StripeLen * stripesPerBlock; // 1024

        const AZStd::size_t nbBlocks = (len - 1) / blockSize;
        for (AZStd::size_t block = 0; block < nbBlocks; ++block)
        {
            const T* blockStart = data + block * blockSize;
            for (AZStd::size_t stripe = 0; stripe < stripesPerBlock; ++stripe)
            {
                Accumulate512Scalar(acc, blockStart + stripe * StripeLen, secret + stripe * 8);
            }
            ScrambleScalar(acc, secret);
        }

        const AZStd::size_t nbStripes = ((len - 1) - nbBlocks * blockSize) / StripeLen;
        const T* lastBlock = data + nbBlocks * blockSize;
        for (AZStd::size_t stripe = 0; stripe < nbStripes; ++stripe)
        {
            Accumulate512Scalar(acc, lastBlock + stripe * StripeLen, secret + stripe * 8);
        }
        Accumulate512Scalar(acc, data + len - StripeLen, secret + (Secret.size() - 71));

        const u64 low = FinalMerge(acc, static_cast<u64>(len) * Prime64_1, secret, 11);
        u64 high = 0;
        if (want128)
        {
            high = FinalMerge(acc, ~(static_cast<u64>(len) * Prime64_2), secret, Secret.size() - 75);
        }
        return u128{.a = low, .b = high};
    }

    AZCORE_API u64 Hash64(const AZStd::byte* data, AZStd::size_t len, u64 seed) noexcept;
    AZCORE_API u128 Hash128(const AZStd::byte* data, AZStd::size_t len, u64 seed) noexcept;

    // XXH3-64 length buckets (<=240 bytes). The >240 long path is HashLongScalar, defined above.
    template <typename T>
    [[nodiscard]]
    constexpr u64 Hash64Scalar(const T* data, const AZStd::size_t len, const u64 seed) noexcept
    {
        if (len == 0)
        {
            return Avalanche64(seed ^ SecretLE64(56) ^ SecretLE64(64));
        }
        if (len <= 3)
        {
            const u32 combined =
                static_cast<u32>(static_cast<u8>(data[len - 1]))
                | (static_cast<u32>(len) << 8)
                | (static_cast<u32>(static_cast<u8>(data[0])) << 16)
                | (static_cast<u32>(static_cast<u8>(data[len >> 1])) << 24);
            const u64 value = ((static_cast<u64>(SecretLE32(0) ^ SecretLE32(4)) + seed) ^ combined);
            return Avalanche64(value);
        }
        if (len <= 8)
        {
            const u32 inputFirst = ReadLE32(data);
            const u32 inputLast = ReadLE32(data + len - 4);
            const u64 modifiedSeed = seed ^ (static_cast<u64>(Bswap32(static_cast<u32>(seed))) << 32);
            const u64 combined = static_cast<u64>(inputLast) | (static_cast<u64>(inputFirst) << 32);
            const u64 value = ((SecretLE64(8) ^ SecretLE64(16)) - modifiedSeed) ^ combined;
            return Rrmxmx(value, len);
        }
        if (len <= 16)
        {
            const u64 inputFirst = ReadLE64(data);
            const u64 inputLast = ReadLE64(data + len - 8);
            const u64 low = ((SecretLE64(24) ^ SecretLE64(32)) + seed) ^ inputFirst;
            const u64 high = ((SecretLE64(40) ^ SecretLE64(48)) - seed) ^ inputLast;
            const u64 value = static_cast<u64>(len) + Bswap64(low) + high + Mul128Fold(low, high);
            return Avalanche(value);
        }
        if (len <= 128)
        {
            u64 acc = static_cast<u64>(len) * Prime64_1;
            const AZStd::size_t numRounds = ((len - 1) >> 5) + 1;
            for (AZStd::size_t r = numRounds; r-- > 0;)
            {
                acc += MixStep(data, r * 16, r * 32, seed);
                acc += MixStep(data, len - r * 16 - 16, r * 32 + 16, seed);
            }
            return Avalanche(acc);
        }
        if (len <= 240)
        {
            u64 acc = static_cast<u64>(len) * Prime64_1;
            const AZStd::size_t numChunks = len >> 4;
            for (AZStd::size_t i = 0; i < 8; ++i)
            {
                acc += MixStep(data, i * 16, i * 16, seed);
            }
            acc = Avalanche(acc);
            for (AZStd::size_t i = 8; i < numChunks; ++i)
            {
                acc += MixStep(data, i * 16, (i - 8) * 16 + 3, seed);
            }
            acc += MixStep(data, len - 16, 119, seed);
            return Avalanche(acc);
        }
        return HashLongScalar(data, len, seed, false).a;
    }

    template <typename T>
    [[nodiscard]]
    constexpr u64 Dispatch64(const T* data, const AZStd::size_t len, const u64 seed)
    {
        if (std::is_constant_evaluated())
        {
            return Hash64Scalar(data, len, seed);
        }
        return Hash64(reinterpret_cast<const AZStd::byte*>(data), len, seed);
    }
} // namespace AZ::Hash::Private::Xxh3

namespace AZ::Hash
{
    /**
     * XXH3 (64-bit), the fastest xxHash for large inputs on SIMD hardware.
     * Compile-time and runtime hashing produce identical results.
     * Runtime hashing automatically uses the available SIMD path.
     * Accepts an optional 64-bit seed.
     * For a 128-bit digest use AZ::Hash::Xxh128.
     * Not suitable for cryptography.
     */
    struct AZCORE_API Xxh3 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Xxh3);

        constexpr Xxh3() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Xxh3(const u64 value)
            : m_value{value}
        {
        }

        template <AZStd::size_t N>
        constexpr Xxh3(const char (&str)[N], const u64 seed = 0)
            : m_value{Private::Xxh3::Dispatch64(str, N - 1, seed)}
        {
        }

        constexpr Xxh3(const AZStd::string_view view, const u64 seed = 0)
            : m_value{Private::Xxh3::Dispatch64(view.data(), view.size(), seed)}
        {
        }

        constexpr Xxh3(const AZStd::span<const AZStd::byte> data, const u64 seed = 0)
            : m_value{Private::Xxh3::Dispatch64(data.data(), data.size(), seed)}
        {
        }

        constexpr Xxh3(const AZStd::span<const u8> data, const u64 seed = 0)
            : m_value{Private::Xxh3::Dispatch64(data.data(), data.size(), seed)}
        {
        }

        [[nodiscard]]
        constexpr u64 GetValue() const noexcept
        {
            return m_value;
        }

        constexpr operator u64() const
        {
            return m_value;
        }

        constexpr bool operator==(const Xxh3 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Xxh3 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        u64 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Xxh3);
} // namespace AZ::Hash

consteval AZ::Hash::Xxh3 operator""_xxh3(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Xxh3{AZStd::string_view{str, count}};
}

template <>
struct AZStd::hash<AZ::Hash::Xxh3>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Xxh3 input) const noexcept
    {
        return input.GetValue();
    }
};

namespace AZ::Hash::Private::Xxh3
{
    template <typename T>
    constexpr void MixTwoChunks(
        u64 (&acc)[2],
        const T* data,
        const AZStd::size_t off1,
        const AZStd::size_t off2,
        const AZStd::size_t secretOff,
        const u64 seed) noexcept
    {
        const u64 d1_0 = ReadLE64(data + off1);
        const u64 d1_1 = ReadLE64(data + off1 + 8);
        const u64 d2_0 = ReadLE64(data + off2);
        const u64 d2_1 = ReadLE64(data + off2 + 8);
        acc[0] += MixStep(data, off1, secretOff, seed);
        acc[1] += MixStep(data, off2, secretOff + 16, seed);
        acc[0] ^= d2_0 + d2_1;
        acc[1] ^= d1_0 + d1_1;
    }

    // XXH3-128 length buckets (<=240 bytes). The >240 long path is HashLongScalar, defined above.
    template <typename T>
    [[nodiscard]]
    constexpr u128 Hash128Scalar(const T* data, const AZStd::size_t len, const u64 seed) noexcept
    {
        if (len == 0)
        {
            return u128{.a = Avalanche64(seed ^ SecretLE64(64) ^ SecretLE64(72)), .b = Avalanche64(seed ^ SecretLE64(80) ^ SecretLE64(88))};
        }
        if (len <= 3)
        {
            const u32 combined =
                static_cast<u32>(static_cast<u8>(data[len - 1]))
                | (static_cast<u32>(len) << 8)
                | (static_cast<u32>(static_cast<u8>(data[0])) << 16)
                | (static_cast<u32>(static_cast<u8>(data[len >> 1])) << 24);
            const u64 low = ((static_cast<u64>(SecretLE32(0) ^ SecretLE32(4)) + seed) ^ combined);
            const u64 high =
                (static_cast<u64>(SecretLE32(8) ^ SecretLE32(12)) - seed)
                ^ static_cast<u64>(std::rotl(Bswap32(combined), 13));
            return u128{.a = Avalanche64(low), .b = Avalanche64(high)};
        }
        if (len <= 8)
        {
            const u32 inputFirst = ReadLE32(data);
            const u32 inputLast = ReadLE32(data + len - 4);
            const u64 modifiedSeed = seed ^ (static_cast<u64>(Bswap32(static_cast<u32>(seed))) << 32);
            const u64 combined = static_cast<u64>(inputFirst) | (static_cast<u64>(inputLast) << 32);
            const u64 value = ((SecretLE64(16) ^ SecretLE64(24)) + modifiedSeed) ^ combined;
            const u128 product = Mult64to128(value, Prime64_1 + (static_cast<u64>(len) << 2));
            u64 high = product.b;
            u64 low = product.a;
            high += low << 1;
            low ^= high >> 3;
            low ^= low >> 35;
            low *= PrimeMx_2;
            low ^= low >> 28;
            high = Avalanche(high);
            return u128{.a = low, .b = high};
        }
        if (len <= 16)
        {
            const u64 inputFirst = ReadLE64(data);
            const u64 inputLast = ReadLE64(data + len - 8);
            const u64 val1 = ((SecretLE64(32) ^ SecretLE64(40)) - seed) ^ inputFirst ^ inputLast;
            const u64 val2 = ((SecretLE64(48) ^ SecretLE64(56)) + seed) ^ inputLast;
            const u128 product = Mult64to128(val1, Prime64_1);
            u64 low = product.a + (static_cast<u64>(len - 1) << 54);
            u64 high = product.b + (static_cast<u64>(static_cast<u32>(val2 >> 32)) << 32) +
                static_cast<u64>(static_cast<u32>(val2)) * Prime32_2;
            low ^= Bswap64(high);
            const u128 product2 = Mult64to128(low, Prime64_2);
            low = product2.a;
            high = product2.b + high * Prime64_2;
            return u128{.a = Avalanche(low), .b = Avalanche(high)};
        }

        u64 acc[2] = {static_cast<u64>(len) * Prime64_1, 0};
        if (len <= 128)
        {
            const AZStd::size_t numRounds = ((len - 1) >> 5) + 1;
            for (AZStd::size_t r = numRounds; r-- > 0;)
            {
                MixTwoChunks(acc, data, r * 16, len - r * 16 - 16, r * 32, seed);
            }
        }
        else if (len <= 240)
        {
            const AZStd::size_t numChunks = len >> 5;
            for (AZStd::size_t i = 0; i < 4; ++i)
            {
                MixTwoChunks(acc, data, i * 32, i * 32 + 16, i * 32, seed);
            }
            acc[0] = Avalanche(acc[0]);
            acc[1] = Avalanche(acc[1]);
            for (AZStd::size_t i = 4; i < numChunks; ++i)
            {
                MixTwoChunks(acc, data, i * 32, i * 32 + 16, (i - 4) * 32 + 3, seed);
            }
            MixTwoChunks(acc, data, len - 16, len - 32, 103, static_cast<u64>(0) - seed);
        }
        else
        {
            return HashLongScalar(data, len, seed, true);
        }

        const u64 low = acc[0] + acc[1];
        const u64 high =
            acc[0] * Prime64_1 + acc[1] * Prime64_4 + (static_cast<u64>(len) - seed) * Prime64_2;
        return u128{.a = Avalanche(low), .b = static_cast<u64>(0) - Avalanche(high)};
    }

    template <typename T>
    [[nodiscard]]
    constexpr u128 Dispatch128(const T* data, const AZStd::size_t len, const u64 seed)
    {
        if (std::is_constant_evaluated())
        {
            return Hash128Scalar(data, len, seed);
        }
        return Hash128(reinterpret_cast<const AZStd::byte*>(data), len, seed);
    }
} // namespace AZ::Hash::Private::Xxh3

namespace AZ::Hash
{
    /**
     * XXH3 128-bit, exposed as a {low, high} pair.
     * The fastest xxHash for large inputs, fully constexpr, and SIMD-accelerated at runtime.
     * Accepts an optional 64-bit seed.
     * For long inputs the low half equals AZ::Hash::Xxh3 of the same data.
     * Not suitable for cryptography.
     */
    struct AZCORE_API Xxh128 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Xxh128);

        constexpr Xxh128() = default;

        /* Wraps precomputed halves rather than hashing input. */
        constexpr Xxh128(const u64 low, const u64 high)
            : m_low{low}
            , m_high{high}
        {
        }

        template <AZStd::size_t N>
        constexpr Xxh128(const char (&str)[N], const u64 seed = 0)
        {
            const u128 result = Private::Xxh3::Dispatch128(str, N - 1, seed);
            m_low = result.a;
            m_high = result.b;
        }

        constexpr Xxh128(const AZStd::string_view view, const u64 seed = 0)
        {
            const u128 result = Private::Xxh3::Dispatch128(view.data(), view.size(), seed);
            m_low = result.a;
            m_high = result.b;
        }

        constexpr Xxh128(const AZStd::span<const AZStd::byte> data, const u64 seed = 0)
        {
            const u128 result = Private::Xxh3::Dispatch128(data.data(), data.size(), seed);
            m_low = result.a;
            m_high = result.b;
        }

        constexpr Xxh128(const AZStd::span<const u8> data, const u64 seed = 0)
        {
            const u128 result = Private::Xxh3::Dispatch128(data.data(), data.size(), seed);
            m_low = result.a;
            m_high = result.b;
        }

        [[nodiscard]]
        constexpr u64 GetLow() const noexcept
        {
            return m_low;
        }

        [[nodiscard]]
        constexpr u64 GetHigh() const noexcept
        {
            return m_high;
        }

        // Reduces the 128-bit value to a 64-bit hash suitable for use as a container key.
        [[nodiscard]]
        constexpr u64 GetValue() const noexcept
        {
            return m_low ^ m_high;
        }

        constexpr bool operator==(const Xxh128&) const = default;

        constexpr std::strong_ordering operator<=>(const Xxh128&) const = default;

        explicit constexpr operator bool() const
        {
            return m_low != 0 || m_high != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        u64 m_low = 0;
        u64 m_high = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Xxh128);
} // namespace AZ::Hash

consteval AZ::Hash::Xxh128 operator""_xxh128(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Xxh128{AZStd::string_view{str, count}};
}

template <>
struct AZStd::hash<AZ::Hash::Xxh128>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Xxh128 input) const noexcept
    {
        return input.GetValue();
    }
};
