/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Colors.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/std/typetraits/is_integral.h>
#include <AzCore/std/typetraits/is_floating_point.h>

namespace AZ
{
    //! A color class with 4 components, RGBA.
    class Color
    {
    public:

        AZ_TYPE_INFO(Color, "{7894072A-9050-4F0F-901B-34B1A0D29417}");

        //! AzCore Reflection.
        //! @param context reflection context
        static AZCORE_API void Reflect(ReflectContext* context);

        //! Default constructor, components are uninitialized.
        Color() = default;

        constexpr Color(const Vector4& v)
            : m_color(v)
        {
        }

        //! Constructs from a named color constant, exactly. See AzCore/Math/Colors.h.
        //! Implicit on purpose. The named constants are the narrowest color representation, so this
        //! widens and cannot lose information, and it is what makes `AZ::Color c = AZ::Colors::White` work.
        constexpr Color(const Internal::NamedColor& color)
            : Color(color.m_r, color.m_g, color.m_b, color.m_a)
        {
        }

        AZ_MATH_INLINE explicit Color(const Vector2& source)
        {
            m_color = Vector4(source);
        }

        AZ_MATH_INLINE explicit Color(const Vector3& source)
        {
            m_color = Vector4(source);
        }

        //! Constructs vector with all components set to the same specified value.
        AZ_MATH_INLINE explicit constexpr Color(float rgba)
            : m_color(rgba)
        {
        }

        //! Constructs a color from the given floating point RGBA values in the range [0..1].
        //! A template so that constructing a Color from mixed floating point and integer arguments is not possible.
        template<typename T> requires AZStd::is_floating_point_v<T>
        AZ_MATH_INLINE constexpr Color(T r, T g, T b, T a = T(1))
            : m_color(r, g, b, a)
        {
        }

        //! Constructs a color from the given integer RGBA values in the range [0..255].
        //! A template so that calling the constructor with a non-u8 integer type such as int or unsigned is not ambiguous.
        template<typename T> requires AZStd::is_integral_v<T>
        AZ_MATH_INLINE constexpr Color(T r, T g, T b, T a = T(255))
            // Built in the mem-init list rather than through SetR8..SetA8 so this is a constant expression.
            // The asserts still fire at runtime.
            : m_color(
                static_cast<float>(static_cast<u8>(r)) * (1.0f / 255.0f),
                static_cast<float>(static_cast<u8>(g)) * (1.0f / 255.0f),
                static_cast<float>(static_cast<u8>(b)) * (1.0f / 255.0f),
                static_cast<float>(static_cast<u8>(a)) * (1.0f / 255.0f))
        {
            AZ_MATH_ASSERT(r >= 0 && r <= 255, "R component must be in the range [0..255]");
            AZ_MATH_ASSERT(g >= 0 && g <= 255, "G component must be in the range [0..255]");
            AZ_MATH_ASSERT(b >= 0 && b <= 255, "B component must be in the range [0..255]");
            AZ_MATH_ASSERT(a >= 0 && a <= 255, "A component must be in the range [0..255]");
        }

        static const Color Zero;
        static const Color One;

        //! Creates a vector with all components set to zero, more efficient than calling Color(0.0f).
        AZ_MATH_INLINE static constexpr Color CreateZero()
        {
            return {0.0f, 0.0f, 0.0f, 0.0f};
        }

        //! Creates a vector with all components set to one.
        AZ_MATH_INLINE static constexpr Color CreateOne()
        {
            return Color(1.0f);
        }

        //! Sets components from rgba.
        AZ_MATH_INLINE static constexpr Color CreateFromRgba(u8 r, u8 g, u8 b, u8 a)
        {
            return {r, g, b, a};
        }

        //! Sets components from an array of 4 floats, stored in xyzw order.
        AZ_MATH_INLINE static Color CreateFromFloat4(const float* values)
        {
            Color result;
            result.Set(values);
            return result;
        }

        //! Copies r,g,b components from a Vector3, sets w to 1.0.
        AZ_MATH_INLINE static Color CreateFromVector3(const Vector3& v)
        {
            Color result;
            result.Set(v);
            return result;
        }

        //! Copies r,g,b components from a Vector3, specify w separately.
        AZ_MATH_INLINE static Color CreateFromVector3AndFloat(const Vector3& v, float w)
        {
            Color result;
            result.Set(v, w);
            return result;
        }

        //! r,g,b,a to u32 => 0xAABBGGRR (COLREF format).
        AZ_MATH_INLINE static constexpr u32 CreateU32(u8 r, u8 g, u8 b, u8 a)
        {
            return (a << 24) | (b << 16) | (g << 8) | r;
        }

