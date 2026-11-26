/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/bit.h>
#include <AzCore/std/concepts/concepts.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/limits.h>

namespace AZ::Internal
{
    template<class T>
    concept HalfNumeric = AZStd::same_as<T, float> || AZStd::same_as<T, double>
        || (AZStd::integral<T> && !AZStd::same_as<T, bool> && sizeof(T) <= sizeof(u64));

    static_assert(sizeof(float) == 4 && AZStd::numeric_limits<float>::is_iec559);
    static_assert(sizeof(double) == 8 && AZStd::numeric_limits<double>::is_iec559);

    // Callers bound the shift to [1, 53], including the half-minimum-subnormal midpoint.
    constexpr u64 RoundHalfSignificand(u64 significand, u32 shift) noexcept
    {
        u64 result = significand >> shift;
        const u64 midpoint = u64{1} << (shift - 1);
        const u64 remainder = significand & ((midpoint << 1) - 1);
        if (remainder > midpoint || (remainder == midpoint && (result & 1) != 0))
        {
            ++result;
        }
        return result;
    }

    // Integer-only encoding makes nearest-even conversion independent of rounding modes and FTZ/DAZ.
    template<u32 MantissaBits, u32 ExponentBits, s32 ExponentBias, class UInt>
    constexpr u16 EncodeHalfBits(UInt bits) noexcept
    {
        const u16 sign = static_cast<u16>((bits >> (MantissaBits + ExponentBits - 15)) & 0x8000);
        constexpr UInt exponentMask = (UInt{1} << ExponentBits) - 1;
        const UInt exponent = (bits >> MantissaBits) & exponentMask;
        const UInt mantissa = bits & ((UInt{1} << MantissaBits) - 1);
        if (exponent == exponentMask)
        {
            if (mantissa != 0)
            {
                return static_cast<u16>(sign | 0x7e00); // Numerical NaNs are quiet, payloads are unspecified.
            }
            return static_cast<u16>(sign | 0x7c00);
        }

        const s32 halfExponent = static_cast<s32>(exponent) - ExponentBias + 15;
        if (halfExponent >= 31)
        {
            return static_cast<u16>(sign | 0x7c00);
        }
        if (halfExponent < -10)
        {
            return sign;
        }
        if (halfExponent <= 0)
        {
            const u64 significand = static_cast<u64>(mantissa) | (u64{1} << MantissaBits);
            const u32 shift = static_cast<u32>(static_cast<s32>(MantissaBits) - 9 - halfExponent);
            return static_cast<u16>(sign | RoundHalfSignificand(significand, shift));
        }

        // Addition lets rounding carry into the exponent, including the overflow boundary at 65520.
        const u64 rounded = (static_cast<u64>(halfExponent) << 10) + RoundHalfSignificand(mantissa, MantissaBits - 10);
        return static_cast<u16>(sign | rounded);
    }

    constexpr u16 FloatToHalf(float value) noexcept
    {
        return EncodeHalfBits<23, 8, 127>(AZStd::bit_cast<u32>(value));
    }

    constexpr u16 DoubleToHalf(double value) noexcept
    {
        return EncodeHalfBits<52, 11, 1023>(AZStd::bit_cast<u64>(value));
    }

    constexpr float HalfToFloat(u16 bits) noexcept
    {
        const u32 sign = static_cast<u32>(bits & 0x8000) << 16;
        const u32 exponent = (bits >> 10) & 0x1f;
        u32 mantissa = bits & 0x03ff;
        if (exponent == 0)
        {
            if (mantissa == 0)
            {
                return AZStd::bit_cast<float>(sign);
            }
            const u32 shift = static_cast<u32>(AZStd::countl_zero(mantissa)) - 21;
            mantissa = (mantissa << shift) & 0x03ff;
            return AZStd::bit_cast<float>(sign | ((113 - shift) << 23) | (mantissa << 13));
        }
        if (exponent == 31)
        {
            if (mantissa != 0)
            {
                mantissa |= 0x0200;
            }
            return AZStd::bit_cast<float>(sign | 0x7f800000 | (mantissa << 13));
        }
        return AZStd::bit_cast<float>(sign | ((exponent + 112) << 23) | (mantissa << 13));
    }
}

namespace AZ
{
    //! Binary16 storage with explicit nearest-even narrowing and exact finite widening.
    //! Binary arithmetic promotes to float. Compound assignment rounds once when storing its result.
    //! Raw bits use native byte order in memory. This type does not define a wire format.
    class Half
    {
    public:
        constexpr Half() noexcept = default;

        explicit constexpr Half(float value) noexcept
            : m_bits(Internal::FloatToHalf(value))
        {
        }

        explicit constexpr Half(double value) noexcept
            : m_bits(Internal::DoubleToHalf(value))
        {
        }

        template<AZStd::signed_integral T>
            requires Internal::HalfNumeric<T>
        explicit constexpr Half(T value) noexcept
        {
            // Bound integers before conversion. Every remaining value is exactly representable in float.
            if (static_cast<s64>(value) <= -65520)
            {
                m_bits = 0xfc00;
                return;
            }
            if (static_cast<s64>(value) >= 65520)
            {
                m_bits = 0x7c00;
                return;
            }
            m_bits = Internal::FloatToHalf(static_cast<float>(value));
        }

