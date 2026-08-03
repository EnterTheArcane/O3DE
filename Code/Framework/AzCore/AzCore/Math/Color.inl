/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

namespace AZ
{
    AZ_MATH_INLINE constexpr Color::Color(const ColorSwatch& swatch)
        : m_color(swatch.GetR(), swatch.GetG(), swatch.GetB(), swatch.GetA())
    {
        ;
    }

    AZ_MATH_INLINE Color::Color(const Vector2& source)
    {
        m_color = Vector4(source);
    }

    AZ_MATH_INLINE constexpr Color::Color(const Vector3& source)
        : m_color(source)
    {
        ;
    }

    AZ_MATH_INLINE constexpr Color::Color(float rgba)
        : m_color(rgba)
    {
        ;
    }

    template<typename T> requires AZStd::is_floating_point_v<T>
    AZ_MATH_INLINE constexpr Color::Color(T r, T g, T b, T a)
        : m_color(r, g, b, a)
    {
        ;
    }

    template<typename T> requires AZStd::is_integral_v<T>
    AZ_MATH_INLINE constexpr Color::Color(T r, T g, T b, T a)
        : m_color(
              static_cast<float>(static_cast<u8>(r)) * (1.0f / 255.0f),
              static_cast<float>(static_cast<u8>(g)) * (1.0f / 255.0f),
              static_cast<float>(static_cast<u8>(b)) * (1.0f / 255.0f),
              static_cast<float>(static_cast<u8>(a)) * (1.0f / 255.0f))
    {
        // Built in the mem-init list rather than through SetR8..SetA8 so this can be a constant
        // expression. The asserts still fire at runtime; out of range values are caught at compile
        // time by the narrowing in static_cast<u8> being visible to the caller.
        AZ_MATH_ASSERT(r >= 0 && r <= 255, "R component must be in the range [0..255]");
        AZ_MATH_ASSERT(g >= 0 && g <= 255, "G component must be in the range [0..255]");
        AZ_MATH_ASSERT(b >= 0 && b <= 255, "B component must be in the range [0..255]");
        AZ_MATH_ASSERT(a >= 0 && a <= 255, "A component must be in the range [0..255]");
    }


    AZ_MATH_INLINE constexpr Color Color::CreateZero()
    {
        return Color(Vector4::CreateZero());
    }


    AZ_MATH_INLINE constexpr Color Color::CreateOne()
    {
        return Color(1.0f);
    }


    AZ_MATH_INLINE constexpr Color Color::CreateFromRgba(u8 r, u8 g, u8 b, u8 a)
    {
        return Color(r,g,b,a);
    }


    AZ_MATH_INLINE Color Color::CreateFromFloat4(const float* values)
    {
        Color result;
        result.Set(values);
        return result;
    }


    AZ_MATH_INLINE constexpr Color Color::CreateFromVector3(const Vector3& v)
    {
        return Color(Vector4(v));
    }


    AZ_MATH_INLINE constexpr Color Color::CreateFromVector3AndFloat(const Vector3& v, float w)
    {
        return Color(Vector4(v, w));
    }


    AZ_MATH_INLINE constexpr u32 Color::CreateU32(u8 r, u8 g, u8 b, u8 a)
    {
        return (a << 24) | (b << 16) | (g << 8) | r;
    }


    AZ_MATH_INLINE void Color::StoreToFloat4(float* values) const
    {
        m_color.StoreToFloat4(values);
    }


    AZ_MATH_INLINE constexpr u8 Color::GetR8() const
    {
        return static_cast<u8>(m_color.GetX() * 255.0f);
    }


    AZ_MATH_INLINE constexpr u8 Color::GetG8() const
    {
        return static_cast<u8>(m_color.GetY() * 255.0f);
    }


    AZ_MATH_INLINE constexpr u8 Color::GetB8() const
    {
        return static_cast<u8>(m_color.GetZ() * 255.0f);
    }


    AZ_MATH_INLINE constexpr u8 Color::GetA8() const
    {
        return static_cast<u8>(m_color.GetW() * 255.0f);
    }


