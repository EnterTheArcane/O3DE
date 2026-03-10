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
#include <AzCore/RTTI/TypeInfoSimple.h>

#include <compare>

namespace AZ
{
    class SerializeContext;
}

#if defined(AZ_RELEASE_BUILD)
#define AZ_HASH_DEBUG 0
#else
#define AZ_HASH_DEBUG 1
#endif

/**
 * FNV-1a (Fowler-Noll-Vo, variant 1a) hash types
 *
 * A fast, non-cryptographic hash function with excellent distribution properties.
 * It operates byte-at-a-time: each input byte is XOR-ed into the running hash and then multiplied by a width-specific prime.
 * The offset basis and prime constants for each width are defined by the specification and must not be changed.
 *
 * - <a href="http://www.isthe.com/chongo/tech/comp/fnv">Publication</a>
 * - <a href="https://www.rfc-editor.org/rfc/rfc9923">RFC</a>
 *
 * This header provides two type-safe wrappers that can be used as drop-in alternatives to raw integer hash values.
 * Both types implicitly convert from string literals, string views, byte spans, so hashing happens at the point of construction.
 *
 * Each type also converts implicitly to its underlying integer (u32 / u64),
 * so it can be used anywhere a plain hash value is expected,
 * while still providing type safety when stored in containers.
 *
 * <b>Fnv32:</b>
 * Suitable for identifiers, tags, and lookup keys where the input domain is small (up to tens of thousands of unique keys).
 * Matches the width of AZ::Crc32 and can serve as a replacement for it.
 *
 * <b>Fnv64:</b>
 * Preferred when the input domain is large or when collision probability must be kept extremely low.
 * On 64-bit platforms the wider multiply is essentially free, so there is no meaningful performance penalty compared to Fnv32.
 *
 * In debug builds the string-constructed variants also store the original source text, which can be read by debuggers.
 * This storage is compiled out in release builds.
 */
namespace AZ::Hash
{
    struct Fnv32 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Fnv32);

        static constexpr u32 OffsetBasis = 0x811C9DC5;
        static constexpr u32 Prime = 0x01000193;

        constexpr Fnv32() = default;

        explicit constexpr Fnv32(const u32 value)
            : m_value{value}
        {
        }

        template <size_t N>
        constexpr Fnv32(const char (&str)[N])
            : m_value(OffsetBasis)
#if AZ_HASH_DEBUG
            , m_source(str)
#endif
        {
            for (size_t i = 0; i < N - 1; ++i)
            {
                m_value ^= static_cast<u32>(static_cast<u8>(str[i]));
                m_value *= Prime;
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

        AZCORE_API static void Reflect(AZ::SerializeContext& context);

    private:
        u32 m_value = 0;
#if AZ_HASH_DEBUG
        const char* m_source = "<UNKNOWN>";
#endif
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Fnv32);

    struct Fnv64 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Fnv64);

        static constexpr u64 OffsetBasis = 0xCBF29CE484222325;
        static constexpr u64 Prime = 0x100000001B3;

        constexpr Fnv64() = default;

        explicit constexpr Fnv64(const u64 value)
            : m_value{value}
        {
        }

        template <size_t N>
        constexpr Fnv64(const char (&str)[N])
            : m_value(OffsetBasis)
#if AZ_HASH_DEBUG
            , m_source(str)
#endif
        {
            for (size_t i = 0; i < N - 1; ++i)
            {
                m_value ^= static_cast<u64>(static_cast<u8>(str[i]));
                m_value *= Prime;
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

        AZCORE_API static void Reflect(AZ::SerializeContext& context);

    private:
        u64 m_value = 0;
#if AZ_HASH_DEBUG
        const char* m_source = nullptr;
#endif
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Fnv64);

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

#undef AZ_HASH_DEBUG
