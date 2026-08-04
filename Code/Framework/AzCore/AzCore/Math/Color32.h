/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Color64.h>
#include <AzCore/Math/Colors.h>
#include <AzCore/Math/MathUtils.h>

namespace AZ
{
    //! A 32-bit color: 4 unsigned 8-bit channels, normalized over [0, 255], with no color space association.
    //! Maps directly to RHI::Format::R8G8B8A8_UNORM, and is aligned as one 32-bit word.
    //!
    //! Conversions follow the rule that widening is implicit and narrowing is explicit:
    //!
    //!     constexpr Color32 white = AZ::Colors::White; // implicit and exact, at compile time
    //!     Color64 wide = white; // implicit, u8 to u16 is exact (v * 257)
    //!     Color full = white; // implicit, u8 to float is exact
    //!     Color32 narrowed{someColor}; // explicit, float to u8 rounds and clamps
    //!
    //! @note Unlike AZ::Color::GetR8(), which truncates and can wrap for values above 1.0,
    //!       narrowing into Color32 rounds to nearest and clamps.
    class Color32 final
    {
    public:
        AZ_TYPE_INFO(Color32, "{9E4B71C3-5A26-4D8F-B013-7C2A6F85D934}");

        static AZCORE_API void Reflect(ReflectContext* context);

        //! Largest representable channel value. A channel of MaxChannel means 1.0.
        static constexpr u8 MaxChannel = AZStd::numeric_limits<u8>::max();

        constexpr Color32() = default;

        AZ_MATH_INLINE constexpr Color32(u8 r, u8 g, u8 b, u8 a)
            : m_r(r)
            , m_g(g)
            , m_b(b)
            , m_a(a)
        {
        }

        AZ_MATH_INLINE constexpr Color32(u8 r, u8 g, u8 b)
            : Color32(r, g, b, MaxChannel)
        {
        }

        //! Widening from a named color constant. Implicit and exact, since both are 8 bit.
        AZ_MATH_INLINE constexpr Color32(const Internal::NamedColor& color)
            : m_r(color.m_r)
            , m_g(color.m_g)
            , m_b(color.m_b)
            , m_a(color.m_a)
        {
        }

        //! Narrowing from a 64-bit color. Explicit. Exact for any value that originated as 8-bit,
        //! because u8 to u16 replicates bits (v * 257) and this takes the high byte back.
        AZ_MATH_INLINE explicit constexpr Color32(const Color64 color)
            : m_r(static_cast<u8>(color.GetR16() >> 8))
            , m_g(static_cast<u8>(color.GetG16() >> 8))
            , m_b(static_cast<u8>(color.GetB16() >> 8))
            , m_a(static_cast<u8>(color.GetA16() >> 8))
        {
        }

        //! Narrowing from a floating point color. Explicit, rounds to nearest and clamps to [0, 1].
        AZ_MATH_INLINE explicit Color32(const Color& color)
        {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t clamped = ClampUnitInterval4(color.GetAsVector4().GetSimdValue());
            m_packed = PackRounded(vmulq_n_f32(clamped, 255.0f));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 clamped = ClampUnitInterval4(color.GetAsVector4().GetSimdValue());
            m_packed = PackRounded(_mm_mul_ps(clamped, _mm_set1_ps(255.0f)));
#else
            m_r = FromFloat(color.GetR());
            m_g = FromFloat(color.GetG());
            m_b = FromFloat(color.GetB());
            m_a = FromFloat(color.GetA());
#endif
        }

        static const Color32 Zero;
        static const Color32 One;

        //! @name Unpacking from a u32, by explicit byte order.
        //! The name states the channel order from most significant byte to least significant, so FromArgb reads 0xAARRGGBB.
        //! Prefer these over hand rolled shifts at call sites.
        //! @{
        AZ_MATH_INLINE static constexpr Color32 FromRgba(u32 v)
        {
            return {
                static_cast<u8>(v >> 24),
                static_cast<u8>(v >> 16),
                static_cast<u8>(v >> 8),
                static_cast<u8>(v),
            };
        }

