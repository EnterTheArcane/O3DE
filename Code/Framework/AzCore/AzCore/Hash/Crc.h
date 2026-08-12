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
     * CRC-32/ISO-HDLC (also known as CRC-32b), the variant used by zlib, Ethernet, PNG, and ZIP.
     * Reflected polynomial 0xEDB88320, initial value 0xFFFFFFFF, final XOR 0xFFFFFFFF.
     * Table-driven and generated at compile time, so there is no runtime initialization cost.
     * Matches the output of zlibs crc32().
     */
    struct AZCORE_API Crc32 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Crc32);

        static constexpr u32 Polynomial = 0xEDB88320u; // CRC-32/ISO-HDLC reflected polynomial
        static constexpr u32 InitialValue = 0xFFFFFFFFu;
        static constexpr u32 XorOut = 0xFFFFFFFFu;

        constexpr Crc32() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Crc32(const u32 value)
            : m_value{value}
        {
        }

        template<AZStd::size_t N>
        constexpr Crc32(const char (&str)[N])
            : m_value(InitialValue)
        {
            for (AZStd::size_t i = 0; i < N - 1; ++i)
            {
                m_value = ProcessByte(m_value, static_cast<u8>(str[i]));
            }
            m_value ^= XorOut;
        }

        constexpr Crc32(const AZStd::string_view view)
            : m_value(InitialValue)
        {
            for (const char ch : view)
            {
                m_value = ProcessByte(m_value, static_cast<u8>(ch));
            }
            m_value ^= XorOut;
        }

        constexpr Crc32(const AZStd::span<const AZStd::byte> data)
            : m_value(InitialValue)
        {
            for (const AZStd::byte b : data)
            {
                m_value = ProcessByte(m_value, static_cast<u8>(b));
            }
            m_value ^= XorOut;
        }

        constexpr Crc32(const AZStd::span<const u8> data)
            : m_value(InitialValue)
        {
            for (const u8 b : data)
            {
                m_value = ProcessByte(m_value, b);
            }
            m_value ^= XorOut;
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

        constexpr bool operator==(const Crc32 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Crc32 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        // Lookup table generated at compile time from the reflected polynomial.
        static constexpr auto Table = []() constexpr
        {
            AZStd::array<u32, 256> result;
            for (u32 i = 0; i < result.size(); ++i)
            {
                u32 crc = i;
                for (s32 j = 0; j < 8; ++j)
                {
                    crc = (crc >> 1) ^ (Polynomial * (crc & 1u));
                }
                result[i] = crc;
            }
            return result;
        }();

        static constexpr u32 ProcessByte(const u32 crc, const u8 byte) noexcept
        {
            return Table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
        }

        u32 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Crc32);
} // namespace AZ::Hash

consteval AZ::Hash::Crc32 operator""_crc32(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Crc32{AZStd::string_view{str, count}};
}

template<>
struct AZStd::hash<AZ::Hash::Crc32>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Crc32 input) const noexcept
    {
        return input.GetValue();
    }
};

namespace AZ::Hash
{
    /**
     * CRC-64/XZ (also known as CRC-64/GO-ECMA), the variant used by the XZ compression format.
     * Reflected polynomial 0xC96C5795D7870F42, initial value all-ones, final XOR all-ones.
     * Table-driven and generated at compile time, so there is no runtime initialization cost.
     * Preferred when the input domain is large or when a 64-bit checksum is required.
     */
    struct AZCORE_API Crc64 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, Crc64);

        static constexpr u64 Polynomial = 0xC96C5795D7870F42ull; // CRC-64/XZ reflected polynomial (normal 0x42F0E1EBA9EA3693)
        static constexpr u64 InitialValue = 0xFFFFFFFFFFFFFFFFull;
        static constexpr u64 XorOut = 0xFFFFFFFFFFFFFFFFull;

        constexpr Crc64() = default;

        /* Wraps a precomputed value rather than hashing input. */
        explicit constexpr Crc64(const u64 value)
            : m_value{value}
        {
        }

        template<AZStd::size_t N>
        constexpr Crc64(const char (&str)[N])
            : m_value(InitialValue)
        {
            for (AZStd::size_t i = 0; i < N - 1; ++i)
            {
                m_value = ProcessByte(m_value, static_cast<u8>(str[i]));
            }
            m_value ^= XorOut;
        }

        constexpr Crc64(const AZStd::string_view view)
            : m_value(InitialValue)
        {
            for (const char ch : view)
            {
                m_value = ProcessByte(m_value, static_cast<u8>(ch));
            }
            m_value ^= XorOut;
        }

        constexpr Crc64(const AZStd::span<const AZStd::byte> data)
            : m_value(InitialValue)
        {
            for (const AZStd::byte b : data)
            {
                m_value = ProcessByte(m_value, static_cast<u8>(b));
            }
            m_value ^= XorOut;
        }

        constexpr Crc64(const AZStd::span<const u8> data)
            : m_value(InitialValue)
        {
            for (const u8 b : data)
            {
                m_value = ProcessByte(m_value, b);
            }
            m_value ^= XorOut;
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

        constexpr bool operator==(const Crc64 rhs) const
        {
            return m_value == rhs.m_value;
        }

        constexpr std::strong_ordering operator<=>(const Crc64 rhs) const
        {
            return m_value <=> rhs.m_value;
        }

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        // Lookup table generated at compile time from the reflected polynomial.
        static constexpr auto Table = []() constexpr
        {
            AZStd::array<u64, 256> result;
            for (u64 i = 0; i < result.size(); ++i)
            {
                u64 crc = i;
                for (s32 j = 0; j < 8; ++j)
                {
                    crc = (crc >> 1) ^ (Polynomial * (crc & 1ull));
                }
                result[i] = crc;
            }
            return result;
        }();

        static constexpr u64 ProcessByte(const u64 crc, const u8 byte) noexcept
        {
            return Table[(crc ^ byte) & 0xFFull] ^ (crc >> 8);
        }

        u64 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, Crc64);
} // namespace AZ::Hash

consteval AZ::Hash::Crc64 operator""_crc64(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::Crc64{AZStd::string_view{str, count}};
}

template<>
struct AZStd::hash<AZ::Hash::Crc64>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::Crc64 input) const noexcept
    {
        return input.GetValue();
    }
};
