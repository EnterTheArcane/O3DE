/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Colors.h>
#include <AzCore/Math/MathUtils.h>

namespace AZ
{
    //! A 64-bit color: 4 unsigned 16-bit channels, normalized over [0, 65535], with no color space association.
    //! Maps directly to RHI::Format::R16G16B16A16_UNORM, and is aligned as one 64-bit word.
    //!
    //! This is the high precision *low dynamic range* color. It cannot represent values outside [0, 1].
    //! For HDR use ColorHalf, which stores IEEE binary16 and maps to R16G16B16A16_FLOAT.
    //!
    //! Conversions follow the rule that widening is implicit and narrowing is explicit:
    //!
    //!     Color64 wide = AZ::Colors::White; // implicit, the named constants are narrower
    //!     Color full = wide; // implicit, u16 to float is exact
    //!     Color64 narrowed{someColor}; // explicit, float to u16 rounds and clamps
    class Color64 final
    {
    public:
        AZ_TYPE_INFO(Color64, "{2F84C7E1-9B36-4A58-8D07-C41E5B93A6D2}");

        static AZCORE_API void Reflect(ReflectContext* context);

        //! Largest representable channel value. A channel of MaxChannel means 1.0.
        static constexpr u16 MaxChannel = AZStd::numeric_limits<u16>::max();

        constexpr Color64() = default;

        AZ_MATH_INLINE constexpr Color64(u16 r, u16 g, u16 b, u16 a)
            : m_r(r)
            , m_g(g)
            , m_b(b)
            , m_a(a)
        {
        }

        AZ_MATH_INLINE constexpr Color64(u16 r, u16 g, u16 b)
            : Color64(r, g, b, MaxChannel)
        {
        }

        //! Widening from a named color constant.
        //! Implicit and exact, because an 8-bit channel expands to 16 bits by bit replication (v * 257),
        //! so 0xFF becomes 0xFFFF rather than 0xFF00.
        AZ_MATH_INLINE constexpr Color64(const Internal::NamedColor& color)
            : m_r(static_cast<u16>(color.m_r * 257))
            , m_g(static_cast<u16>(color.m_g * 257))
            , m_b(static_cast<u16>(color.m_b * 257))
            , m_a(static_cast<u16>(color.m_a * 257))
        {
        }

        //! Narrowing from a floating point color. Explicit, rounds to nearest and clamps to [0, 1].
        AZ_MATH_INLINE explicit Color64(const Color& color)
        {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t clamped = ClampUnitInterval4(color.GetAsVector4().GetSimdValue());
            m_packed = PackRounded(vmulq_n_f32(clamped, 65535.0f));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 clamped = ClampUnitInterval4(color.GetAsVector4().GetSimdValue());
            m_packed = PackRounded(_mm_mul_ps(clamped, _mm_set1_ps(65535.0f)));
#else
            m_r = FromFloat(color.GetR());
            m_g = FromFloat(color.GetG());
            m_b = FromFloat(color.GetB());
            m_a = FromFloat(color.GetA());
#endif
        }

        static const Color64 Zero;
        static const Color64 One;

        //! @name Channel access, as 16-bit integers.
        //! @{
        AZ_MATH_INLINE constexpr u16 GetR16() const
        {
            return m_r;
        }
        AZ_MATH_INLINE constexpr u16 GetG16() const
        {
            return m_g;
        }
        AZ_MATH_INLINE constexpr u16 GetB16() const
        {
            return m_b;
        }
        AZ_MATH_INLINE constexpr u16 GetA16() const
        {
            return m_a;
        }

        AZ_MATH_INLINE constexpr void SetR16(u16 r)
        {
            m_r = r;
        }
        AZ_MATH_INLINE constexpr void SetG16(u16 g)
        {
            m_g = g;
        }
        AZ_MATH_INLINE constexpr void SetB16(u16 b)
        {
            m_b = b;
        }
        AZ_MATH_INLINE constexpr void SetA16(u16 a)
        {
            m_a = a;
        }
        //! @}

