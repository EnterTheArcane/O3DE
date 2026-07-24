/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Portions of this file are an independent reimplementation of the XXH32 and XXH64 algorithms from Yann Collet's xxHash.
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

namespace AZ::Hash::Private::Xxh32
{
    inline constexpr u32 Prime32_1 = 0x9E3779B1u;
    inline constexpr u32 Prime32_2 = 0x85EBCA77u;
    inline constexpr u32 Prime32_3 = 0xC2B2AE3Du;
    inline constexpr u32 Prime32_4 = 0x27D4EB2Fu;
    inline constexpr u32 Prime32_5 = 0x165667B1u;

    [[nodiscard]]
    constexpr u32 Round(u32 acc, const u32 lane) noexcept
    {
        acc += lane * Prime32_2;
        acc = std::rotl(acc, 13);
        return acc * Prime32_1;
    }

    template <typename T>
    [[nodiscard]]
    constexpr u32 Hash(const T* input, const AZStd::size_t len, const u32 seed) noexcept
    {
        const T* p = input;
        const T* const end = input + len;
        u32 acc;

        if (len >= 16)
        {
            const T* const limit = end - 16;
            u32 acc1 = seed + Prime32_1 + Prime32_2;
            u32 acc2 = seed + Prime32_2;
            u32 acc3 = seed + 0;
            u32 acc4 = seed - Prime32_1;
            do
            {
                acc1 = Round(acc1, ReadLE32(p)); p += 4;
                acc2 = Round(acc2, ReadLE32(p)); p += 4;
                acc3 = Round(acc3, ReadLE32(p)); p += 4;
                acc4 = Round(acc4, ReadLE32(p)); p += 4;
            } while (p <= limit);
            acc = std::rotl(acc1, 1) + std::rotl(acc2, 7) + std::rotl(acc3, 12) + std::rotl(acc4, 18);
        }
        else
        {
            acc = seed + Prime32_5;
        }

        acc += static_cast<u32>(len);

        while (end - p >= 4)
        {
            acc += ReadLE32(p) * Prime32_3;
            acc = std::rotl(acc, 17) * Prime32_4;
            p += 4;
        }
        while (p < end)
        {
            acc += static_cast<u32>(static_cast<u8>(*p)) * Prime32_5;
            acc = std::rotl(acc, 11) * Prime32_1;
            p += 1;
        }

        acc ^= acc >> 15;
        acc *= Prime32_2;
        acc ^= acc >> 13;
        acc *= Prime32_3;
        acc ^= acc >> 16;
        return acc;
    }
} // namespace AZ::Hash::Private::Xxh32

namespace AZ::Hash
{
    /**
     * XXH32, Yann Collet's classic 32-bit xxHash.
     * Fast, high quality, and fully constexpr: a hash computed at compile time equals the same input hashed at runtime.
     * Accepts an optional 32-bit seed.
     * For the fastest large-buffer hashing prefer AZ::Hash::Xxh3 or Xxh128.
     */
    struct AZCORE_API Xxh32 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Xxh32);

        constexpr Xxh32() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Xxh32(const u32 value)
            : m_value{value}
        {
        }

        template <AZStd::size_t N>
        constexpr Xxh32(const char (&str)[N], const u32 seed = 0)
            : m_value{Private::Xxh32::Hash(str, N - 1, seed)}
        {
        }

        constexpr Xxh32(const AZStd::string_view view, const u32 seed = 0)
            : m_value{Private::Xxh32::Hash(view.data(), view.size(), seed)}
        {
        }

        constexpr Xxh32(const AZStd::span<const AZStd::byte> data, const u32 seed = 0)
            : m_value{Private::Xxh32::Hash(data.data(), data.size(), seed)}
        {
        }

        constexpr Xxh32(const AZStd::span<const u8> data, const u32 seed = 0)
            : m_value{Private::Xxh32::Hash(data.data(), data.size(), seed)}
        {
        }

        [[nodiscard]]
        constexpr u32 GetValue() const noexcept
        {
            return m_value;
        }

        constexpr operator u32() const
        {
            return m_value;
        }

        constexpr bool operator==(const Xxh32 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Xxh32 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        u32 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Xxh32);
} // namespace AZ::Hash

consteval AZ::Hash::Xxh32 operator""_xxh32(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Xxh32{AZStd::string_view{str, count}};
}

template <>
struct AZStd::hash<AZ::Hash::Xxh32>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Xxh32 input) const noexcept
    {
        return input.GetValue();
    }
};