    AZ_MATH_INLINE constexpr void Color::SetR8(u8 r)
    {
        m_color.SetX(static_cast<float>(r) * (1.0f / 255.0f));
    }


    AZ_MATH_INLINE constexpr void Color::SetG8(u8 g)
    {
        m_color.SetY(static_cast<float>(g) * (1.0f / 255.0f));
    }


    AZ_MATH_INLINE constexpr void Color::SetB8(u8 b)
    {
        m_color.SetZ(static_cast<float>(b) * (1.0f / 255.0f));
    }


    AZ_MATH_INLINE constexpr void Color::SetA8(u8 a)
    {
        m_color.SetW(static_cast<float>(a) * (1.0f / 255.0f));
    }


    AZ_MATH_INLINE constexpr float Color::GetR() const
    {
        return m_color.GetX();
    }


    AZ_MATH_INLINE constexpr float Color::GetG() const
    {
        return m_color.GetY();
    }


    AZ_MATH_INLINE constexpr float Color::GetB() const
    {
        return m_color.GetZ();
    }


    AZ_MATH_INLINE constexpr float Color::GetA() const
    {
        return m_color.GetW();
    }


    AZ_MATH_INLINE constexpr float Color::GetElement(int32_t index) const
    {
        return m_color.GetElement(index);
    }


    AZ_MATH_INLINE constexpr void Color::SetR(float r)
    {
        m_color.SetX(r);
    }


    AZ_MATH_INLINE constexpr void Color::SetG(float g)
    {
        m_color.SetY(g);
    }


    AZ_MATH_INLINE constexpr void Color::SetB(float b)
    {
        m_color.SetZ(b);
    }


    AZ_MATH_INLINE constexpr void Color::SetA(float a)
    {
        m_color.SetW(a);
    }


    AZ_MATH_INLINE constexpr void Color::Set(float x)
    {
        m_color.Set(x);
    }


    AZ_MATH_INLINE constexpr void Color::Set(float r, float g, float b, float a)
    {
        m_color.Set(r, g, b, a);
    }


    AZ_MATH_INLINE void Color::Set(const float values[4])
    {
        m_color.Set(values);
    }


    AZ_MATH_INLINE constexpr void Color::Set(const Vector3& v)
    {
        m_color.Set(v);
    }


    AZ_MATH_INLINE constexpr void Color::Set(const Vector3& color, float a)
    {
        m_color.Set(color, a);
    }


    AZ_MATH_INLINE constexpr void Color::SetElement(int32_t index, float v)
    {
        m_color.SetElement(index, v);
    }


    AZ_MATH_INLINE constexpr Vector3 Color::GetAsVector3() const
    {
        return m_color.GetAsVector3();
    }


    AZ_MATH_INLINE constexpr Vector4 Color::GetAsVector4() const
    {
        return m_color;
    }


    AZ_MATH_INLINE void Color::SetFromHSVRadians(float hueRadians, float saturation, float value)
    {
        float alpha = GetA();

        // Saturation and value outside of [0-1] are invalid, so clamp them to valid values.
        saturation = GetClamp(saturation, 0.0f, 1.0f);
        value = GetClamp(value, 0.0f, 1.0f);

        hueRadians = fmodf(hueRadians, AZ::Constants::TwoPi);
        if (hueRadians < 0)
        {
            hueRadians += AZ::Constants::TwoPi;
        }

        // https://en.wikipedia.org/wiki/HSL_and_HSV#Converting_to_RGB
        float hue = fmodf(hueRadians / AZ::DegToRad(60.0f), 6.0f);
        const int32_t hueSexant = static_cast<int32_t>(hue);
        const float hueSexantRemainder = hue - hueSexant;

        const float offColor = value * (1.0f - saturation);
        const float fallingColor = value * (1.0f - (saturation * hueSexantRemainder));
        const float risingColor = value * (1.0f - (saturation * (1.0f - hueSexantRemainder)));

        switch (hueSexant)
        {
        case 0:
            Set(value, risingColor, offColor, alpha);
            break;
        case 1:
            Set(fallingColor, value, offColor, alpha);
            break;
        case 2:
            Set(offColor, value, risingColor, alpha);
            break;
        case 3:
            Set(offColor, fallingColor, value, alpha);
            break;
        case 4:
            Set(risingColor, offColor, value, alpha);
            break;
        case 5:
            Set(value, offColor, fallingColor, alpha);
            break;
        default:
            AZ_MATH_ASSERT(true,
                "SetFromHSV has generated invalid data from these parameters : H %.5f, S %.5f, V %.5f.",
                hueRadians,
                saturation,
                value);
        }
    }