        //! @name Channel access, normalized to [0, 1].
        //! @{
        AZ_MATH_INLINE constexpr float GetR() const
        {
            return ToFloat(m_r);
        }
        AZ_MATH_INLINE constexpr float GetG() const
        {
            return ToFloat(m_g);
        }
        AZ_MATH_INLINE constexpr float GetB() const
        {
            return ToFloat(m_b);
        }
        AZ_MATH_INLINE constexpr float GetA() const
        {
            return ToFloat(m_a);
        }

        AZ_MATH_INLINE constexpr void SetR(float r)
        {
            m_r = FromFloat(r);
        }
        AZ_MATH_INLINE constexpr void SetG(float g)
        {
            m_g = FromFloat(g);
        }
        AZ_MATH_INLINE constexpr void SetB(float b)
        {
            m_b = FromFloat(b);
        }
        AZ_MATH_INLINE constexpr void SetA(float a)
        {
            m_a = FromFloat(a);
        }
        //! @}

        //! Widening to a floating point color. Implicit and exact.
        AZ_MATH_INLINE operator Color() const
        {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            return Color(Vector4(vmulq_n_f32(WidenToFloat4(m_packed), 1.0f / 65535.0f)));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            return Color(Vector4(_mm_mul_ps(WidenToFloat4(m_packed), _mm_set1_ps(1.0f / 65535.0f))));
#else
            return {
                GetR(),
                GetG(),
                GetB(),
                GetA(),
            };
#endif
        }

        //! @name Conversion to the vector types.
        //! Not constexpr, because AZ::Vector3 and AZ::Vector4 are not literal types.
        //! @{
        AZ_MATH_INLINE Vector3 ToVector3() const
        {
            return {
                GetR(),
                GetG(),
                GetB(),
            };
        }

        AZ_MATH_INLINE Vector4 ToVector4() const
        {
            return Vector4(GetR(), GetG(), GetB(), GetA());
        }
        //! @}

        AZ_MATH_INLINE constexpr Color64 WithAlpha(u16 a) const
        {
            return {m_r, m_g, m_b, a};
        }

