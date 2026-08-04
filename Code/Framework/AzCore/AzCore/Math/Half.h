/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Internal/MathTypes.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/bit.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/limits.h>

namespace AZ::Internal
{
    //! Reference software conversions: correctly rounded (to nearest, ties to even), with full
    //! subnormal, signed zero, infinity and NaN handling. These serve constant evaluation and any
    //! configuration without hardware conversion. AZ::Half is the public interface to them.
    constexpr u16 FloatToHalf(float value)
    {
        const u32 bits = AZStd::bit_cast<u32>(value);
        const u32 sign = (bits >> 16) & 0x8000u;
        const u32 biasedExponent = (bits >> 23) & 0xFFu;
        const u32 mantissa = bits & 0x007FFFFFu;

        // Infinity and NaN keep their class.
        // A NaN must stay a NaN, so force a non-zero mantissa rather than letting
        // the truncation below turn a small payload into an infinity.
        if (biasedExponent == 0xFFu)
        {
            u32 halfMantissa = 0u;
            if (mantissa != 0u)
            {
                halfMantissa = 0x0200u | (mantissa >> 13);
            }
            return static_cast<u16>(sign | 0x7C00u | halfMantissa);
        }

        const s32 halfExponent = static_cast<s32>(biasedExponent) - 127 + 15;

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
            if ((withImplicitBit & roundBit) != 0u
                && (((withImplicitBit & (roundBit - 1)) != 0u) || ((result & 1u) != 0u)))
            {
                ++result;
            }
            return static_cast<u16>(sign | result);
        }

        // Normal. A round that carries out of the mantissa correctly increments the exponent,
        // and an exponent that carries into 0x1F correctly yields infinity.
        u32 result = (static_cast<u32>(halfExponent) << 10) | (mantissa >> 13);
        constexpr u32 roundBit = 1u << 12;
        if ((mantissa & roundBit) != 0u
            && (((mantissa & (roundBit - 1)) != 0u) || ((result & 1u) != 0u)))
        {
            ++result;
        }
        return static_cast<u16>(sign | result);
    }

    constexpr float HalfToFloat(u16 half)
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

            // Subnormal: shift the leading 1 up into the implicit bit position (bit 10).
            const u32 shift = static_cast<u32>(AZStd::countl_zero(mantissa)) - 21u;
            mantissa = (mantissa << shift) & 0x03FFu;
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
} // namespace AZ::Internal

namespace AZ
{
    //! An IEEE 754 binary16 ("half precision") floating point value:
    //! 1 sign bit, 5 exponent bits with a bias of 15, and 10 mantissa bits.
    //!
    //! Conversions are correctly rounded (round to nearest, ties to even),
    //! handle the full range of subnormals, signed zero, infinities and NaN, are usable at compile time,
    //! and use the hardware conversion instructions where the toolchain guarantees them.
    //! The hardware paths are bit-exact with the reference conversions under the default floating point
    //! environment; a thread that enables FTZ/DAZ (some third party libraries do) flushes subnormals
    //! through the hardware paths only.
    //!
    //! Range notes: the largest finite value is 65504, the smallest positive normal is 2^-14, and the smallest positive subnormal is 2^-24.
    //! Values that overflow convert to infinity, and values that underflow convert to a signed zero.
    class Half final
    {
    public:
        AZ_TYPE_INFO(Half, "{5C6F2E80-A417-4B93-9D25-8E0F31C7A6B4}");

        constexpr Half() = default;

        AZ_MATH_INLINE explicit constexpr Half(float value)
            : m_bits(FromFloat(value))
        {
        }

        //! Wraps a raw binary16 bit pattern without converting it.
        AZ_MATH_INLINE static constexpr Half FromBits(u16 bits)
        {
            Half result;
            result.m_bits = bits;
            return result;
        }

        AZ_MATH_INLINE constexpr u16 GetBits() const
        {
            return m_bits;
        }

        //! Widening to float. Implicit and exact, since every binary16 value is a float value.
        AZ_MATH_INLINE constexpr operator float() const
        {
            return ToFloat(m_bits);
        }

        static const Half Zero;
        static const Half One;
        static const Half Max;
        static const Half Infinity;
        static const Half NaN;