        AZ_MATH_INLINE static constexpr Color32 FromArgb(u32 v)
        {
            return {
                static_cast<u8>(v >> 16),
                static_cast<u8>(v >> 8),
                static_cast<u8>(v),
                static_cast<u8>(v >> 24),
            };
        }

        AZ_MATH_INLINE static constexpr Color32 FromAbgr(u32 v)
        {
            return {
                static_cast<u8>(v),
                static_cast<u8>(v >> 8),
                static_cast<u8>(v >> 16),
                static_cast<u8>(v >> 24),
            };
        }

        AZ_MATH_INLINE static constexpr Color32 FromBgra(u32 v)
        {
            return {
                static_cast<u8>(v >> 8),
                static_cast<u8>(v >> 16),
                static_cast<u8>(v >> 24),
                static_cast<u8>(v),
            };
        }
        //! @}

        //! Matches AZ::Color::FromU32, reading 0xAABBGGRR (COLREF), i.e. R8G8B8A8_UNORM in memory.
        AZ_MATH_INLINE static constexpr Color32 FromU32(u32 v)
        {
            return FromAbgr(v);
        }

        //! @name Channel access, as bytes.
        //! @{
        AZ_MATH_INLINE constexpr u8 GetR8() const
        {
            return m_r;
        }

        AZ_MATH_INLINE constexpr u8 GetG8() const
        {
            return m_g;
        }

        AZ_MATH_INLINE constexpr u8 GetB8() const
        {
            return m_b;
        }

        AZ_MATH_INLINE constexpr u8 GetA8() const
        {
            return m_a;
        }

        AZ_MATH_INLINE constexpr void SetR8(u8 r)
        {
            m_r = r;
        }

        AZ_MATH_INLINE constexpr void SetG8(u8 g)
        {
            m_g = g;
        }

        AZ_MATH_INLINE constexpr void SetB8(u8 b)
        {
            m_b = b;
        }

        AZ_MATH_INLINE constexpr void SetA8(u8 a)
        {
            m_a = a;
        }
        //! @}

        //! @name Channel access, normalized to the range [0, 1].
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

        //! @name Packing to a u32, by explicit byte order.
        //! ToRgba is 0xRRGGBBAA, ToAbgr is 0xAABBGGRR (which is R8G8B8A8_UNORM in memory), and ToArgb is 0xAARRGGBB.
        //! @{
        AZ_MATH_INLINE constexpr u32 ToRgba() const
        {
            return (static_cast<u32>(m_r) << 24)
                | (static_cast<u32>(m_g) << 16)
                | (static_cast<u32>(m_b) << 8)
                | static_cast<u32>(m_a);
        }

        AZ_MATH_INLINE constexpr u32 ToArgb() const
        {
            return (static_cast<u32>(m_a) << 24)
                | (static_cast<u32>(m_r) << 16)
                | (static_cast<u32>(m_g) << 8)
                | static_cast<u32>(m_b);
        }

        AZ_MATH_INLINE constexpr u32 ToAbgr() const
        {
            return (static_cast<u32>(m_a) << 24)
                | (static_cast<u32>(m_b) << 16)
                | (static_cast<u32>(m_g) << 8)
                | static_cast<u32>(m_r);
        }

        AZ_MATH_INLINE constexpr u32 ToBgra() const
        {
            return (static_cast<u32>(m_b) << 24)
                | (static_cast<u32>(m_g) << 16)
                | (static_cast<u32>(m_r) << 8)
                | static_cast<u32>(m_a);
        }
        //! @}

        //! Matches AZ::Color::ToU32, producing 0xAABBGGRR (COLREF), i.e. R8G8B8A8_UNORM in memory.
        AZ_MATH_INLINE constexpr u32 ToU32() const
        {
            return ToAbgr();
        }

