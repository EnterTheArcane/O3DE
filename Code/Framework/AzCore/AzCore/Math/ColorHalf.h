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
#include <AzCore/Math/ColorSwatch.h>
#include <AzCore/Math/Half.h>

namespace AZ
{
    //! A 64 bit HDR color: 4 IEEE binary16 channels, with no color space association.
    //! Maps directly to RHI::Format::R16G16B16A16_FLOAT.
    //!
    //! Unlike Color32 and Color64 this is *not* normalized -- channels may exceed 1.0, go negative,
    //! or be infinite, which is what makes it the right storage for HDR values. Consequently its
    //! arithmetic is ordinary floating point arithmetic, not the saturating/modulating integer
    //! arithmetic those two use.
    //!
    //! Because it is HDR it sits outside the Color32 -> Color64 -> Color widening chain:
    //!
    //!     Color     full = someColorHalf;      // implicit: binary16 -> float is exact
    //!     ColorHalf hdr{ someColor };           // explicit: float -> binary16 loses precision
    //!     ColorHalf hdr{ someColor32 };         // explicit: 1/255 is not representable in binary16
    //!     ColorHalf hdr = AZ::Colors::White;    // implicit, for palette ergonomics (see note)
    //!
    //! @note The ColorSwatch conversion is implicit for palette ergonomics even though it is not
    //!       bit-exact: 255/255 is exactly 1.0, but a channel such as 1/255 has no exact binary16
    //!       representation. The error is at most one binary16 ulp.
    //!
    //! @note Active union member rule. The named components (m_r/m_g/m_b/m_a) are ALWAYS the active
    //!       union member: every constexpr constructor initializes them and every constexpr accessor
    //!       reads them. m_channels[] and m_packed are runtime-only views -- using either in a constant
    //!       expression is a compile error rather than a silent wrong answer.
    struct ColorHalf final
    {
        AZ_TYPE_INFO(ColorHalf, "{A3E7D419-6C85-4F20-B7D3-1E9A45C806F7}");

        constexpr ColorHalf() = default;