        AZ_MATH_INLINE constexpr Color64 Inverted() const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    static_cast<u16>(MaxChannel - m_r),
                    static_cast<u16>(MaxChannel - m_g),
                    static_cast<u16>(MaxChannel - m_b),
                    m_a,
                };
            }
            // MaxChannel - x is x ^ MaxChannel, and alpha stays.
            Color64 result;
            result.m_packed = m_packed ^ 0x0000FFFFFFFFFFFFull;
            return result;
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear channel values.
        AZ_MATH_INLINE constexpr float GetLuminance() const
        {
            return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB();
        }

        AZ_MATH_INLINE constexpr u16 GetMaxComponent() const
        {
            return GetMax(GetMax(m_r, m_g), m_b);
        }
        AZ_MATH_INLINE constexpr u16 GetMinComponent() const
        {
            return GetMin(GetMin(m_r, m_g), m_b);
        }

        AZ_MATH_INLINE constexpr bool IsOpaque() const
        {
            return m_a == MaxChannel;
        }

        AZ_MATH_INLINE constexpr bool IsTransparent() const
        {
            return m_a == 0;
        }

        //! Linear interpolation towards @p dest. @p t is clamped to the range [0, 1] because the
        //! result must land in the representable [0, 1] range; ColorHalf::Lerp extrapolates instead.
        AZ_MATH_INLINE constexpr Color64 Lerp(const Color64 dest, float t) const
        {
            const float clamped = ClampUnitInterval(t);
            if (az_builtin_is_constant_evaluated())
            {
                const float r = static_cast<float>(m_r);
                const float g = static_cast<float>(m_g);
                const float b = static_cast<float>(m_b);
                const float a = static_cast<float>(m_a);
                return {
                    static_cast<u16>(r + (static_cast<float>(dest.m_r) - r) * clamped + 0.5f),
                    static_cast<u16>(g + (static_cast<float>(dest.m_g) - g) * clamped + 0.5f),
                    static_cast<u16>(b + (static_cast<float>(dest.m_b) - b) * clamped + 0.5f),
                    static_cast<u16>(a + (static_cast<float>(dest.m_a) - a) * clamped + 0.5f),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t src4 = WidenToFloat4(m_packed);
            const float32x4_t dest4 = WidenToFloat4(dest.m_packed);
            Color64 result;
            result.m_packed = PackRounded(vaddq_f32(src4, vmulq_n_f32(vsubq_f32(dest4, src4), clamped)));
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 src4 = WidenToFloat4(m_packed);
            const __m128 dest4 = WidenToFloat4(dest.m_packed);
            Color64 result;
            result.m_packed = PackRounded(_mm_add_ps(src4, _mm_mul_ps(_mm_sub_ps(dest4, src4), _mm_set1_ps(clamped))));
            return result;
#else
            const float r = static_cast<float>(m_r);
            const float g = static_cast<float>(m_g);
            const float b = static_cast<float>(m_b);
            const float a = static_cast<float>(m_a);
            return {
                static_cast<u16>(r + (static_cast<float>(dest.m_r) - r) * clamped + 0.5f),
                static_cast<u16>(g + (static_cast<float>(dest.m_g) - g) * clamped + 0.5f),
                static_cast<u16>(b + (static_cast<float>(dest.m_b) - b) * clamped + 0.5f),
                static_cast<u16>(a + (static_cast<float>(dest.m_a) - a) * clamped + 0.5f),
            };
#endif
        }

        //! Source over alpha compositing of @p src on top of this color:
        //! channel = src * srcAlpha + dst * (1 - srcAlpha), alpha = Porter-Duff over.
        //! The channel math treats this color as opaque, so it is an approximation when it is not.
        AZ_MATH_INLINE constexpr Color64 Over(const Color64 src) const
        {
            // Modulate(sa, MaxChannel) == sa, so the alpha lane comes out right and its sum cannot saturate.
            const u16 sa = src.m_a;
            const u16 inv = static_cast<u16>(MaxChannel - sa);
            return (src * Color64(sa, sa, sa, MaxChannel)) + (*this * Color64(inv, inv, inv, inv));
        }

        //! Multiplies r, g and b by alpha.
        AZ_MATH_INLINE constexpr Color64 Premultiply() const
        {
            return *this * Color64(m_a, m_a, m_a, MaxChannel);
        }

        //! @name Arithmetic.
        //! Integer color arithmetic is not float color arithmetic. Addition and subtraction
        //! **saturate** rather than wrap, and color times color modulates, i.e. (a * b) / MaxChannel.
        //! @{
        AZ_MATH_INLINE constexpr Color64 operator+(const Color64 rhs) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    SaturatingAdd(m_r, rhs.m_r),
                    SaturatingAdd(m_g, rhs.m_g),
                    SaturatingAdd(m_b, rhs.m_b),
                    SaturatingAdd(m_a, rhs.m_a),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const uint16x4_t lhs = vreinterpret_u16_u64(vdup_n_u64(m_packed));
            const uint16x4_t rhs16 = vreinterpret_u16_u64(vdup_n_u64(rhs.m_packed));
            Color64 result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_u16(vqadd_u16(lhs, rhs16)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i lhs = _mm_cvtsi64_si128(static_cast<s64>(m_packed));
            const __m128i rhs16 = _mm_cvtsi64_si128(static_cast<s64>(rhs.m_packed));
            Color64 result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(_mm_adds_epu16(lhs, rhs16)));
            return result;
#else
            return {
                SaturatingAdd(m_r, rhs.m_r),
                SaturatingAdd(m_g, rhs.m_g),
                SaturatingAdd(m_b, rhs.m_b),
                SaturatingAdd(m_a, rhs.m_a),
            };
#endif
        }

        AZ_MATH_INLINE constexpr Color64 operator-(const Color64 rhs) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    SaturatingSubtract(m_r, rhs.m_r),
                    SaturatingSubtract(m_g, rhs.m_g),
                    SaturatingSubtract(m_b, rhs.m_b),
                    SaturatingSubtract(m_a, rhs.m_a),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const uint16x4_t lhs = vreinterpret_u16_u64(vdup_n_u64(m_packed));
            const uint16x4_t rhs16 = vreinterpret_u16_u64(vdup_n_u64(rhs.m_packed));
            Color64 result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_u16(vqsub_u16(lhs, rhs16)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i lhs = _mm_cvtsi64_si128(static_cast<s64>(m_packed));
            const __m128i rhs16 = _mm_cvtsi64_si128(static_cast<s64>(rhs.m_packed));
            Color64 result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(_mm_subs_epu16(lhs, rhs16)));
            return result;
#else
            return {
                SaturatingSubtract(m_r, rhs.m_r),
                SaturatingSubtract(m_g, rhs.m_g),
                SaturatingSubtract(m_b, rhs.m_b),
                SaturatingSubtract(m_a, rhs.m_a),
            };
#endif
        }

        AZ_MATH_INLINE constexpr Color64 operator*(const Color64 rhs) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    Modulate(m_r, rhs.m_r),
                    Modulate(m_g, rhs.m_g),
                    Modulate(m_b, rhs.m_b),
                    Modulate(m_a, rhs.m_a),
                };
            }
            // (u + (u >> 16)) >> 16 with u = t + 32768 equals Modulate for every product. Verified exhaustively.
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const uint16x4_t lhs = vreinterpret_u16_u64(vdup_n_u64(m_packed));
            const uint16x4_t rhs16 = vreinterpret_u16_u64(vdup_n_u64(rhs.m_packed));
            uint32x4_t product = vmull_u16(lhs, rhs16);
            product = vaddq_u32(product, vdupq_n_u32(32768));
            product = vsraq_n_u32(product, product, 16);
            Color64 result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_u16(vshrn_n_u32(product, 16)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i lhs = _mm_cvtepu16_epi32(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128i rhs32 = _mm_cvtepu16_epi32(_mm_cvtsi64_si128(static_cast<s64>(rhs.m_packed)));
            __m128i product = _mm_mullo_epi32(lhs, rhs32);
            product = _mm_add_epi32(product, _mm_set1_epi32(32768));
            product = _mm_add_epi32(product, _mm_srli_epi32(product, 16));
            product = _mm_srli_epi32(product, 16);
            Color64 result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(_mm_packus_epi32(product, product)));
            return result;
#else
            return {
                Modulate(m_r, rhs.m_r),
                Modulate(m_g, rhs.m_g),
                Modulate(m_b, rhs.m_b),
                Modulate(m_a, rhs.m_a),
            };
#endif
        }

        AZ_MATH_INLINE constexpr Color64 operator*(float scale) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    FromFloat(ToFloat(m_r) * scale),
                    FromFloat(ToFloat(m_g) * scale),
                    FromFloat(ToFloat(m_b) * scale),
                    FromFloat(ToFloat(m_a) * scale),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t normalized = vmulq_n_f32(WidenToFloat4(m_packed), 1.0f / 65535.0f);
            const float32x4_t clamped = ClampUnitInterval4(vmulq_n_f32(normalized, scale));
            Color64 result;
            result.m_packed = PackRounded(vmulq_n_f32(clamped, 65535.0f));
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 normalized = _mm_mul_ps(WidenToFloat4(m_packed), _mm_set1_ps(1.0f / 65535.0f));
            const __m128 clamped = ClampUnitInterval4(_mm_mul_ps(normalized, _mm_set1_ps(scale)));
            Color64 result;
            result.m_packed = PackRounded(_mm_mul_ps(clamped, _mm_set1_ps(65535.0f)));
            return result;
