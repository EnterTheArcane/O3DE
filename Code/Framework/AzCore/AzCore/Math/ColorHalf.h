/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Color32.h>
#include <AzCore/Math/Color64.h>
#include <AzCore/Math/Colors.h>
#include <AzCore/Math/Half.h>

namespace AZ
{
    //! A 64-bit HDR color: 4 IEEE binary16 channels, with no color space association.
    //! Maps directly to RHI::Format::R16G16B16A16_FLOAT, and is aligned as one 64-bit word.
    //!
    //! Unlike Color32 and Color64 this is *not* normalized. Channels may exceed 1.0,
    //! go negative or be infinite, which is what makes it the right storage for HDR values,
    //! and its arithmetic is ordinary floating point rather than the saturating integer arithmetic those two use.
    //!
    //! Because it is HDR it sits outside the Color32 -> Color64 -> Color widening chain:
    //!
    //!     Color full = someColorHalf; // implicit, binary16 to float is exact
    //!     ColorHalf hdr{someColor}; // explicit, float to binary16 loses precision
    //!     ColorHalf hdr{someColor32}; // explicit, 1/255 is not representable in binary16
    //!     ColorHalf hdr = AZ::Colors::White; // implicit, for palette ergonomics (see note)
    //!
    //! @note Conversion from a named color constant is implicit for palette ergonomics even though it is not exact.
    //!       255/255 is exactly 1.0, but a channel such as 1/255 has no binary16 representation.
    //!       The error is at most one binary16 ulp.
    //!
    //! @note There is deliberately no Over(): correct HDR source over wants premultiplied colors,
    //!       so composite in float and convert back.
    class ColorHalf final
    {
    public:
        AZ_TYPE_INFO(ColorHalf, "{A3E7D419-6C85-4F20-B7D3-1E9A45C806F7}");

        constexpr ColorHalf() = default;

        AZ_MATH_INLINE constexpr ColorHalf(Half r, Half g, Half b, Half a)
            : m_r(r)
            , m_g(g)
            , m_b(b)
            , m_a(a)
        {
        }

        AZ_MATH_INLINE constexpr ColorHalf(float r, float g, float b, float a)
            : m_r(Half(r))
            , m_g(Half(g))
            , m_b(Half(b))
            , m_a(Half(a))
        {
        }

        AZ_MATH_INLINE constexpr ColorHalf(float r, float g, float b)
            : ColorHalf(r, g, b, 1.0f)
        {
        }

        //! Widening from a named color constant. Implicit. See the note above about exactness.
        AZ_MATH_INLINE constexpr ColorHalf(const Internal::NamedColor& color)
            : ColorHalf(
                static_cast<float>(color.m_r) * (1.0f / 255.0f),
                static_cast<float>(color.m_g) * (1.0f / 255.0f),
                static_cast<float>(color.m_b) * (1.0f / 255.0f),
                static_cast<float>(color.m_a) * (1.0f / 255.0f))
        {
        }

        //! @name Narrowing from the other color types.
        //! All explicit, because binary16 has an 11-bit significand and none of the source formats survive it intact.
        //! @{
        AZ_MATH_INLINE explicit constexpr ColorHalf(const Color32 color)
            : ColorHalf(color.GetR(), color.GetG(), color.GetB(), color.GetA())
        {
        }

        AZ_MATH_INLINE explicit constexpr ColorHalf(const Color64 color)
            : ColorHalf(color.GetR(), color.GetG(), color.GetB(), color.GetA())
        {
        }