        //! Widening to a 64-bit color. Implicit and exact, by bit replication (v * 257).
        AZ_MATH_INLINE constexpr operator Color64() const
        {
            return {
                static_cast<u16>(m_r * 257),
                static_cast<u16>(m_g * 257),
                static_cast<u16>(m_b * 257),
                static_cast<u16>(m_a * 257),
            };
        }

        //! Widening to a floating point color. Implicit and exact.
        AZ_MATH_INLINE operator Color() const
        {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            return Color(Vector4(vmulq_n_f32(WidenToFloat4(m_packed), 1.0f / 255.0f)));
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            return Color(Vector4(_mm_mul_ps(WidenToFloat4(m_packed), _mm_set1_ps(1.0f / 255.0f))));
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

        AZ_MATH_INLINE constexpr Color32 WithAlpha(u8 a) const
        {
            return {m_r, m_g, m_b, a};
        }

        AZ_MATH_INLINE constexpr Color32 Inverted() const
        {
            if (az_builtin_is_constant_evaluated())
            {
                return {
                    static_cast<u8>(MaxChannel - m_r),
                    static_cast<u8>(MaxChannel - m_g),
                    static_cast<u8>(MaxChannel - m_b),
                    m_a,
                };
            }
            // MaxChannel - x is x ^ MaxChannel, and alpha stays.
            Color32 result;
            result.m_packed = m_packed ^ 0x00FFFFFFu;
            return result;
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear channel values.
        AZ_MATH_INLINE constexpr float GetLuminance() const
        {
            return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB();
        }

        AZ_MATH_INLINE constexpr u8 GetMaxComponent() const
        {
            return GetMax(GetMax(m_r, m_g), m_b);
        }

        AZ_MATH_INLINE constexpr u8 GetMinComponent() const
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

        //! Linear interpolation towards @p dest. @p t is clamped to the range [0, 1]
        //! because the result must land in the representable [0, 1] range.
        //! ColorHalf::Lerp extrapolates instead.
        AZ_MATH_INLINE constexpr Color32 Lerp(const Color32 dest, float t) const
        {
            const float clamped = ClampUnitInterval(t);
            if (az_builtin_is_constant_evaluated())
            {
                const float r = static_cast<float>(m_r);
                const float g = static_cast<float>(m_g);
                const float b = static_cast<float>(m_b);
                const float a = static_cast<float>(m_a);
                return {
                    static_cast<u8>(r + (static_cast<float>(dest.m_r) - r) * clamped + 0.5f),
                    static_cast<u8>(g + (static_cast<float>(dest.m_g) - g) * clamped + 0.5f),
                    static_cast<u8>(b + (static_cast<float>(dest.m_b) - b) * clamped + 0.5f),
                    static_cast<u8>(a + (static_cast<float>(dest.m_a) - a) * clamped + 0.5f),
                };
            }
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const float32x4_t src4 = WidenToFloat4(m_packed);
            const float32x4_t dest4 = WidenToFloat4(dest.m_packed);
            Color32 result;
            result.m_packed = PackRounded(vaddq_f32(src4, vmulq_n_f32(vsubq_f32(dest4, src4), clamped)));
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 src4 = WidenToFloat4(m_packed);
            const __m128 dest4 = WidenToFloat4(dest.m_packed);
            Color32 result;
            result.m_packed = PackRounded(_mm_add_ps(src4, _mm_mul_ps(_mm_sub_ps(dest4, src4), _mm_set1_ps(clamped))));
            return result;
#else
            const float r = static_cast<float>(m_r);
            const float g = static_cast<float>(m_g);
            const float b = static_cast<float>(m_b);
            const float a = static_cast<float>(m_a);
            return {
                static_cast<u8>(r + (static_cast<float>(dest.m_r) - r) * clamped + 0.5f),
                static_cast<u8>(g + (static_cast<float>(dest.m_g) - g) * clamped + 0.5f),
                static_cast<u8>(b + (static_cast<float>(dest.m_b) - b) * clamped + 0.5f),
                static_cast<u8>(a + (static_cast<float>(dest.m_a) - a) * clamped + 0.5f),
            };
#endif
        }

        //! Source over alpha compositing of @p src on top of this color:
        //! channel = src * srcAlpha + dst * (1 - srcAlpha), alpha = Porter-Duff over.
        //! The channel math treats this color as opaque, so it is an approximation when it is not.
        AZ_MATH_INLINE constexpr Color32 Over(const Color32 src) const
        {
            // Modulate(sa, MaxChannel) == sa, so the alpha lane comes out right and its sum cannot saturate.
            const u8 sa = src.m_a;
            const u8 inv = static_cast<u8>(MaxChannel - sa);
            return (src * Color32(sa, sa, sa, MaxChannel)) + (*this * Color32(inv, inv, inv, inv));
        }

        //! Multiplies r, g and b by alpha.
        AZ_MATH_INLINE constexpr Color32 Premultiply() const
        {
            return *this * Color32(m_a, m_a, m_a, MaxChannel);
        }

        //! @name Arithmetic.
        //! Integer color arithmetic is not float color arithmetic. Addition and subtraction
        //! **saturate** rather than wrap, and color times color modulates, i.e. (a * b) / MaxChannel.
        //! @{
        AZ_MATH_INLINE constexpr Color32 operator+(const Color32 rhs) const
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
            const uint8x8_t lhs = vreinterpret_u8_u32(vdup_n_u32(m_packed));
            const uint8x8_t rhs8 = vreinterpret_u8_u32(vdup_n_u32(rhs.m_packed));
            Color32 result;
            result.m_packed = vget_lane_u32(vreinterpret_u32_u8(vqadd_u8(lhs, rhs8)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i lhs = _mm_cvtsi32_si128(static_cast<s32>(m_packed));
            const __m128i rhs8 = _mm_cvtsi32_si128(static_cast<s32>(rhs.m_packed));
            Color32 result;
            result.m_packed = static_cast<u32>(_mm_cvtsi128_si32(_mm_adds_epu8(lhs, rhs8)));
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

        AZ_MATH_INLINE constexpr Color32 operator-(const Color32 rhs) const
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
            const uint8x8_t lhs = vreinterpret_u8_u32(vdup_n_u32(m_packed));
            const uint8x8_t rhs8 = vreinterpret_u8_u32(vdup_n_u32(rhs.m_packed));
            Color32 result;
            result.m_packed = vget_lane_u32(vreinterpret_u32_u8(vqsub_u8(lhs, rhs8)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i lhs = _mm_cvtsi32_si128(static_cast<s32>(m_packed));
            const __m128i rhs8 = _mm_cvtsi32_si128(static_cast<s32>(rhs.m_packed));
            Color32 result;
            result.m_packed = static_cast<u32>(_mm_cvtsi128_si32(_mm_subs_epu8(lhs, rhs8)));
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

        AZ_MATH_INLINE constexpr Color32 operator*(const Color32 rhs) const
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
            // (u + (u >> 8)) >> 8 with u = t + 128 equals Modulate for every product. The tests sweep all of them.
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
            const uint8x8_t lhs = vreinterpret_u8_u32(vdup_n_u32(m_packed));
            const uint8x8_t rhs8 = vreinterpret_u8_u32(vdup_n_u32(rhs.m_packed));
            uint16x8_t product = vmull_u8(lhs, rhs8);
            product = vaddq_u16(product, vdupq_n_u16(128));
            product = vsraq_n_u16(product, product, 8);
            Color32 result;
            result.m_packed = vget_lane_u32(vreinterpret_u32_u8(vshrn_n_u16(product, 8)), 0);
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128i lhs = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(static_cast<s32>(m_packed)));
            const __m128i rhs16 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(static_cast<s32>(rhs.m_packed)));
            __m128i product = _mm_mullo_epi16(lhs, rhs16);
            product = _mm_add_epi16(product, _mm_set1_epi16(128));
            product = _mm_add_epi16(product, _mm_srli_epi16(product, 8));
            product = _mm_srli_epi16(product, 8);
            Color32 result;
            result.m_packed = static_cast<u32>(_mm_cvtsi128_si32(_mm_packus_epi16(product, product)));
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

        AZ_MATH_INLINE constexpr Color32 operator*(float scale) const
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
            const float32x4_t normalized = vmulq_n_f32(WidenToFloat4(m_packed), 1.0f / 255.0f);
            const float32x4_t clamped = ClampUnitInterval4(vmulq_n_f32(normalized, scale));
            Color32 result;
            result.m_packed = PackRounded(vmulq_n_f32(clamped, 255.0f));
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 normalized = _mm_mul_ps(WidenToFloat4(m_packed), _mm_set1_ps(1.0f / 255.0f));
            const __m128 clamped = ClampUnitInterval4(_mm_mul_ps(normalized, _mm_set1_ps(scale)));
            Color32 result;
            result.m_packed = PackRounded(_mm_mul_ps(clamped, _mm_set1_ps(255.0f)));
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

        AZ_MATH_INLINE constexpr Color32 operator/(float divisor) const
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
            const float32x4_t normalized = vmulq_n_f32(WidenToFloat4(m_packed), 1.0f / 255.0f);
            const float32x4_t clamped = ClampUnitInterval4(vdivq_f32(normalized, vdupq_n_f32(divisor)));
            Color32 result;
            result.m_packed = PackRounded(vmulq_n_f32(clamped, 255.0f));
            return result;
#elif AZ_TRAIT_USE_PLATFORM_SIMD_SSE
            const __m128 normalized = _mm_mul_ps(WidenToFloat4(m_packed), _mm_set1_ps(1.0f / 255.0f));
            const __m128 clamped = ClampUnitInterval4(_mm_div_ps(normalized, _mm_set1_ps(divisor)));
            Color32 result;
            result.m_packed = PackRounded(_mm_mul_ps(clamped, _mm_set1_ps(255.0f)));
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

        AZ_MATH_INLINE constexpr Color32& operator+=(const Color32 rhs)
        {
            *this = *this + rhs;
            return *this;
        }

        AZ_MATH_INLINE constexpr Color32& operator-=(const Color32 rhs)
        {
            *this = *this - rhs;
            return *this;
        }

        AZ_MATH_INLINE constexpr Color32& operator*=(const Color32 rhs)
        {
            *this = *this * rhs;
            return *this;
        }

        AZ_MATH_INLINE constexpr Color32& operator*=(float scale)
        {
            *this = *this * scale;
            return *this;
        }

        AZ_MATH_INLINE constexpr Color32& operator/=(float divisor)
        {
            *this = *this / divisor;
            return *this;
        }

        //! A friend so the scalar can appear on either side of the multiply.
        AZ_MATH_INLINE friend constexpr Color32 operator*(float scale, const Color32 color)
        {
            return color * scale;
        }
        //! @}

        AZ_MATH_INLINE constexpr bool operator==(const Color32 rhs) const
        {
            return m_r == rhs.m_r
                && m_g == rhs.m_g
                && m_b == rhs.m_b
                && m_a == rhs.m_a;
        }

        AZ_MATH_INLINE constexpr bool operator!=(const Color32 rhs) const
        {
            return !(*this == rhs);
        }

        //! @name Ordering, so a color can be used as an associative container key.
        //! The order is stable and total but arbitrary. It is not perceptual.
        //! @{
        AZ_MATH_INLINE constexpr bool operator<(const Color32 rhs) const
        {
            return ToRgba() < rhs.ToRgba();
        }

        AZ_MATH_INLINE constexpr bool operator>(const Color32 rhs) const
        {
            return rhs < *this;
        }

        AZ_MATH_INLINE constexpr bool operator<=(const Color32 rhs) const
        {
            return !(rhs < *this);
        }

        AZ_MATH_INLINE constexpr bool operator>=(const Color32 rhs) const
        {
            return !(*this < rhs);
        }
        //! @}

    private:
        AZ_MATH_INLINE static constexpr float ToFloat(u8 v)
        {
            return static_cast<float>(v) * (1.0f / 255.0f);
        }

        //! Clamps to the range [0, 1] with defined behavior for NaN.
        //!
        //! AZ::GetClamp cannot be used here.
        //! It tests `value < min` then `value > max`, both of which are false for NaN,
        //! so it returns NaN unchanged, and static_cast<u8> of NaN is undefined behavior.
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

        AZ_MATH_INLINE static constexpr u8 FromFloat(float v)
        {
            return static_cast<u8>(ClampUnitInterval(v) * 255.0f + 0.5f);
        }

        //! A wrapping add plus overflow test is the shape compilers recognize as a saturating add and vectorize.
        AZ_MATH_INLINE static constexpr u8 SaturatingAdd(u8 a, u8 b)
        {
            const u8 sum = static_cast<u8>(a + b);
            if (sum < a)
            {
                return MaxChannel;
            }
            return sum;
        }

        AZ_MATH_INLINE static constexpr u8 SaturatingSubtract(u8 a, u8 b)
        {
            if (a > b)
            {
                return static_cast<u8>(a - b);
            }
            return 0;
        }

        //! Exact normalized multiply, round(a * b / 255).
        AZ_MATH_INLINE static constexpr u8 Modulate(u8 a, u8 b)
        {
            return static_cast<u8>((static_cast<u32>(a) * static_cast<u32>(b) + 127u) / 255u);
        }

#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
        //! The four packed channels as raw values in float lanes 0-3.
        AZ_MATH_INLINE static float32x4_t WidenToFloat4(u32 packed)
        {
            const uint8x8_t bytes = vreinterpret_u8_u32(vdup_n_u32(packed));
            return vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(bytes))));
        }

        //! Rounds four raw channel values in [0, 255] back into packed bytes, matching FromFloat's rounding.
        AZ_MATH_INLINE static u32 PackRounded(float32x4_t channels)
        {
            const uint32x4_t truncated = vcvtq_u32_f32(vaddq_f32(channels, vdupq_n_f32(0.5f)));
            const uint16x4_t narrowed = vmovn_u32(truncated);
            return vget_lane_u32(vreinterpret_u32_u8(vmovn_u16(vcombine_u16(narrowed, narrowed))), 0);
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
        AZ_MATH_INLINE static __m128 WidenToFloat4(u32 packed)
        {
            return _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(static_cast<s32>(packed))));
        }

        //! Rounds four raw channel values in [0, 255] back into packed bytes, matching FromFloat's rounding.
        AZ_MATH_INLINE static u32 PackRounded(__m128 channels)
        {
            const __m128i truncated = _mm_cvttps_epi32(_mm_add_ps(channels, _mm_set1_ps(0.5f)));
            const __m128i packed = _mm_packus_epi16(_mm_packus_epi32(truncated, truncated), truncated);
            return static_cast<u32>(_mm_cvtsi128_si32(packed));
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
                u8 m_r = 0;
                u8 m_g = 0;
                u8 m_b = 0;
                u8 m_a = MaxChannel;
            };
            u32 m_packed;
        };
    };

    inline constexpr Color32 Color32::Zero{0, 0, 0, 0};
    inline constexpr Color32 Color32::One{
        Color32::MaxChannel,
        Color32::MaxChannel,
        Color32::MaxChannel,
        Color32::MaxChannel,
    };
} // namespace AZ

template<>
struct AZStd::hash<AZ::Color32>
{
    constexpr size_t operator()(const AZ::Color32 value) const
    {
        return static_cast<size_t>(value.ToRgba());
    }
};