#else
            return {
                FromFloat(ToFloat(m_r) * scale),
                FromFloat(ToFloat(m_g) * scale),
                FromFloat(ToFloat(m_b) * scale),
                FromFloat(ToFloat(m_a) * scale),
            };
#endif
        }

        AZ_MATH_INLINE constexpr Color64 operator/(float divisor) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    FromFloat(ToFloat(m_r) / divisor),
                    FromFloat(ToFloat(m_g) / divisor),
                    FromFloat(ToFloat(m_b) / divisor),
                    FromFloat(ToFloat(m_a) / divisor),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t normalized = vmulq_n_f32(WidenToFloat4(m_packed), 1.0f / 65535.0f);
            const float32x4_t clamped = ClampUnitInterval4(vdivq_f32(normalized, vdupq_n_f32(divisor)));
            Color64 result;
            result.m_packed = PackRounded(vmulq_n_f32(clamped, 65535.0f));
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 normalized = _mm_mul_ps(WidenToFloat4(m_packed), _mm_set1_ps(1.0f / 65535.0f));
            const __m128 clamped = ClampUnitInterval4(_mm_div_ps(normalized, _mm_set1_ps(divisor)));
            Color64 result;
            result.m_packed = PackRounded(_mm_mul_ps(clamped, _mm_set1_ps(65535.0f)));
            return result;