        //! Stores the vector to an array of 4 floats.
        //! The floats need only be 4 byte aligned, 16 byte alignment is not required.
        AZ_MATH_INLINE void StoreToFloat4(float* values) const
        {
            m_color.StoreToFloat4(values);
        }

        //! @name Channel access, as bytes.
        //! @{
        AZ_MATH_INLINE constexpr u8 GetR8() const
        {
            return static_cast<u8>(m_color.GetX() * 255.0f);
        }

        AZ_MATH_INLINE constexpr u8 GetG8() const
        {
            return static_cast<u8>(m_color.GetY() * 255.0f);
        }

        AZ_MATH_INLINE constexpr u8 GetB8() const
        {
            return static_cast<u8>(m_color.GetZ() * 255.0f);
        }

        AZ_MATH_INLINE constexpr u8 GetA8() const
        {
            return static_cast<u8>(m_color.GetW() * 255.0f);
        }

        AZ_MATH_INLINE constexpr void SetR8(u8 r)
        {
            m_color.SetX(static_cast<float>(r) * (1.0f / 255.0f));
        }

        AZ_MATH_INLINE constexpr void SetG8(u8 g)
        {
            m_color.SetY(static_cast<float>(g) * (1.0f / 255.0f));
        }

        AZ_MATH_INLINE constexpr void SetB8(u8 b)
        {
            m_color.SetZ(static_cast<float>(b) * (1.0f / 255.0f));
        }

        AZ_MATH_INLINE constexpr void SetA8(u8 a)
        {
            m_color.SetW(static_cast<float>(a) * (1.0f / 255.0f));
        }
        //! @}

        //! @name Channel access, normalized to the range [0, 1].
        //! @{
        AZ_MATH_INLINE constexpr float GetR() const
        {
            return m_color.GetX();
        }

        AZ_MATH_INLINE constexpr float GetG() const
        {
            return m_color.GetY();
        }

        AZ_MATH_INLINE constexpr float GetB() const
        {
            return m_color.GetZ();
        }

        AZ_MATH_INLINE constexpr float GetA() const
        {
            return m_color.GetW();
        }

        AZ_MATH_INLINE constexpr void SetR(float r)
        {
            m_color.SetX(r);
        }

        AZ_MATH_INLINE constexpr void SetG(float g)
        {
            m_color.SetY(g);
        }

        AZ_MATH_INLINE constexpr void SetB(float b)
        {
            m_color.SetZ(b);
        }

        AZ_MATH_INLINE constexpr void SetA(float a)
        {
            m_color.SetW(a);
        }
        //! @}

        AZ_MATH_INLINE constexpr float GetElement(int32_t index) const
        {
            return m_color.GetElement(index);
        }

        AZ_MATH_INLINE constexpr void SetElement(int32_t index, float v)
        {
            m_color.SetElement(index, v);
        }

        AZ_MATH_INLINE Vector3 GetAsVector3() const
        {
            return m_color.GetAsVector3();
        }

        AZ_MATH_INLINE constexpr Vector4 GetAsVector4() const
        {
            return m_color;
        }

        //! Sets all components to the same specified value.
        AZ_MATH_INLINE constexpr void Set(float x)
        {
            m_color.Set(x);
        }

        AZ_MATH_INLINE constexpr void Set(float r, float g, float b, float a)
        {
            m_color.Set(r, g, b, a);
        }

        //! Sets components from an array of 4 floats, stored in rgba order.
        AZ_MATH_INLINE void Set(const float values[4])
        {
            m_color.Set(values);
        }

        //! Sets r,g,b components from a Vector3, sets a to 1.0.
        AZ_MATH_INLINE void Set(const Vector3& v)
        {
            m_color.Set(v);
        }

        //! Sets r,g,b components from a Vector3, specify a separately.
        AZ_MATH_INLINE void Set(const Vector3& v, float a)
        {
            m_color.Set(v, a);
        }

