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
    inline constexpr u32 Fnv1a32_OffsetBasis = 0x811c9dc5u;
    inline constexpr u32 Fnv1a32_Prime = 0x01000193u;

    inline constexpr u64 Fnv1a64_OffsetBasis = 0xcbf29ce484222325ULL;
    inline constexpr u64 Fnv1a64_Prime = 0x100000001b3ULL;

    constexpr u32 Fnv1a_32(const AZStd::span<const AZStd::byte> data)
    {
        u32 hash = Fnv1a32_OffsetBasis;
        for (const AZStd::byte b : data)
        {
            hash ^= static_cast<u32>(b);
            hash *= Fnv1a32_Prime;
        }
        return hash;
    }

    constexpr u64 Fnv1a_64(const AZStd::span<const AZStd::byte> data)
    {
        u64 hash = Fnv1a64_OffsetBasis;
        for (const AZStd::byte b : data)
        {
            hash ^= static_cast<u64>(b);
            hash *= Fnv1a64_Prime;
        }
        return hash;
    }

    constexpr u32 Fnv1a_32(AZStd::string_view view)
    {
        u32 hash = Fnv1a32_OffsetBasis;
        for (const char ch : view)
        {
            hash ^= static_cast<u32>(static_cast<u8>(ch));
            hash *= Fnv1a32_Prime;
        }
        return hash;
    }

    constexpr u64 Fnv1a_64(AZStd::string_view view)
    {
        u64 hash = Fnv1a64_OffsetBasis;
        for (const char ch : view)
        {
            hash ^= static_cast<u64>(static_cast<u8>(ch));
            hash *= Fnv1a64_Prime;
        }
        return hash;
    }

    struct Fnv1a32 final
    {
        static constexpr u32 OffsetBasis = 0x811c9dc5u;
        static constexpr u32 Prime = 0x01000193u;

        constexpr Fnv1a32() = default;

        explicit constexpr Fnv1a32(const u32 value)
            : m_value{value}
        {
        }

        explicit constexpr Fnv1a32(AZStd::string_view view)
        {
            m_value = OffsetBasis;
            for (const char ch : view)
            {
                m_value ^= static_cast<u32>(static_cast<u8>(ch));
                m_value *= Prime;
            }
        }

        explicit constexpr Fnv1a32(const AZStd::span<const AZStd::byte> data)
        {
            m_value = OffsetBasis;
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

        explicit constexpr operator u32() const
        {
            return m_value;
        }

        constexpr bool operator==(const Fnv1a32 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr bool operator==(const u32 rhs) const
        {
            return m_value == rhs;
        }

        constexpr std::strong_ordering operator<=>(const Fnv1a32 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const u32 rhs) const
        {
            return m_value <=> rhs;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

    private:
        u32 m_value = 0;
    };

    struct Fnv1a64 final
    {
        constexpr Fnv1a64() = default;

        explicit constexpr Fnv1a64(const u64 value)
            : m_value{value}
        {
        }

        explicit constexpr Fnv1a64(AZStd::string_view view)
            : m_value{Fnv1a_64(view)}
        {
        }

        explicit constexpr Fnv1a64(const AZStd::span<const AZStd::byte> data)
            : m_value{Fnv1a_64(data)}
        {
        }

        [[nodiscard]] constexpr u64 GetValue() const
        {
            return m_value;
        }

        explicit constexpr operator u64() const
        {
            return m_value;
        }

        constexpr bool operator==(const Fnv1a64 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr bool operator==(const u64 rhs) const
        {
            return m_value == rhs;
        }

        constexpr auto operator<=>(const Fnv1a64 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        constexpr auto operator<=>(const u64 rhs) const
        {
            return m_value <=> rhs;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

    private:
        u64 m_value = 0;
    };
} // namespace AZ::Hash

template <>
struct AZStd::hash<AZ::Hash::Fnv1a32>
{
    constexpr size_t operator()(const AZ::Hash::Fnv1a32 input) const
    {
        return input.GetValue();
    }
};

template <>
struct AZStd::hash<AZ::Hash::Fnv1a64>
{
    constexpr size_t operator()(const AZ::Hash::Fnv1a64 input) const
    {
        return input.GetValue();
    }
};
