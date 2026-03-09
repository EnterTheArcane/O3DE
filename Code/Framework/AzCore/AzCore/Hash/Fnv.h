/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/string/string_view.h>

#include <compare>

/**
 * FNV (Fowler-Noll-Vo) hash functions
 *
 * FNV-1a is a fast, non-cryptographic hash function with excellent distribution properties.
 * It operates byte-at-a-time: each input byte is XOR-ed into the running hash and then
 * multiplied by a width-specific prime.
 *
 * The algorithm is fully specified in RFC 9923 and the reference material maintained by Landon Curt Noll:
 * https://www.rfc-editor.org/rfc/rfc9923
 * http://www.isthe.com/chongo/tech/comp/fnv
 *
 * This header provides:
 * - Free constexpr functions accepting span or string_view.
 * - Type-safe wrappers that can be used as container keys via the hash specializations at the bottom.
 */
namespace AZ::Hash
{
    struct Fnv32 final
    {
        static constexpr u32 OffsetBasis = 0x811c9dc5u;
        static constexpr u32 Prime = 0x01000193u;

        constexpr Fnv32() = default;

        explicit constexpr Fnv32(const u32 value)
            : m_value{value}
        {
        }

        template <size_t N>
        constexpr Fnv32(const char (&str)[N])
            : m_value(OffsetBasis)
        {
            for (size_t i = 0; i < N - 1; ++i)
            {
                m_value ^= static_cast<u32>(static_cast<u8>(str[i]));
                m_value *= Prime;
            }

            if (!std::is_constant_evaluated())
            {
                DebugString(AZStd::string_view{str, N});
            }
        }

        constexpr Fnv32(AZStd::string_view view)
            : m_value(OffsetBasis)
        {
            for (const char ch : view)
            {
                m_value ^= static_cast<u32>(static_cast<u8>(ch));
                m_value *= Prime;
            }

            if (!std::is_constant_evaluated())
            {
                DebugString(view);
            }
        }

        constexpr Fnv32(const AZStd::span<const AZStd::byte> data)
            : m_value(OffsetBasis)
        {
            for (const AZStd::byte b : data)
            {
                m_value ^= static_cast<u32>(b);
                m_value *= Prime;
            }
        }

        [[nodiscard]] constexpr u32 GetValue() const
        {
            return m_value;
        }

        constexpr operator u32() const
        {
            return m_value;
        }

        constexpr bool operator==(const Fnv32 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Fnv32 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

    protected:
        AZCORE_API void DebugString(AZStd::string_view str) const;

    private:
        u32 m_value = 0;
    };

    struct Fnv64 final
    {
        static constexpr u64 OffsetBasis = 0xcbf29ce484222325ULL;
        static constexpr u64 Prime = 0x100000001b3ULL;

        constexpr Fnv64() = default;

        explicit constexpr Fnv64(const u64 value)
            : m_value{value}
        {
        }

        template <size_t N>
        constexpr Fnv64(const char (&str)[N])
            : m_value(OffsetBasis)
        {
            for (size_t i = 0; i < N - 1; ++i)
            {
                m_value ^= static_cast<u64>(static_cast<u8>(str[i]));
                m_value *= Prime;
            }

            if (!std::is_constant_evaluated())
            {
                DebugString(AZStd::string_view{str, N});
            }
        }

        constexpr Fnv64(AZStd::string_view view)
            : m_value(OffsetBasis)
        {
            for (const char ch : view)
            {
                m_value ^= static_cast<u64>(static_cast<u8>(ch));
                m_value *= Prime;
            }

            if (!std::is_constant_evaluated())
            {
                DebugString(view);
            }
        }

        constexpr Fnv64(const AZStd::span<const AZStd::byte> data)
            : m_value(OffsetBasis)
        {
            for (const AZStd::byte b : data)
            {
                m_value ^= static_cast<u64>(b);
                m_value *= Prime;
            }
        }

        [[nodiscard]] constexpr u64 GetValue() const
        {
            return m_value;
        }

        constexpr operator u64() const
        {
            return m_value;
        }

        constexpr bool operator==(const Fnv64 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr auto operator<=>(const Fnv64 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

    protected:
        AZCORE_API void DebugString(AZStd::string_view str) const;

    private:
        u64 m_value = 0;
    };

    using Fnv1a_32 = Fnv32;
    using Fnv1a_64 = Fnv64;
} // namespace AZ::Hash

template <>
struct AZStd::hash<AZ::Hash::Fnv32>
{
    constexpr size_t operator()(const AZ::Hash::Fnv32 input) const
    {
        return input.GetValue();
    }
};

template <>
struct AZStd::hash<AZ::Hash::Fnv64>
{
    constexpr size_t operator()(const AZ::Hash::Fnv64 input) const
    {
        return input.GetValue();
    }
};