#else
            return {
                FromFloat(ToFloat(m_r) / divisor),
                FromFloat(ToFloat(m_g) / divisor),
                FromFloat(ToFloat(m_b) / divisor),
                FromFloat(ToFloat(m_a) / divisor),
            };
#endif
        }

        AZ_MATH_INLINE constexpr Color64& operator+=(const Color64 rhs)
        {
            *this = *this + rhs;
            return *this;
        }
        AZ_MATH_INLINE constexpr Color64& operator-=(const Color64 rhs)
        {
            *this = *this - rhs;
            return *this;
        }
        AZ_MATH_INLINE constexpr Color64& operator*=(const Color64 rhs)
        {
            *this = *this * rhs;
            return *this;
        }
        AZ_MATH_INLINE constexpr Color64& operator*=(float scale)
        {
            *this = *this * scale;
            return *this;
        }
        AZ_MATH_INLINE constexpr Color64& operator/=(float divisor)
        {
            *this = *this / divisor;
            return *this;
        }

        //! A friend so the scalar can appear on either side of the multiply.
        AZ_MATH_INLINE friend constexpr Color64 operator*(float scale, const Color64 color)
        {
            return color * scale;
        }
        //! @}

        AZ_MATH_INLINE constexpr bool operator==(const Color64 rhs) const
        {
            return m_r == rhs.m_r
                && m_g == rhs.m_g
                && m_b == rhs.m_b
                && m_a == rhs.m_a;
        }

        AZ_MATH_INLINE constexpr bool operator!=(const Color64 rhs) const
        {
            return !(*this == rhs);
        }

        //! @name Ordering, so a color can be used as an associative container key.
        //! The order is stable and total but arbitrary. It is not perceptual.
        //! @{
        AZ_MATH_INLINE constexpr bool operator<(const Color64 rhs) const
        {
            const u64 lhsPacked =
                (static_cast<u64>(m_r) << 48)
                | (static_cast<u64>(m_g) << 32)
                | (static_cast<u64>(m_b) << 16)
                | static_cast<u64>(m_a);
            const u64 rhsPacked =
                (static_cast<u64>(rhs.m_r) << 48)
                | (static_cast<u64>(rhs.m_g) << 32)
                | (static_cast<u64>(rhs.m_b) << 16)
                | static_cast<u64>(rhs.m_a);
            return lhsPacked < rhsPacked;
        }
        AZ_MATH_INLINE constexpr bool operator>(const Color64 rhs) const
        {
            return rhs < *this;
        }
        AZ_MATH_INLINE constexpr bool operator<=(const Color64 rhs) const
        {
            return !(rhs < *this);
        }
        AZ_MATH_INLINE constexpr bool operator>=(const Color64 rhs) const
        {
            return !(*this < rhs);
        }
        //! @}

    private:
        AZ_MATH_INLINE static constexpr float ToFloat(u16 v)
        {
            return static_cast<float>(v) * (1.0f / 65535.0f);
        }

        //! Clamps to the range [0, 1] with defined behavior for NaN.
        //!
        //! AZ::GetClamp cannot be used here.
        //! It tests `value < min` then `value > max`, both of which are false for NaN,
        //! so it returns NaN unchanged, and static_cast<u16> of NaN is undefined behavior.
        //! Ordering the tests so that an unordered input falls through to 0
        //! makes a NaN channel deterministically transparent black instead.
        AZ_MATH_INLINE static constexpr float ClampUnitInterval(float v)
        {
            if (v > 0.0f)
            {
                if (v < 1.0f)
                {
                    return v;
                }
                return 1.0f;
            }
            return 0.0f;
        }

        AZ_MATH_INLINE static constexpr u16 FromFloat(float v)
        {
            return static_cast<u16>(ClampUnitInterval(v) * 65535.0f + 0.5f);
        }

        //! A wrapping add plus overflow test is the shape compilers recognize as a saturating add and vectorize.
        AZ_MATH_INLINE static constexpr u16 SaturatingAdd(u16 a, u16 b)
        {
            const u16 sum = static_cast<u16>(a + b);
            if (sum < a)
            {
                return MaxChannel;
            }
            return sum;
        }

        AZ_MATH_INLINE static constexpr u16 SaturatingSubtract(u16 a, u16 b)
        {
            if (a > b)
            {
                return static_cast<u16>(a - b);
            }
            return 0;
        }

        //! Exact normalized multiply: round(a * b / 65535).
        AZ_MATH_INLINE static constexpr u16 Modulate(u16 a, u16 b)
        {
            return static_cast<u16>((static_cast<u32>(a) * static_cast<u32>(b) + 32767u) / 65535u);
        }