    AZ_MATH_INLINE bool Color::IsClose(const Color& v, float tolerance) const
    {
        return m_color.IsClose(v.GetAsVector4(), tolerance);
    }


    AZ_MATH_INLINE bool Color::IsZero(float tolerance) const
    {
        return IsClose(CreateZero(), tolerance);
    }

    AZ_MATH_INLINE bool Color::IsFinite() const
    {
        return m_color.IsFinite();
    }

    AZ_MATH_INLINE constexpr bool Color::operator==(const Color& rhs) const
    {
        return m_color == rhs.m_color;
    }


    AZ_MATH_INLINE constexpr bool Color::operator!=(const Color& rhs) const
    {
        return m_color != rhs.m_color;
    }


    AZ_MATH_INLINE constexpr Color::operator Vector3() const
    {
        return m_color.GetAsVector3();
    }


    AZ_MATH_INLINE constexpr Color::operator Vector4() const
    {
        return m_color;
    }


    AZ_MATH_INLINE constexpr Color& Color::operator=(const Vector3& rhs)
    {
        Set(rhs);
        return *this;
    }


    // Color to u32 => 0xAABBGGRR (COLREF format)
    AZ_MATH_INLINE constexpr u32 Color::ToU32() const
    {
        return CreateU32(GetR8(), GetG8(), GetB8(), GetA8());
    }


    // Color from u32 => 0xAABBGGRR (COLREF format)
    AZ_MATH_INLINE constexpr void Color::FromU32(u32 c)
    {
        SetA(static_cast<float>(c >> 24) * (1.0f / 255.0f));
        SetB(static_cast<float>((c >> 16) & 0xff) * (1.0f / 255.0f));
        SetG(static_cast<float>((c >> 8) & 0xff) * (1.0f / 255.0f));
        SetR(static_cast<float>(c & 0xff) * (1.0f / 255.0f));
    }


    AZ_MATH_INLINE u32 Color::ToU32LinearToGamma() const
    {
        return LinearToGamma().ToU32();
    }


    AZ_MATH_INLINE void Color::FromU32GammaToLinear(u32 c)
    {
        FromU32(c);
        *this = GammaToLinear();
    }

    AZ_MATH_INLINE float Color::ConvertSrgbGammaToLinear(float x)
    {
        return x <= 0.04045 ? (x / 12.92f) : static_cast<float>(pow((static_cast<double>(x) + 0.055) / 1.055, 2.4));
    }

    AZ_MATH_INLINE float Color::ConvertSrgbLinearToGamma(float x)
    {
        return x <= 0.0031308 ? 12.92f * x : static_cast<float>(1.055 * pow(static_cast<double>(x), 1.0 / 2.4) - 0.055);
    }

    AZ_MATH_INLINE void Color::Saturate()
    {
        m_color = m_color.GetClamp(Vector4(0.f), Vector4(1.f));
    }

    AZ_MATH_INLINE Color Color::GetSaturated() const
    {
        Color copy(*this);
        copy.Saturate();
        return copy;
    }

    AZ_MATH_INLINE Color Color::LinearToGamma() const
    {
        float r = GetR();
        float g = GetG();
        float b = GetB();

        r = ConvertSrgbLinearToGamma(r);
        g = ConvertSrgbLinearToGamma(g);
        b = ConvertSrgbLinearToGamma(b);

        return Color(r,g,b,GetA());
    }


