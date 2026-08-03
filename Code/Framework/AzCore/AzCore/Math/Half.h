/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/bit.h>
#include <AzCore/std/hash.h>

//! 1 when AZ::Half conversions can be evaluated in a constant expression. See AzCore/std/bit.h.
#define AZ_MATH_HALF_IS_CONSTEXPR AZ_STD_HAS_CONSTEXPR_BIT_CAST

#if AZ_MATH_HALF_IS_CONSTEXPR
#   define AZ_MATH_HALF_CONSTEXPR constexpr
#else
#   define AZ_MATH_HALF_CONSTEXPR
#endif

namespace AZ
{
    //! An IEEE 754 binary16 ("half precision") floating point value: 1 sign bit, 5 exponent bits
    //! with a bias of 15, and 10 mantissa bits.
    //!
    //! Conversions are correctly rounded (round to nearest, ties to even) and handle the full range:
    //! subnormals, signed zero, infinities and NaN. Where the toolchain provides __builtin_bit_cast
    //! they are usable at compile time.
    //!
    //! Range notes: the largest finite value is 65504, the smallest positive normal is 2^-14, and the
    //! smallest positive subnormal is 2^-24. Values that overflow convert to infinity, and values
    //! that underflow convert to a signed zero.
    struct Half final
    {
        AZ_TYPE_INFO(Half, "{5C6F2E80-A417-4B93-9D25-8E0F31C7A6B4}");

        constexpr Half() = default;

        AZ_MATH_HALF_CONSTEXPR explicit Half(float value)
            : m_bits(FromFloat(value))
        {
        }

        //! Wraps a raw binary16 bit pattern without converting it.
        static constexpr Half FromBits(u16 bits)
        {
            Half result;
            result.m_bits = bits;
            return result;
        }

        constexpr u16 GetBits() const { return m_bits; }

        //! Widening to float. Implicit and exact -- every binary16 value is representable as a float.
        AZ_MATH_HALF_CONSTEXPR operator float() const { return ToFloat(m_bits); }

        static constexpr Half CreateZero() { return FromBits(0x0000); }
        static constexpr Half CreateOne() { return FromBits(0x3C00); }
        //! The largest finite binary16 value, 65504.
        static constexpr Half CreateMax() { return FromBits(0x7BFF); }
        static constexpr Half CreateInfinity() { return FromBits(0x7C00); }
        static constexpr Half CreateNaN() { return FromBits(0x7E00); }

        constexpr bool IsZero() const { return (m_bits & 0x7FFF) == 0; }
        constexpr bool IsNegative() const { return (m_bits & 0x8000) != 0; }
        constexpr bool IsInfinity() const { return (m_bits & 0x7FFF) == 0x7C00; }
        constexpr bool IsNaN() const { return (m_bits & 0x7FFF) > 0x7C00; }
        constexpr bool IsFinite() const { return (m_bits & 0x7C00) != 0x7C00; }

        //! Bit pattern equality. Note this is NOT float equality: NaN compares equal to itself, and
        //! +0 does not compare equal to -0. Compare as float when IEEE semantics are wanted.
        constexpr bool operator==(const Half& rhs) const { return m_bits == rhs.m_bits; }
        constexpr bool operator!=(const Half& rhs) const { return m_bits != rhs.m_bits; }

        //! Converts a float to a binary16 bit pattern, rounding to nearest with ties to even.
        static AZ_MATH_HALF_CONSTEXPR u16 FromFloat(float value)
        {
            const u32 bits = AZStd::bit_cast<u32>(value);
            const u32 sign = (bits >> 16) & 0x8000u;
            const u32 biasedExponent = (bits >> 23) & 0xFFu;
            const u32 mantissa = bits & 0x007FFFFFu;

            // Infinity and NaN keep their class. A NaN must stay a NaN, so force a non-zero mantissa
            // rather than letting the truncation below turn a small payload into an infinity.
            if (biasedExponent == 0xFFu)
            {
                const u32 halfMantissa = (mantissa != 0u) ? (0x0200u | (mantissa >> 13)) : 0u;
                return static_cast<u16>(sign | 0x7C00u | halfMantissa);
            }

            const AZ::s32 halfExponent = static_cast<AZ::s32>(biasedExponent) - 127 + 15;

            if (halfExponent >= 0x1F)
            {
                // Overflows the binary16 exponent range.
                return static_cast<u16>(sign | 0x7C00u);
            }

            if (halfExponent <= 0)
            {
                // Subnormal, or too small to represent at all.
                if (halfExponent < -10)
                {
                    return static_cast<u16>(sign);
                }

                // Restore the implicit leading 1 and shift into subnormal position.
                const u32 withImplicitBit = mantissa | 0x00800000u;
                const u32 shift = static_cast<u32>(14 - halfExponent);
                u32 result = withImplicitBit >> shift;

                const u32 roundBit = 1u << (shift - 1);
                if ((withImplicitBit & roundBit) != 0u &&
                    (((withImplicitBit & (roundBit - 1)) != 0u) || ((result & 1u) != 0u)))
                {
                    ++result;
                }
                return static_cast<u16>(sign | result);
            }

            // Normal. A round that carries out of the mantissa correctly increments the exponent,
            // and an exponent that carries into 0x1F correctly yields infinity.
            u32 result = (static_cast<u32>(halfExponent) << 10) | (mantissa >> 13);
            constexpr u32 roundBit = 1u << 12;
            if ((mantissa & roundBit) != 0u &&
                (((mantissa & (roundBit - 1)) != 0u) || ((result & 1u) != 0u)))
            {
                ++result;
            }
            return static_cast<u16>(sign | result);
        }

        //! Converts a binary16 bit pattern to a float. Always exact.
        static AZ_MATH_HALF_CONSTEXPR float ToFloat(u16 half)
        {
            const u32 sign = static_cast<u32>(half & 0x8000u) << 16;
            const u32 exponent = (half >> 10) & 0x1Fu;
            u32 mantissa = half & 0x03FFu;

            if (exponent == 0u)
            {
                if (mantissa == 0u)
                {
                    return AZStd::bit_cast<float>(sign); // signed zero
                }

                // Subnormal: normalize by shifting the leading 1 up into the implicit bit position.
                u32 shift = 0u;
                while ((mantissa & 0x0400u) == 0u)
                {
                    mantissa <<= 1;
                    ++shift;
                }
                mantissa &= 0x03FFu;
                const u32 floatExponent = 113u - shift;
                return AZStd::bit_cast<float>(sign | (floatExponent << 23) | (mantissa << 13));
            }

            if (exponent == 0x1Fu)
            {
                // Infinity or NaN.
                return AZStd::bit_cast<float>(sign | 0x7F800000u | (mantissa << 13));
            }

            return AZStd::bit_cast<float>(sign | ((exponent - 15u + 127u) << 23) | (mantissa << 13));
        }

        u16 m_bits{ 0 };
    };

    static_assert(sizeof(Half) == 2, "Half must be exactly 2 bytes");
    static_assert(AZStd::is_trivially_copyable_v<Half>, "Half must be trivially copyable");
} // namespace AZ

namespace AZStd
{
    template<>
    struct hash<AZ::Half>
    {
        using result_type = AZStd::size_t;
        constexpr result_type operator()(const AZ::Half& value) const
        {
            return static_cast<result_type>(value.GetBits());
        }
    };
} // namespace AZStd