        AZ_MATH_INLINE explicit ColorHalf(const Color& color)
        {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float16x4_t halves = vcvt_f16_f32(color.GetAsVector4().GetSimdValue());
            m_packed = vget_lane_u64(vreinterpret_u64_f16(halves), 0);
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i halves = _mm_cvtps_ph(color.GetAsVector4().GetSimdValue(), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
#else
            m_r = Half(color.GetR());
            m_g = Half(color.GetG());
            m_b = Half(color.GetB());
            m_a = Half(color.GetA());
#endif
        }
        //! @}

        static const ColorHalf Zero;
        static const ColorHalf One;

        //! Wraps four raw binary16 bit patterns without converting them.
        AZ_MATH_INLINE static constexpr ColorHalf FromBits(u16 r, u16 g, u16 b, u16 a)
        {
            return {
                Half::FromBits(r),
                Half::FromBits(g),
                Half::FromBits(b),
                Half::FromBits(a),
            };
        }

        //! @name Channel access, as float.
        //! @{
        AZ_MATH_INLINE constexpr float GetR() const
        {
            return static_cast<float>(m_r);
        }
        AZ_MATH_INLINE constexpr float GetG() const
        {
            return static_cast<float>(m_g);
        }
        AZ_MATH_INLINE constexpr float GetB() const
        {
            return static_cast<float>(m_b);
        }
        AZ_MATH_INLINE constexpr float GetA() const
        {
            return static_cast<float>(m_a);
        }

        AZ_MATH_INLINE constexpr void SetR(float r)
        {
            m_r = Half(r);
        }
        AZ_MATH_INLINE constexpr void SetG(float g)
        {
            m_g = Half(g);
        }
        AZ_MATH_INLINE constexpr void SetB(float b)
        {
            m_b = Half(b);
        }
        AZ_MATH_INLINE constexpr void SetA(float a)
        {
            m_a = Half(a);
        }
        //! @}

        //! @name Channel access, as raw Half.
        //! @{
        AZ_MATH_INLINE constexpr Half GetRHalf() const
        {
            return m_r;
        }
        AZ_MATH_INLINE constexpr Half GetGHalf() const
        {
            return m_g;
        }
        AZ_MATH_INLINE constexpr Half GetBHalf() const
        {
            return m_b;
        }
        AZ_MATH_INLINE constexpr Half GetAHalf() const
        {
            return m_a;
        }
        //! @}


        //! Widening to a floating point color. Implicit and exact.
        AZ_MATH_INLINE operator Color() const
        {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float16x4_t halves = vreinterpret_f16_u16(vcreate_u16(m_packed));
            return Color(Vector4(vcvt_f32_f16(halves)));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i halves = _mm_cvtsi64_si128(static_cast<s64>(m_packed));
            return Color(Vector4(_mm_cvtph_ps(halves)));
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

        AZ_MATH_INLINE constexpr ColorHalf WithAlpha(float a) const
        {
            return {
                GetR(),
                GetG(),
                GetB(),
                a,
            };
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear channel values.
        AZ_MATH_INLINE constexpr float GetLuminance() const
        {
            return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB();
        }

        AZ_MATH_INLINE constexpr float GetMaxComponent() const
        {
            return GetMax(GetMax(GetR(), GetG()), GetB());
        }
        AZ_MATH_INLINE constexpr float GetMinComponent() const
        {
            return GetMin(GetMin(GetR(), GetG()), GetB());
        }

        AZ_MATH_INLINE constexpr bool IsOpaque() const
        {
            return m_a == Half::One;
        }

        AZ_MATH_INLINE constexpr bool IsTransparent() const
        {
            return m_a.IsZero();
        }

        //! True when every channel is finite, i.e. no channel is an infinity or a NaN.
        AZ_MATH_INLINE constexpr bool IsFinite() const
        {
            return m_r.IsFinite() && m_g.IsFinite() && m_b.IsFinite() && m_a.IsFinite();
        }

        //! Linear interpolation towards @p dest. @p t is NOT clamped, so this extrapolates,
        //! matching AZ::Color::Lerp. The unorm types clamp instead.
        AZ_MATH_INLINE constexpr ColorHalf Lerp(const ColorHalf dest, float t) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    AZ::Lerp(GetR(), dest.GetR(), t),
                    AZ::Lerp(GetG(), dest.GetG(), t),
                    AZ::Lerp(GetB(), dest.GetB(), t),
                    AZ::Lerp(GetA(), dest.GetA(), t),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t src4 = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float32x4_t dest4 = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(dest.m_packed)));
            const float32x4_t stepped = vaddq_f32(src4, vmulq_n_f32(vsubq_f32(dest4, src4), t));
            ColorHalf result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_f16(vcvt_f16_f32(stepped)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 src4 = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128 dest4 = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(dest.m_packed)));
            const __m128 stepped = _mm_add_ps(src4, _mm_mul_ps(_mm_sub_ps(dest4, src4), _mm_set1_ps(t)));
            const __m128i halves = _mm_cvtps_ph(stepped, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            ColorHalf result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            return result;
#else
            return {
                AZ::Lerp(GetR(), dest.GetR(), t),
                AZ::Lerp(GetG(), dest.GetG(), t),
                AZ::Lerp(GetB(), dest.GetB(), t),
                AZ::Lerp(GetA(), dest.GetA(), t),
            };
#endif
        }