        template<AZStd::unsigned_integral T>
            requires Internal::HalfNumeric<T>
        explicit constexpr Half(T value) noexcept
        {
            if (static_cast<u64>(value) >= 65520)
            {
                m_bits = 0x7c00;
                return;
            }
            m_bits = Internal::FloatToHalf(static_cast<float>(value));
        }

        // Keep argument conversions from admitting unsupported source types through float or double.
        template<class T>
            requires (!Internal::HalfNumeric<T> && !AZStd::same_as<T, Half>)
        explicit Half(T) = delete;

        static constexpr Half FromBits(u16 bits) noexcept
        {
            Half value;
            value.m_bits = bits;
            return value;
        }

        constexpr u16 GetBits() const noexcept { return m_bits; }
        constexpr float ToFloat() const noexcept { return Internal::HalfToFloat(m_bits); }
        constexpr operator float() const noexcept { return ToFloat(); }

        constexpr bool IsZero() const noexcept { return (m_bits & 0x7fff) == 0; }
        //! Tests the sign bit, including negative zero and negative NaN encodings.
        constexpr bool IsNegative() const noexcept { return (m_bits & 0x8000) != 0; }
        constexpr bool IsFinite() const noexcept { return (m_bits & 0x7c00) != 0x7c00; }
        constexpr bool IsInfinity() const noexcept { return (m_bits & 0x7fff) == 0x7c00; }
        constexpr bool IsNaN() const noexcept { return (m_bits & 0x7fff) > 0x7c00; }
        constexpr bool IsNormal() const noexcept { return (m_bits & 0x7c00) != 0 && IsFinite(); }
        constexpr bool IsSubnormal() const noexcept { return (m_bits & 0x7c00) == 0 && !IsZero(); }

        constexpr Half operator+() const noexcept { return *this; }
        constexpr Half operator-() const noexcept { return FromBits(static_cast<u16>(m_bits ^ 0x8000)); }

        template<class T>
            requires (Internal::HalfNumeric<T> || AZStd::same_as<T, Half>)
        constexpr Half& operator+=(T rhs) noexcept
        {
            *this = Half(ToFloat() + rhs);
            return *this;
        }

        template<class T>
            requires (Internal::HalfNumeric<T> || AZStd::same_as<T, Half>)
        constexpr Half& operator-=(T rhs) noexcept
        {
            *this = Half(ToFloat() - rhs);
            return *this;
        }

        template<class T>
            requires (Internal::HalfNumeric<T> || AZStd::same_as<T, Half>)
        constexpr Half& operator*=(T rhs) noexcept
        {
            *this = Half(ToFloat() * rhs);
            return *this;
        }

        template<class T>
            requires (Internal::HalfNumeric<T> || AZStd::same_as<T, Half>)
        constexpr Half& operator/=(T rhs) noexcept
        {
            *this = Half(ToFloat() / rhs);
            return *this;
        }

        static const Half Zero;
        static const Half One;

    private:
        u16 m_bits{};
    };

    inline constexpr Half Half::Zero = Half::FromBits(0);
    inline constexpr Half Half::One = Half::FromBits(0x3c00);
    static_assert(sizeof(Half) == 2 && alignof(Half) == 2);
}

template<>
class std::numeric_limits<AZ::Half>
{
public:
    static constexpr bool is_specialized = true;
    static constexpr AZ::Half min() noexcept { return AZ::Half::FromBits(0x0400); }
    static constexpr AZ::Half max() noexcept { return AZ::Half::FromBits(0x7bff); }
    static constexpr AZ::Half lowest() noexcept { return AZ::Half::FromBits(0xfbff); }
    static constexpr int digits = 11;
    static constexpr int digits10 = 3;
    static constexpr int max_digits10 = 5;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr int radix = 2;
    static constexpr AZ::Half epsilon() noexcept { return AZ::Half::FromBits(0x1400); }
    static constexpr AZ::Half round_error() noexcept { return AZ::Half::FromBits(0x3800); }
    static constexpr int min_exponent = -13;
    static constexpr int min_exponent10 = -4;
    static constexpr int max_exponent = 16;
    static constexpr int max_exponent10 = 4;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr std::float_denorm_style has_denorm = std::denorm_present;
    static constexpr bool has_denorm_loss = false;
    static constexpr AZ::Half infinity() noexcept { return AZ::Half::FromBits(0x7c00); }
    static constexpr AZ::Half quiet_NaN() noexcept { return AZ::Half::FromBits(0x7e00); }
    static constexpr AZ::Half signaling_NaN() noexcept { return AZ::Half::FromBits(0x7d00); }
    static constexpr AZ::Half denorm_min() noexcept { return AZ::Half::FromBits(1); }
    // Binary16 representation does not imply a complete IEEE arithmetic or exception environment.
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr std::float_round_style round_style = std::round_to_nearest;
};

template<>
struct AZStd::hash<AZ::Half>
{
    constexpr AZStd::size_t operator()(AZ::Half value) const noexcept
    {
        if (value.IsZero())
        {
            return 0;
        }
        return AZStd::hash<AZ::u16>{}(value.GetBits());
    }
};