        //! Sets the RGB values of this Color based on a passed in hue, saturation, and value. Alpha is unchanged.
        AZ_MATH_INLINE void SetFromHSVRadians(float hueRadians, float saturation, float value)
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
                AZ_MATH_ASSERT(false,
                    "SetFromHSV has generated invalid data from these parameters : H %.5f, S %.5f, V %.5f.",
                    hueRadians,
                    saturation,
                    value);
            }
        }

        //! Checks the color is equal to another within a floating point tolerance.
        AZ_MATH_INLINE bool IsClose(const Color& v, float tolerance = Constants::Tolerance) const
        {
            return m_color.IsClose(v.GetAsVector4(), tolerance);
        }

        AZ_MATH_INLINE bool IsZero(float tolerance = Constants::FloatEpsilon) const
        {
            return IsClose(CreateZero(), tolerance);
        }

        AZ_MATH_INLINE constexpr bool IsOpaque() const
        {
            return GetA() == 1.0f;
        }

        AZ_MATH_INLINE constexpr bool IsTransparent() const
        {
            return GetA() == 0.0f;
        }

        //! Checks whether all components are finite.
        AZ_MATH_INLINE bool IsFinite() const
        {
            return m_color.IsFinite();
        }

        AZ_MATH_INLINE bool operator==(const Color& rhs) const
        {
            return m_color == rhs.m_color;
        }

        AZ_MATH_INLINE bool operator!=(const Color& rhs) const
        {
            return m_color != rhs.m_color;
        }

        AZ_MATH_INLINE explicit operator Vector3() const
        {
            return m_color.GetAsVector3();
        }

        AZ_MATH_INLINE explicit constexpr operator Vector4() const
        {
            return m_color;
        }

        AZ_MATH_INLINE Color& operator=(const Vector3& rhs)
        {
            Set(rhs);
            return *this;
        }

        //! Color to u32 => 0xAABBGGRR.
        AZ_MATH_INLINE constexpr u32 ToU32() const
        {
            return CreateU32(GetR8(), GetG8(), GetB8(), GetA8());
        }

        //! Color to u32 => 0xAABBGGRR, RGB convert from Linear to Gamma corrected values.
        AZ_MATH_INLINE u32 ToU32LinearToGamma() const
        {
            return LinearToGamma().ToU32();
        }

        //! Color from u32 => 0xAABBGGRR.
        AZ_MATH_INLINE constexpr void FromU32(u32 c)
        {
            SetA(static_cast<float>(c >> 24) * (1.0f / 255.0f));
            SetB(static_cast<float>((c >> 16) & 0xff) * (1.0f / 255.0f));
            SetG(static_cast<float>((c >> 8) & 0xff) * (1.0f / 255.0f));
            SetR(static_cast<float>(c & 0xff) * (1.0f / 255.0f));
        }

        //! Color from u32 => 0xAABBGGRR, RGB convert from Gamma corrected to Linear values.
        AZ_MATH_INLINE void FromU32GammaToLinear(u32 c)
        {
            FromU32(c);
            *this = GammaToLinear();
        }

        //! Convert SRGB gamma space to linear space
        AZ_MATH_INLINE static float ConvertSrgbGammaToLinear(float x)
        {
            if (x <= 0.04045)
            {
                return x / 12.92f;
            }
            return static_cast<float>(pow((static_cast<double>(x) + 0.055) / 1.055, 2.4));
        }

        //! Convert SRGB linear space to gamma space
        AZ_MATH_INLINE static float ConvertSrgbLinearToGamma(float x)
        {
            if (x <= 0.0031308)
            {
                return 12.92f * x;
            }
            return static_cast<float>(1.055 * pow(static_cast<double>(x), 1.0 / 2.4) - 0.055);
        }

        //! Clamps the color to the range [0..1]
        AZ_MATH_INLINE void Saturate()
        {
            m_color = m_color.GetClamp(Vector4(0.f), Vector4(1.f));
        }

        //! Returns a color which was clamped in the range [0..1]
        AZ_MATH_INLINE Color GetSaturated() const
        {
            Color copy(*this);
            copy.Saturate();
            return copy;
        }

        //! Convert color from linear to gamma corrected space.
        AZ_MATH_INLINE Color LinearToGamma() const
        {
            float r = GetR();
            float g = GetG();
            float b = GetB();

            r = ConvertSrgbLinearToGamma(r);
            g = ConvertSrgbLinearToGamma(g);
            b = ConvertSrgbLinearToGamma(b);

            return {
                r,
                g,
                b,
                GetA(),
            };
        }

        //! Convert color from gamma corrected to linear space.
        AZ_MATH_INLINE Color GammaToLinear() const
        {
            float r = GetR();
            float g = GetG();
            float b = GetB();

            return {
                ConvertSrgbGammaToLinear(r),
                ConvertSrgbGammaToLinear(g),
                ConvertSrgbGammaToLinear(b),
                GetA(),
            };
        }

        //! Comparison functions, not implemented as operators since that would probably be a little dangerous. These
        //! functions return true only if all components pass the comparison test.
        //! @{
        AZ_MATH_INLINE bool IsLessThan(const Color& rhs) const
        {
            return m_color.IsLessThan(rhs.m_color);
        }

        AZ_MATH_INLINE bool IsLessEqualThan(const Color& rhs) const
        {
            return m_color.IsLessEqualThan(rhs.m_color);
        }

        AZ_MATH_INLINE bool IsGreaterThan(const Color& rhs) const
        {
            return m_color.IsGreaterThan(rhs.m_color);
        }

        AZ_MATH_INLINE bool IsGreaterEqualThan(const Color& rhs) const
        {
            return m_color.IsGreaterEqualThan(rhs.m_color);
        }
        //! @}

        //! Linear interpolation between this color and a destination.
        //! @return (*this)*(1-t) + dest*t
        AZ_MATH_INLINE Color Lerp(const Color& dest, float t) const
        {
            return Color(m_color.Lerp(dest.m_color, t));
        }

        //! Dot product of two colors, uses all 4 components.
        AZ_MATH_INLINE float Dot(const Color& rhs) const
        {
            return m_color.Dot(rhs.m_color);
        }

        //! Dot product of two colors, using only the r,g,b components.
        AZ_MATH_INLINE float Dot3(const Color& rhs) const
        {
            return m_color.Dot3(rhs.m_color.GetAsVector3());
        }

        //! @name Arithmetic.
        //! These go through AZ::Vector4 and therefore through the SIMD layer, which is why none of
        //! them are constant expressions.
        //! @{
        AZ_MATH_INLINE Color operator-() const
        {
            return Color(-m_color);
        }

        AZ_MATH_INLINE Color operator+(const Color& rhs) const
        {
            return Color(m_color + rhs.m_color);
        }

        AZ_MATH_INLINE Color operator-(const Color& rhs) const
        {
            return Color(m_color - rhs.m_color);
        }

        AZ_MATH_INLINE Color operator*(const Color& rhs) const
        {
            return Color(m_color * rhs.m_color);
        }

        AZ_MATH_INLINE Color operator/(const Color& rhs) const
        {
            return Color(m_color / rhs.m_color);
        }

        AZ_MATH_INLINE Color operator*(float multiplier) const
        {
            return Color(m_color * multiplier);
        }

        AZ_MATH_INLINE Color operator/(float divisor) const
        {
            return Color(m_color / divisor);
        }

        AZ_MATH_INLINE Color& operator+=(const Color& rhs)
        {
            *this = (*this) + rhs;
            return *this;
        }

        AZ_MATH_INLINE Color& operator-=(const Color& rhs)
        {
            *this = (*this) - rhs;
            return *this;
        }

        AZ_MATH_INLINE Color& operator*=(const Color& rhs)
        {
            *this = (*this) * rhs;
            return *this;
        }

        AZ_MATH_INLINE Color& operator/=(const Color& rhs)
        {
            *this = (*this) / rhs;
            return *this;
        }

        AZ_MATH_INLINE Color& operator*=(float multiplier)
        {
            *this = (*this) * multiplier;
            return *this;
        }

        AZ_MATH_INLINE Color& operator/=(float divisor)
        {
            *this = (*this) / divisor;
            return *this;
        }

        //! A friend so the scalar can appear on either side of the multiply.
        AZ_MATH_INLINE friend const Color operator*(float multiplier, const Color& rhs)
        {
            return rhs * multiplier;
        }
        //! @}

    private:
        Vector4 m_color;
    };

    inline constexpr Color Color::Zero{0.0f, 0.0f, 0.0f, 0.0f};
    inline constexpr Color Color::One{1.0f, 1.0f, 1.0f, 1.0f};

    // Transitional NamedColor members.
    // Defined here because they need the vector types,
    // and every consumer of the named constants historically included this header.
    inline Vector3 Internal::NamedColor::GetAsVector3() const
    {
        return Vector3(
            static_cast<float>(m_r) * (1.0f / 255.0f),
            static_cast<float>(m_g) * (1.0f / 255.0f),
            static_cast<float>(m_b) * (1.0f / 255.0f));
    }

    inline Vector4 Internal::NamedColor::GetAsVector4() const
    {
        return Vector4(
            static_cast<float>(m_r) * (1.0f / 255.0f),
            static_cast<float>(m_g) * (1.0f / 255.0f),
            static_cast<float>(m_b) * (1.0f / 255.0f),
            static_cast<float>(m_a) * (1.0f / 255.0f));
    }
}
