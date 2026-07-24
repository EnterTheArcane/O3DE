/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/hash.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/RTTI/TypeInfoSimple.h>

namespace AZ
{
    class ReflectContext;
}

namespace AZ::Hash
{
    /**
     * FNV-1a (Fowler-Noll-Vo, variant 1a), a fast non-cryptographic hash with excellent distribution.
     * It operates byte-at-a-time: each input byte is XOR-ed into the running hash, then multiplied by a width-specific prime.
     * The offset basis and prime are fixed by the specification and must not be changed.
     * This 32-bit variant suits identifiers, tags, and lookup keys with a small input domain (up to tens of thousands of unique keys).
     *
     * - <a href="http://www.isthe.com/chongo/tech/comp/fnv">Publication</a>
     * - <a href="https://www.rfc-editor.org/rfc/rfc9923">RFC</a>
     */
    struct AZCORE_API Fnv32 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Fnv32);

        static constexpr u32 OffsetBasis = 0x811C9DC5;
        static constexpr u32 Prime = 0x01000193;

        constexpr Fnv32() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Fnv32(const u32 value)
            : m_value{value}
        {
        }

        template <AZStd::size_t N>
        constexpr Fnv32(const char (&str)[N])
            : m_value(OffsetBasis)
        {
            for (AZStd::size_t i = 0; i < N - 1; ++i)
            {
                m_value ^= static_cast<u32>(static_cast<u8>(str[i]));
                m_value *= Prime;
            }
        }

        constexpr Fnv32(const AZStd::string_view view)
            : m_value(OffsetBasis)
        {
            for (const char ch : view)
            {
                m_value ^= static_cast<u32>(static_cast<u8>(ch));
                m_value *= Prime;
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

        constexpr Fnv32(const AZStd::span<const u8> data)
            : m_value(OffsetBasis)
        {
            for (const u8 b : data)
            {
                m_value ^= static_cast<u32>(b);
                m_value *= Prime;
            }
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

        static void Reflect(AZ::ReflectContext* context);

    private:
        u32 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Fnv32);

    /**
     * FNV-1a (Fowler-Noll-Vo, variant 1a), the 64-bit variant.
     * Preferred when the input domain is large or when collision probability must be kept extremely low.
     * On 64-bit platforms the wider multiply is essentially free, so there is no meaningful performance penalty compared to Fnv32.
     *
     * - <a href="http://www.isthe.com/chongo/tech/comp/fnv">Publication</a>
     * - <a href="https://www.rfc-editor.org/rfc/rfc9923">RFC</a>
     */
    struct AZCORE_API Fnv64 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Fnv64);

        static constexpr u64 OffsetBasis = 0xCBF29CE484222325;
        static constexpr u64 Prime = 0x100000001B3;

        constexpr Fnv64() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Fnv64(const u64 value)
            : m_value{value}
        {
        }

        template <AZStd::size_t N>
        constexpr Fnv64(const char (&str)[N])
            : m_value(OffsetBasis)
        {
            for (AZStd::size_t i = 0; i < N - 1; ++i)
            {
                m_value ^= static_cast<u64>(static_cast<u8>(str[i]));
                m_value *= Prime;
            }
        }

        constexpr Fnv64(const AZStd::string_view view)
            : m_value(OffsetBasis)
        {
            for (const char ch : view)
            {
                m_value ^= static_cast<u64>(static_cast<u8>(ch));
                m_value *= Prime;
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

        constexpr Fnv64(const AZStd::span<const u8> data)
            : m_value(OffsetBasis)
        {
            for (const u8 b : data)
            {
                m_value ^= static_cast<u64>(b);
                m_value *= Prime;
            }
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

        constexpr bool operator==(const Fnv64 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Fnv64 rhs) const
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

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Fnv64);
} // namespace AZ::Hash

consteval AZ::Hash::Fnv32 operator""_fnv32(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Fnv32{AZStd::string_view{str, count}};
}

consteval AZ::Hash::Fnv64 operator""_fnv64(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Fnv64{AZStd::string_view{str, count}};
}

template <>
struct AZStd::hash<AZ::Hash::Fnv32>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Fnv32 input) const noexcept
    {
        return input.GetValue();
    }
};

template <>
struct AZStd::hash<AZ::Hash::Fnv64>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Fnv64 input) const noexcept
    {
        return input.GetValue();
    }
};