#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
        //! The four packed channels as raw values in float lanes 0-3.
        AZ_MATH_INLINE static float32x4_t WidenToFloat4(u64 packed)
        {
            const uint16x4_t words = vreinterpret_u16_u64(vdup_n_u64(packed));
            return vcvtq_f32_u32(vmovl_u16(words));
        }

        //! Rounds four raw channel values in [0, 65535] back into packed words, matching FromFloat's rounding.
        AZ_MATH_INLINE static u64 PackRounded(float32x4_t channels)
        {
            const uint32x4_t truncated = vcvtq_u32_f32(vaddq_f32(channels, vdupq_n_f32(0.5f)));
            return vget_lane_u64(vreinterpret_u64_u16(vmovn_u32(truncated)), 0);
        }

        //! ClampUnitInterval on four lanes. The compare fails for a NaN lane, signaling or quiet,
        //! so it selects 0 exactly like the scalar clamp. FMAXNM would pass a signaling NaN through.
        AZ_MATH_INLINE static float32x4_t ClampUnitInterval4(float32x4_t v)
        {
            const uint32x4_t positive = vcgtq_f32(v, vdupq_n_f32(0.0f));
            const float32x4_t upperClamped = vminq_f32(v, vdupq_n_f32(1.0f));
            return vbslq_f32(positive, upperClamped, vdupq_n_f32(0.0f));
        }
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
        //! The four packed channels as raw values in float lanes 0-3.
        AZ_MATH_INLINE static __m128 WidenToFloat4(u64 packed)
        {
            return _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_cvtsi64_si128(static_cast<s64>(packed))));
        }

        //! Rounds four raw channel values in [0, 65535] back into packed words, matching FromFloat's rounding.
        AZ_MATH_INLINE static u64 PackRounded(__m128 channels)
        {
            const __m128i truncated = _mm_cvttps_epi32(_mm_add_ps(channels, _mm_set1_ps(0.5f)));
            return static_cast<u64>(_mm_cvtsi128_si64(_mm_packus_epi32(truncated, truncated)));
        }

        //! ClampUnitInterval on four lanes. maxps returns its second operand for a NaN lane, so a NaN becomes 0.
        AZ_MATH_INLINE static __m128 ClampUnitInterval4(__m128 v)
        {
            return _mm_min_ps(_mm_max_ps(v, _mm_setzero_ps()), _mm_set1_ps(1.0f));
        }
#endif

        union
        {
            struct
            {
                u16 m_r = 0;
                u16 m_g = 0;
                u16 m_b = 0;
                u16 m_a = MaxChannel;
            };
            u64 m_packed;
        };
    };

    inline constexpr Color64 Color64::Zero{0, 0, 0, 0};
    inline constexpr Color64 Color64::One{
        Color64::MaxChannel,
        Color64::MaxChannel,
        Color64::MaxChannel,
        Color64::MaxChannel,
    };
} // namespace AZ

template<>
struct AZStd::hash<AZ::Color64>
{
    constexpr size_t operator()(const AZ::Color64 value) const
    {
        return static_cast<size_t>(
            (static_cast<AZ::u64>(value.GetR16()) << 48)
            | (static_cast<AZ::u64>(value.GetG16()) << 32)
            | (static_cast<AZ::u64>(value.GetB16()) << 16)
            | static_cast<AZ::u64>(value.GetA16()));
    }
};