namespace AZ::Hash::Private::Xxh64Detail
{
    inline constexpr u64 Prime64_1 = 0x9E3779B185EBCA87ull;
    inline constexpr u64 Prime64_2 = 0xC2B2AE3D27D4EB4Full;
    inline constexpr u64 Prime64_3 = 0x165667B19E3779F9ull;
    inline constexpr u64 Prime64_4 = 0x85EBCA77C2B2AE63ull;
    inline constexpr u64 Prime64_5 = 0x27D4EB2F165667C5ull;

    [[nodiscard]]
    constexpr u64 Round(u64 acc, const u64 lane) noexcept
    {
        acc += lane * Prime64_2;
        acc = std::rotl(acc, 31);
        return acc * Prime64_1;
    }

    [[nodiscard]]
    constexpr u64 MergeRound(u64 acc, const u64 accN) noexcept
    {
        acc ^= Round(0, accN);
        return acc * Prime64_1 + Prime64_4;
    }

    template <typename T>
    [[nodiscard]]
    constexpr u64 Hash(const T* input, const AZStd::size_t len, const u64 seed) noexcept
    {
        const T* p = input;
        const T* const end = input + len;
        u64 acc;

        if (len >= 32)
        {
            const T* const limit = end - 32;
            u64 acc1 = seed + Prime64_1 + Prime64_2;
            u64 acc2 = seed + Prime64_2;
            u64 acc3 = seed + 0;
            u64 acc4 = seed - Prime64_1;
            do
            {
                acc1 = Round(acc1, ReadLE64(p)); p += 8;
                acc2 = Round(acc2, ReadLE64(p)); p += 8;
                acc3 = Round(acc3, ReadLE64(p)); p += 8;
                acc4 = Round(acc4, ReadLE64(p)); p += 8;
            } while (p <= limit);
            acc = std::rotl(acc1, 1) + std::rotl(acc2, 7) + std::rotl(acc3, 12) + std::rotl(acc4, 18);
            acc = MergeRound(acc, acc1);
            acc = MergeRound(acc, acc2);
            acc = MergeRound(acc, acc3);
            acc = MergeRound(acc, acc4);
        }
        else
        {
            acc = seed + Prime64_5;
        }

        acc += static_cast<u64>(len);

        while (end - p >= 8)
        {
            acc ^= Round(0, ReadLE64(p));
            acc = std::rotl(acc, 27) * Prime64_1 + Prime64_4;
            p += 8;
        }
        if (end - p >= 4)
        {
            acc ^= static_cast<u64>(ReadLE32(p)) * Prime64_1;
            acc = std::rotl(acc, 23) * Prime64_2 + Prime64_3;
            p += 4;
        }
        while (p < end)
        {
            acc ^= static_cast<u64>(static_cast<u8>(*p)) * Prime64_5;
            acc = std::rotl(acc, 11) * Prime64_1;
            p += 1;
        }

        acc ^= acc >> 33;
        acc *= Prime64_2;
        acc ^= acc >> 29;
        acc *= Prime64_3;
        acc ^= acc >> 32;
        return acc;
    }
} // namespace AZ::Hash::Private::Xxh64Detail

namespace AZ::Hash
{
    /**
     * XXH64, Yann Collet's classic 64-bit xxHash.
     * Fast, high quality, and fully constexpr: a hash computed at compile time equals the same input hashed at runtime.
     * Accepts an optional 64-bit seed.
     * For the fastest large-buffer hashing prefer AZ::Hash::Xxh3 or Xxh128.
     */
    struct AZCORE_API Xxh64 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Xxh64);

        constexpr Xxh64() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Xxh64(const u64 value)
            : m_value{value}
        {
        }

        template <AZStd::size_t N>
        constexpr Xxh64(const char (&str)[N], const u64 seed = 0)
            : m_value{Private::Xxh64Detail::Hash(str, N - 1, seed)}
        {
        }

        constexpr Xxh64(const AZStd::string_view view, const u64 seed = 0)
            : m_value{Private::Xxh64Detail::Hash(view.data(), view.size(), seed)}
        {
        }

        constexpr Xxh64(const AZStd::span<const AZStd::byte> data, const u64 seed = 0)
            : m_value{Private::Xxh64Detail::Hash(data.data(), data.size(), seed)}
        {
        }

        constexpr Xxh64(const AZStd::span<const u8> data, const u64 seed = 0)
            : m_value{Private::Xxh64Detail::Hash(data.data(), data.size(), seed)}
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

        constexpr bool operator==(const Xxh64 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Xxh64 rhs) const
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

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Xxh64);
} // namespace AZ::Hash

consteval AZ::Hash::Xxh64 operator""_xxh64(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Xxh64{AZStd::string_view{str, count}};
}

template <>
struct AZStd::hash<AZ::Hash::Xxh64>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Xxh64 input) const noexcept
    {
        return input.GetValue();
    }
};