        AZ_MATH_INLINE constexpr bool IsZero() const
        {
            return (m_bits & 0x7FFF) == 0;
        }
        AZ_MATH_INLINE constexpr bool IsNegative() const
        {
            return (m_bits & 0x8000) != 0;
        }
        AZ_MATH_INLINE constexpr bool IsInfinity() const
        {
            return (m_bits & 0x7FFF) == 0x7C00;
        }
        AZ_MATH_INLINE constexpr bool IsNaN() const
        {
            return (m_bits & 0x7FFF) > 0x7C00;
        }
        AZ_MATH_INLINE constexpr bool IsFinite() const
        {
            return (m_bits & 0x7C00) != 0x7C00;
        }

        //! IEEE equality, consistent with every other comparison and arithmetic operator,
        //! which reach float through the implicit conversion: +0 equals -0, and NaN does not equal itself.
        //! Compare GetBits() when representational equality is wanted.
        AZ_MATH_INLINE constexpr bool operator==(Half rhs) const
        {
            return static_cast<float>(*this) == static_cast<float>(rhs);
        }
        AZ_MATH_INLINE constexpr bool operator!=(Half rhs) const
        {
            return !(*this == rhs);
        }

        //! Converts a float to a binary16 bit pattern, rounding to nearest with ties to even.
        AZ_MATH_INLINE static constexpr u16 FromFloat(float value)
        {
            if (az_builtin_is_constant_evaluated())
            {
                return Internal::FloatToHalf(value);
            }

#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            return AZStd::bit_cast<u16>(static_cast<__fp16>(value));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            return static_cast<u16>(_mm_cvtsi128_si32(_mm_cvtps_ph(_mm_set_ss(value), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)));
#else
            return Internal::FloatToHalf(value);
#endif
        }

        //! Converts a binary16 bit pattern to a float. Always exact.
        AZ_MATH_INLINE static constexpr float ToFloat(u16 half)
        {
            if (az_builtin_is_constant_evaluated())
            {
                return Internal::HalfToFloat(half);
            }

#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            return static_cast<float>(AZStd::bit_cast<__fp16>(half));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            return _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128(half)));
#else
            return Internal::HalfToFloat(half);
#endif
        }

    private:
        u16 m_bits = 0;
    };

    inline constexpr Half Half::Zero = Half::FromBits(0x0000);
    inline constexpr Half Half::One = Half::FromBits(0x3C00);
    inline constexpr Half Half::Max = Half::FromBits(0x7BFF);
    inline constexpr Half Half::Infinity = Half::FromBits(0x7C00);
    inline constexpr Half Half::NaN = Half::FromBits(0x7E00);
} // namespace AZ

template<>
class std::numeric_limits<AZ::Half>
{
public:
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr float_denorm_style has_denorm = denorm_present;
    static constexpr bool has_denorm_loss = false;
    static constexpr bool is_iec559 = true;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr int digits = 11;
    static constexpr int digits10 = 3;
    static constexpr int max_digits10 = 5;
    static constexpr int radix = 2;
    static constexpr int min_exponent = -13;
    static constexpr int min_exponent10 = -4;
    static constexpr int max_exponent = 16;
    static constexpr int max_exponent10 = 4;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_to_nearest;

    static constexpr AZ::Half min() noexcept
    {
        return AZ::Half::FromBits(0x0400); // 2^-14, the smallest positive normal
    }
    static constexpr AZ::Half lowest() noexcept
    {
        return AZ::Half::FromBits(0xFBFF); // -65504
    }
    static constexpr AZ::Half max() noexcept
    {
        return AZ::Half::FromBits(0x7BFF); // 65504
    }
    static constexpr AZ::Half epsilon() noexcept
    {
        return AZ::Half::FromBits(0x1400); // 2^-10
    }
    static constexpr AZ::Half round_error() noexcept
    {
        return AZ::Half::FromBits(0x3800); // 0.5
    }
    static constexpr AZ::Half infinity() noexcept
    {
        return AZ::Half::FromBits(0x7C00);
    }
    static constexpr AZ::Half quiet_NaN() noexcept
    {
        return AZ::Half::FromBits(0x7E00);
    }
    static constexpr AZ::Half signaling_NaN() noexcept
    {
        return AZ::Half::FromBits(0x7D00);
    }
    static constexpr AZ::Half denorm_min() noexcept
    {
        return AZ::Half::FromBits(0x0001); // 2^-24
    }
};

template<>
struct AZStd::hash<AZ::Half>
{
    constexpr size_t operator()(const AZ::Half value) const
    {
        // +0 and -0 compare equal, so they must hash equal.
        AZ::u16 bits = value.GetBits();
        if (value.IsZero())
        {
            bits = 0;
        }
        return static_cast<size_t>(bits);
    }
};