        //! Multiplies r, g and b by alpha.
        AZ_MATH_INLINE constexpr ColorHalf Premultiply() const
        {
            if (az_builtin_is_constant_evaluated())
            {
                const float a = GetA();
                return {
                    GetR() * a,
                    GetG() * a,
                    GetB() * a,
                    a,
                };
            }
            // Alpha passes through untouched, so its bits merge back in place of the alpha * alpha lane.
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t channels = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float32x4_t premultiplied = vmulq_laneq_f32(channels, channels, 3);
            const u64 packed = vget_lane_u64(vreinterpret_u64_f16(vcvt_f16_f32(premultiplied)), 0);
            ColorHalf result;
            result.m_packed = (packed & 0x0000FFFFFFFFFFFFull) | (m_packed & 0xFFFF000000000000ull);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 channels = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128 alpha = _mm_shuffle_ps(channels, channels, _MM_SHUFFLE(3, 3, 3, 3));
            const __m128i halves = _mm_cvtps_ph(_mm_mul_ps(channels, alpha), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            const u64 packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            ColorHalf result;
            result.m_packed = (packed & 0x0000FFFFFFFFFFFFull) | (m_packed & 0xFFFF000000000000ull);
            return result;
#else
            const float a = GetA();
            return {
                GetR() * a,
                GetG() * a,
                GetB() * a,
                a,
            };
#endif
        }

        //! @name Arithmetic.
        //! Ordinary floating point arithmetic, with no saturation and no modulation, because HDR values legitimately exceed 1.0.
        //! Each operation rounds once, on storage back into binary16.
        //! @{
        AZ_MATH_INLINE constexpr ColorHalf operator-() const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    -GetR(),
                    -GetG(),
                    -GetB(),
                    -GetA(),
                };
            }
            // Negating a binary16 is exactly a sign bit flip, for every value class including NaN.
            ColorHalf result;
            result.m_packed = m_packed ^ 0x8000800080008000ull;
            return result;
        }

        AZ_MATH_INLINE constexpr ColorHalf operator+(const ColorHalf rhs) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    GetR() + rhs.GetR(),
                    GetG() + rhs.GetG(),
                    GetB() + rhs.GetB(),
                    GetA() + rhs.GetA(),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t lhs = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float32x4_t rhs4 = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(rhs.m_packed)));
            const float16x4_t halves = vcvt_f16_f32(vaddq_f32(lhs, rhs4));
            ColorHalf result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_f16(halves), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 lhs = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128 rhs4 = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(rhs.m_packed)));
            const __m128i halves = _mm_cvtps_ph(_mm_add_ps(lhs, rhs4), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            ColorHalf result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            return result;
#else
            return {
                GetR() + rhs.GetR(),
                GetG() + rhs.GetG(),
                GetB() + rhs.GetB(),
                GetA() + rhs.GetA(),
            };
#endif
        }

        AZ_MATH_INLINE constexpr ColorHalf operator-(const ColorHalf rhs) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    GetR() - rhs.GetR(),
                    GetG() - rhs.GetG(),
                    GetB() - rhs.GetB(),
                    GetA() - rhs.GetA(),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t lhs = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float32x4_t rhs4 = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(rhs.m_packed)));
            const float16x4_t halves = vcvt_f16_f32(vsubq_f32(lhs, rhs4));
            ColorHalf result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_f16(halves), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 lhs = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128 rhs4 = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(rhs.m_packed)));
            const __m128i halves = _mm_cvtps_ph(_mm_sub_ps(lhs, rhs4), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            ColorHalf result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            return result;
#else
            return {
                GetR() - rhs.GetR(),
                GetG() - rhs.GetG(),
                GetB() - rhs.GetB(),
                GetA() - rhs.GetA(),
            };
#endif
        }

        AZ_MATH_INLINE constexpr ColorHalf operator*(const ColorHalf rhs) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    GetR() * rhs.GetR(),
                    GetG() * rhs.GetG(),
                    GetB() * rhs.GetB(),
                    GetA() * rhs.GetA(),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t lhs = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float32x4_t rhs4 = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(rhs.m_packed)));
            const float16x4_t halves = vcvt_f16_f32(vmulq_f32(lhs, rhs4));
            ColorHalf result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_f16(halves), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 lhs = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128 rhs4 = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(rhs.m_packed)));
            const __m128i halves = _mm_cvtps_ph(_mm_mul_ps(lhs, rhs4), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            ColorHalf result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            return result;
#else
            return {
                GetR() * rhs.GetR(),
                GetG() * rhs.GetG(),
                GetB() * rhs.GetB(),
                GetA() * rhs.GetA(),
            };
#endif
        }

        AZ_MATH_INLINE constexpr ColorHalf operator*(float scale) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    GetR() * scale,
                    GetG() * scale,
                    GetB() * scale,
                    GetA() * scale,
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t lhs = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float16x4_t halves = vcvt_f16_f32(vmulq_n_f32(lhs, scale));
            ColorHalf result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_f16(halves), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 lhs = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128i halves = _mm_cvtps_ph(_mm_mul_ps(lhs, _mm_set1_ps(scale)), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            ColorHalf result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            return result;
#else
            return {
                GetR() * scale,
                GetG() * scale,
                GetB() * scale,
                GetA() * scale,
            };
#endif
        }

        AZ_MATH_INLINE constexpr ColorHalf operator/(float divisor) const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    GetR() / divisor,
                    GetG() / divisor,
                    GetB() / divisor,
                    GetA() / divisor,
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t lhs = vcvt_f32_f16(vreinterpret_f16_u16(vcreate_u16(m_packed)));
            const float16x4_t halves = vcvt_f16_f32(vdivq_f32(lhs, vdupq_n_f32(divisor)));
            ColorHalf result;
            result.m_packed = vget_lane_u64(vreinterpret_u64_f16(halves), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 lhs = _mm_cvtph_ps(_mm_cvtsi64_si128(static_cast<s64>(m_packed)));
            const __m128i halves = _mm_cvtps_ph(_mm_div_ps(lhs, _mm_set1_ps(divisor)), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            ColorHalf result;
            result.m_packed = static_cast<u64>(_mm_cvtsi128_si64(halves));
            return result;
#else
            return {
                GetR() / divisor,
                GetG() / divisor,
                GetB() / divisor,
                GetA() / divisor,
            };
#endif
        }

        AZ_MATH_INLINE constexpr ColorHalf& operator+=(const ColorHalf rhs)
        {
            *this = *this + rhs;
            return *this;
        }
        AZ_MATH_INLINE constexpr ColorHalf& operator-=(const ColorHalf rhs)
        {
            *this = *this - rhs;
            return *this;
        }
        AZ_MATH_INLINE constexpr ColorHalf& operator*=(const ColorHalf rhs)
        {
            *this = *this * rhs;
            return *this;
        }
        AZ_MATH_INLINE constexpr ColorHalf& operator*=(float scale)
        {
            *this = *this * scale;
            return *this;
        }
        AZ_MATH_INLINE constexpr ColorHalf& operator/=(float divisor)
        {
            *this = *this / divisor;
            return *this;
        }

        //! A friend so the scalar can appear on either side of the multiply.
        AZ_MATH_INLINE friend constexpr ColorHalf operator*(float scale, const ColorHalf color)
        {
            return color * scale;
        }
        //! @}

        //! IEEE equality on each channel, matching Half and AZ::Color: +0 equals -0, and a NaN channel makes the colors unequal.
        //! Compare GetBits() on the channels for representational equality.
        AZ_MATH_INLINE constexpr bool operator==(const ColorHalf rhs) const
        {
            return m_r == rhs.m_r
                && m_g == rhs.m_g
                && m_b == rhs.m_b
                && m_a == rhs.m_a;
        }

        AZ_MATH_INLINE constexpr bool operator!=(const ColorHalf rhs) const
        {
            return !(*this == rhs);
        }

        //! Checks each channel is within @p tolerance of @p rhs.
        AZ_MATH_INLINE constexpr bool IsClose(const ColorHalf rhs, float tolerance = 0.001f) const
        {
            return GetAbs(GetR() - rhs.GetR()) <= tolerance && GetAbs(GetG() - rhs.GetG()) <= tolerance
                && GetAbs(GetB() - rhs.GetB()) <= tolerance && GetAbs(GetA() - rhs.GetA()) <= tolerance;
        }

    private:
        //! Local absolute value so this stays usable in a constant expression.
        AZ_MATH_INLINE static constexpr float GetAbs(float v)
        {
            if (v < 0.0f)
            {
                return -v;
            }
            return v;
        }

        union
        {
            struct
            {
                Half m_r = Half::Zero;
                Half m_g = Half::Zero;
                Half m_b = Half::Zero;
                Half m_a = Half::One;
            };
            u64 m_packed;
        };
    };

    inline constexpr ColorHalf ColorHalf::Zero{
        Half::Zero,
        Half::Zero,
        Half::Zero,
        Half::Zero,
    };
    inline constexpr ColorHalf ColorHalf::One{
        Half::One,
        Half::One,
        Half::One,
        Half::One,
    };
} // namespace AZ

template<>
struct AZStd::hash<AZ::ColorHalf>
{
    constexpr size_t operator()(const AZ::ColorHalf value) const
    {
        // Channels hash through hash<Half> so that +0 and -0, which compare equal, hash equal.
        constexpr AZStd::hash<AZ::Half> hasher{};
        return static_cast<size_t>(
            (static_cast<AZ::u64>(hasher(value.GetRHalf())) << 48)
            | (static_cast<AZ::u64>(hasher(value.GetGHalf())) << 32)
            | (static_cast<AZ::u64>(hasher(value.GetBHalf())) << 16)
            | static_cast<AZ::u64>(hasher(value.GetAHalf())));
    }
};