    AZ_MATH_INLINE Color Color::GammaToLinear() const
    {
        float r = GetR();
        float g = GetG();
        float b = GetB();

        return Color(ConvertSrgbGammaToLinear(r),
                     ConvertSrgbGammaToLinear(g),
                     ConvertSrgbGammaToLinear(b), GetA());
    }


    AZ_MATH_INLINE bool Color::IsLessThan(const Color& rhs) const
    {
        return m_color.IsLessThan(rhs.m_color);
    }


    AZ_MATH_INLINE bool Color::IsLessEqualThan(const Color& rhs) const
    {
        return m_color.IsLessEqualThan(rhs.m_color);
    }


    AZ_MATH_INLINE bool Color::IsGreaterThan(const Color& rhs) const
    {
        return m_color.IsGreaterThan(rhs.m_color);
    }


    AZ_MATH_INLINE bool Color::IsGreaterEqualThan(const Color& rhs) const
    {
        return m_color.IsGreaterEqualThan(rhs.m_color);
    }


    AZ_MATH_INLINE Color Color::Lerp(const Color& dest, float t) const
    {
        return Color(m_color.Lerp(dest.m_color, t));
    }


    AZ_MATH_INLINE float Color::Dot(const Color& rhs) const
    {
        return (m_color.Dot(rhs.m_color));
    }


    AZ_MATH_INLINE float Color::Dot3(const Color& rhs) const
    {
        return (m_color.Dot3(rhs.m_color.GetAsVector3()));
    }


    AZ_MATH_INLINE Color Color::operator-() const
    {
        return Color(-m_color);
    }


    AZ_MATH_INLINE Color Color::operator+(const Color& rhs) const
    {
        return Color(m_color + rhs.m_color);
    }


    AZ_MATH_INLINE Color Color::operator-(const Color& rhs) const
    {
        return Color(m_color - rhs.m_color);
    }


    AZ_MATH_INLINE Color Color::operator*(const Color& rhs) const
    {
        return Color(m_color * rhs.m_color);
    }


    AZ_MATH_INLINE Color Color::operator/(const Color& rhs) const
    {
        return Color(m_color / rhs.m_color);
    }


    AZ_MATH_INLINE Color Color::operator*(float multiplier) const
    {
        return Color(m_color * multiplier);
    }


    AZ_MATH_INLINE Color Color::operator/(float divisor) const
    {
        return Color(m_color / divisor);
    }


    AZ_MATH_INLINE Color& Color::operator+=(const Color& rhs)
    {
        *this = (*this) + rhs;
        return *this;
    }


    AZ_MATH_INLINE Color& Color::operator-=(const Color& rhs)
    {
        *this = (*this) - rhs;
        return *this;
    }


    AZ_MATH_INLINE Color& Color::operator*=(const Color& rhs)
    {
        *this = (*this) * rhs;
        return *this;
    }


    AZ_MATH_INLINE Color& Color::operator/=(const Color& rhs)
    {
        *this = (*this) / rhs;
        return *this;
    }


    AZ_MATH_INLINE Color& Color::operator*=(float multiplier)
    {
        *this = (*this) * multiplier;
        return *this;
    }


    AZ_MATH_INLINE Color& Color::operator/=(float divisor)
    {
        *this = (*this) / divisor;
        return *this;
    }


    AZ_MATH_INLINE const Color operator*(float multiplier, const Color& rhs)
    {
        return rhs * multiplier;
    }

    AZ_MATH_INLINE Color operator*(const ColorSwatch& lhs, float multiplier)
    {
        return Color(lhs) * multiplier;
    }

    AZ_MATH_INLINE Color operator/(const ColorSwatch& lhs, float divisor)
    {
        return Color(lhs) / divisor;
    }

    AZ_MATH_INLINE Color operator+(const ColorSwatch& lhs, const Color& rhs)
    {
        return Color(lhs) + rhs;
    }

    AZ_MATH_INLINE Color operator-(const ColorSwatch& lhs, const Color& rhs)
    {
        return Color(lhs) - rhs;
    }

    AZ_MATH_INLINE Color operator*(const ColorSwatch& lhs, const Color& rhs)
    {
        return Color(lhs) * rhs;
    }
}