        constexpr ColorHalf(Half r, Half g, Half b, Half a)
            : m_r(r), m_g(g), m_b(b), m_a(a)
        {
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf(float r, float g, float b, float a = 1.0f)
            : m_r(Half(r)), m_g(Half(g)), m_b(Half(b)), m_a(Half(a))
        {
        }

        //! Widening from a compile-time color literal. Implicit; see the note above about exactness.
        AZ_MATH_HALF_CONSTEXPR ColorHalf(const ColorSwatch& swatch)
            : ColorHalf(swatch.GetR(), swatch.GetG(), swatch.GetB(), swatch.GetA())
        {
        }

        //! Explicit: binary16 has an 11 bit significand, so this loses precision.
        AZ_MATH_HALF_CONSTEXPR explicit ColorHalf(const Color32& color)
            : ColorHalf(color.GetR(), color.GetG(), color.GetB(), color.GetA())
        {
        }

        //! Explicit: binary16 has an 11 bit significand, so this loses precision.
        AZ_MATH_HALF_CONSTEXPR explicit ColorHalf(const Color64& color)
            : ColorHalf(color.GetR(), color.GetG(), color.GetB(), color.GetA())
        {
        }

        //! Explicit: binary16 has an 11 bit significand, so this loses precision.
        explicit AZ_MATH_HALF_CONSTEXPR ColorHalf(const Color& color)
            : ColorHalf(color.GetR(), color.GetG(), color.GetB(), color.GetA())
        {
        }

        static constexpr ColorHalf CreateZero()
        {
            return ColorHalf(Half::CreateZero(), Half::CreateZero(), Half::CreateZero(), Half::CreateZero());
        }

        static constexpr ColorHalf CreateOne()
        {
            return ColorHalf(Half::CreateOne(), Half::CreateOne(), Half::CreateOne(), Half::CreateOne());
        }

        //! Wraps four raw binary16 bit patterns without converting them.
        static constexpr ColorHalf FromBits(u16 r, u16 g, u16 b, u16 a)
        {
            return ColorHalf(Half::FromBits(r), Half::FromBits(g), Half::FromBits(b), Half::FromBits(a));
        }

        //! @name Channel access, as float.
        //! @{
        AZ_MATH_HALF_CONSTEXPR float GetR() const { return static_cast<float>(m_r); }
        AZ_MATH_HALF_CONSTEXPR float GetG() const { return static_cast<float>(m_g); }
        AZ_MATH_HALF_CONSTEXPR float GetB() const { return static_cast<float>(m_b); }
        AZ_MATH_HALF_CONSTEXPR float GetA() const { return static_cast<float>(m_a); }

        AZ_MATH_HALF_CONSTEXPR void SetR(float r) { m_r = Half(r); }
        AZ_MATH_HALF_CONSTEXPR void SetG(float g) { m_g = Half(g); }
        AZ_MATH_HALF_CONSTEXPR void SetB(float b) { m_b = Half(b); }
        AZ_MATH_HALF_CONSTEXPR void SetA(float a) { m_a = Half(a); }
        //! @}

        //! @name Channel access, as raw Half.
        //! @{
        constexpr Half GetRHalf() const { return m_r; }
        constexpr Half GetGHalf() const { return m_g; }
        constexpr Half GetBHalf() const { return m_b; }
        constexpr Half GetAHalf() const { return m_a; }
        //! @}

        AZ_MATH_HALF_CONSTEXPR float GetElement(u32 index) const
        {
            switch (index)
            {
            case 0:  return GetR();
            case 1:  return GetG();
            case 2:  return GetB();
            default: return GetA();
            }
        }

        AZ_MATH_HALF_CONSTEXPR void SetElement(u32 index, float value)
        {
            switch (index)
            {
            case 0:  SetR(value); break;
            case 1:  SetG(value); break;
            case 2:  SetB(value); break;
            default: SetA(value); break;
            }
        }

        AZ_MATH_HALF_CONSTEXPR float operator[](u32 index) const { return GetElement(index); }

        //! @name Packing the raw binary16 bit patterns into a u64, by explicit channel order.
        //! @{
        constexpr u64 AsU64Rgba() const { return Pack(m_r, m_g, m_b, m_a); }
        constexpr u64 AsU64Argb() const { return Pack(m_a, m_r, m_g, m_b); }
        constexpr u64 AsU64Abgr() const { return Pack(m_a, m_b, m_g, m_r); }
        constexpr u64 AsU64Bgra() const { return Pack(m_b, m_g, m_r, m_a); }
        //! @}

        //! Widening to a floating point color. Implicit and exact.
        AZ_MATH_HALF_CONSTEXPR operator Color() const { return Color(GetR(), GetG(), GetB(), GetA()); }

        //! @note Not yet constexpr: AZ::Vector3/Vector4 are not literal types.
        AZ_MATH_HALF_CONSTEXPR Vector3 AsVector3() const { return Vector3(GetR(), GetG(), GetB()); }
        AZ_MATH_HALF_CONSTEXPR Vector4 AsVector4() const { return Vector4(GetR(), GetG(), GetB(), GetA()); }

        AZ_MATH_HALF_CONSTEXPR ColorHalf WithAlpha(float a) const { return ColorHalf(GetR(), GetG(), GetB(), a); }

        //! Reflects r, g and b about 1.0. Only meaningful for values already inside [0, 1].
        AZ_MATH_HALF_CONSTEXPR ColorHalf Inverted() const
        {
            return ColorHalf(1.0f - GetR(), 1.0f - GetG(), 1.0f - GetB(), GetA());
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear channel values.
        AZ_MATH_HALF_CONSTEXPR float GetLuminance() const
        {
            return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB();
        }

        AZ_MATH_HALF_CONSTEXPR float GetMaxComponent() const { return GetMax(GetMax(GetR(), GetG()), GetB()); }
        AZ_MATH_HALF_CONSTEXPR float GetMinComponent() const { return GetMin(GetMin(GetR(), GetG()), GetB()); }

        constexpr bool IsOpaque() const { return m_a == Half::CreateOne(); }
        constexpr bool IsTransparent() const { return m_a.IsZero(); }

        //! True when every channel is finite, i.e. no channel is an infinity or a NaN.
        constexpr bool IsFinite() const
        {
            return m_r.IsFinite() && m_g.IsFinite() && m_b.IsFinite() && m_a.IsFinite();
        }

        //! Linear interpolation towards @p dest. @p t is NOT clamped, so this extrapolates.
        AZ_MATH_HALF_CONSTEXPR ColorHalf Lerp(const ColorHalf& dest, float t) const
        {
            return ColorHalf(AZ::Lerp(GetR(), dest.GetR(), t), AZ::Lerp(GetG(), dest.GetG(), t),
                AZ::Lerp(GetB(), dest.GetB(), t), AZ::Lerp(GetA(), dest.GetA(), t));
        }

        //! Multiplies r, g and b by alpha.
        AZ_MATH_HALF_CONSTEXPR ColorHalf Premultiply() const
        {
            const float a = GetA();
            return ColorHalf(GetR() * a, GetG() * a, GetB() * a, a);
        }

        //! @name Arithmetic.
        //! Ordinary floating point arithmetic -- no saturation and no modulation, because HDR values
        //! legitimately exceed 1.0. Each operation rounds once, on storage back into binary16.
        //! @{
        AZ_MATH_HALF_CONSTEXPR ColorHalf operator-() const
        {
            return ColorHalf(-GetR(), -GetG(), -GetB(), -GetA());
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf operator+(const ColorHalf& rhs) const
        {
            return ColorHalf(GetR() + rhs.GetR(), GetG() + rhs.GetG(), GetB() + rhs.GetB(), GetA() + rhs.GetA());
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf operator-(const ColorHalf& rhs) const
        {
            return ColorHalf(GetR() - rhs.GetR(), GetG() - rhs.GetG(), GetB() - rhs.GetB(), GetA() - rhs.GetA());
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf operator*(const ColorHalf& rhs) const
        {
            return ColorHalf(GetR() * rhs.GetR(), GetG() * rhs.GetG(), GetB() * rhs.GetB(), GetA() * rhs.GetA());
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf operator/(const ColorHalf& rhs) const
        {
            return ColorHalf(GetR() / rhs.GetR(), GetG() / rhs.GetG(), GetB() / rhs.GetB(), GetA() / rhs.GetA());
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf operator*(float scale) const
        {
            return ColorHalf(GetR() * scale, GetG() * scale, GetB() * scale, GetA() * scale);
        }

        AZ_MATH_HALF_CONSTEXPR ColorHalf operator/(float divisor) const { return (*this) * (1.0f / divisor); }

        AZ_MATH_HALF_CONSTEXPR ColorHalf& operator+=(const ColorHalf& rhs) { return *this = *this + rhs; }
        AZ_MATH_HALF_CONSTEXPR ColorHalf& operator-=(const ColorHalf& rhs) { return *this = *this - rhs; }
        AZ_MATH_HALF_CONSTEXPR ColorHalf& operator*=(const ColorHalf& rhs) { return *this = *this * rhs; }
        AZ_MATH_HALF_CONSTEXPR ColorHalf& operator*=(float scale) { return *this = *this * scale; }
        AZ_MATH_HALF_CONSTEXPR ColorHalf& operator/=(float divisor) { return *this = *this / divisor; }
        //! @}

        //! Bit pattern equality. As with Half, this is not IEEE comparison: NaN equals itself and
        //! +0 does not equal -0. Compare channel-wise as float when IEEE semantics are wanted.
        constexpr bool operator==(const ColorHalf& rhs) const
        {
            return m_r == rhs.m_r && m_g == rhs.m_g && m_b == rhs.m_b && m_a == rhs.m_a;
        }

        constexpr bool operator!=(const ColorHalf& rhs) const { return !(*this == rhs); }

        //! Checks each channel is within @p tolerance of @p rhs.
        AZ_MATH_HALF_CONSTEXPR bool IsClose(const ColorHalf& rhs, float tolerance = 0.001f) const
        {
            return GetAbs(GetR() - rhs.GetR()) <= tolerance && GetAbs(GetG() - rhs.GetG()) <= tolerance &&
                GetAbs(GetB() - rhs.GetB()) <= tolerance && GetAbs(GetA() - rhs.GetA()) <= tolerance;
        }

        union
        {
            //! Always the active member. See the active union member rule above.
            struct
            {
                Half m_r{ Half::CreateZero() };
                Half m_g{ Half::CreateZero() };
                Half m_b{ Half::CreateZero() };
                Half m_a{ Half::CreateOne() };
            };
            //! Runtime-only views. Reading either in a constant expression is a compile error.
            Half m_channels[4];
            u64 m_packed;
        };

    private:
        static constexpr u64 Pack(Half w3, Half w2, Half w1, Half w0)
        {
            return (static_cast<u64>(w3.GetBits()) << 48) | (static_cast<u64>(w2.GetBits()) << 32) |
                (static_cast<u64>(w1.GetBits()) << 16) | static_cast<u64>(w0.GetBits());
        }

        //! Local absolute value so this stays usable in a constant expression.
        static constexpr float GetAbs(float v) { return v < 0.0f ? -v : v; }
    };

    static_assert(sizeof(ColorHalf) == 8, "ColorHalf must be exactly 8 bytes");
    static_assert(alignof(ColorHalf) == 8, "ColorHalf must be 8 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<ColorHalf>, "ColorHalf must be trivially copyable");
} // namespace AZ

namespace AZStd
{
    template<>
    struct hash<AZ::ColorHalf>
    {
        using result_type = AZStd::size_t;
        constexpr result_type operator()(const AZ::ColorHalf& value) const
        {
            // Computed from the components rather than m_packed so this is usable at compile time.
            return static_cast<result_type>(value.AsU64Rgba());
        }
    };
} // namespace AZStd
