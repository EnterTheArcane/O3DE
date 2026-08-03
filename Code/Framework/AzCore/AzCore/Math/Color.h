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
    class AZCORE_API Color
    {
    public:

        AZ_TYPE_INFO(Color, "{7894072A-9050-4F0F-901B-34B1A0D29417}");

        //! AzCore Reflection.
        //! @param context reflection context
        static void Reflect(ReflectContext* context);

        //! Default constructor, components are uninitialized.
        Color() = default;
        constexpr Color(const Vector4& v) : m_color(v) {}

        //! Constructs from a compile-time color literal, exactly. See AzCore/Math/ColorSwatch.h.
        //! Implicit on purpose: ColorSwatch is the narrowest color representation, so this widens
        //! and cannot lose information. It is what makes `AZ::Color c = AZ::Colors::White;` work.
        constexpr Color(const ColorSwatch& swatch);

        explicit Color(const Vector2& source);

        explicit constexpr Color(const Vector3& source);

        //! Constructs vector with all components set to the same specified value.
        explicit constexpr Color(float rgba);

        //! Constructs a color from the given floating point RGBA values in the range [0..1].
        //! This must be a template function so that constructing a Color with mixed floating point and integer arguments is not possible.
        template<typename T> requires AZStd::is_floating_point_v<T>
        constexpr Color(T r, T g, T b, T a = T(1));

        //! Constructs a color from the given integer RGBA values in the range [0..255].
        //! This must be a template function so that calling the constructor with non-u8 integer types such as int/uint is not ambiguous.
        template<typename T> requires AZStd::is_integral_v<T>
        constexpr Color(T r, T g, T b, T a = T(255));

        //! Creates a vector with all components set to zero, more efficient than calling Color(0.0f).
        static constexpr Color CreateZero();

        //! Creates a vector with all components set to one.
        static constexpr Color CreateOne();

        //! Sets components from rgba.
        static constexpr Color CreateFromRgba(u8 r, u8 g, u8 b, u8 a);

        //! Sets components from an array of 4 floats, stored in xyzw order.
        static Color CreateFromFloat4(const float* values);

        //! Copies r,g,b components from a Vector3, sets w to 1.0.
        static constexpr Color CreateFromVector3(const Vector3& v);

        //! Copies r,g,b components from a Vector3, specify w separately.
        static constexpr Color CreateFromVector3AndFloat(const Vector3& v, float w);

        //! r,g,b,a to u32 => 0xAABBGGRR (COLREF format).
        static constexpr u32 CreateU32(u8 r, u8 g, u8 b, u8 a);

        //! Stores the vector to an array of 4 floats.
        //! The floats need only be 4 byte aligned, 16 byte alignment is not required.
        void StoreToFloat4(float* values) const;

        constexpr u8 GetR8() const;
        constexpr u8 GetG8() const;
        constexpr u8 GetB8() const;
        constexpr u8 GetA8() const;

        constexpr void SetR8(u8 r);
        constexpr void SetG8(u8 g);
        constexpr void SetB8(u8 b);
        constexpr void SetA8(u8 a);

        constexpr float GetR() const;
        constexpr float GetG() const;
        constexpr float GetB() const;
        constexpr float GetA() const;

        constexpr void SetR(float r);
        constexpr void SetG(float g);
        constexpr void SetB(float b);
        constexpr void SetA(float a);

        constexpr float GetElement(int32_t index) const;
        constexpr void SetElement(int32_t index, float v);

        constexpr Vector3 GetAsVector3() const;

        constexpr Vector4 GetAsVector4() const;

        //! Sets all components to the same specified value.
        constexpr void Set(float x);

        constexpr void Set(float r, float g, float b, float a);

        //! Sets components from an array of 4 floats, stored in rgba order.
        void Set(const float values[4]);

        //! Sets r,g,b components from a Vector3, sets a to 1.0.
        constexpr void Set(const Vector3& v);

        //! Sets r,g,b components from a Vector3, specify a separately.
        constexpr void Set(const Vector3& v, float a);

        //! Sets the RGB values of this Color based on a passed in hue, saturation, and value. Alpha is unchanged.
        void SetFromHSVRadians(float hueRadians, float saturation, float value);

        //! Checks the color is equal to another within a floating point tolerance.
        bool IsClose(const Color& v, float tolerance = Constants::Tolerance) const;

        bool IsZero(float tolerance = Constants::FloatEpsilon) const;

        //! Checks whether all components are finite.
        bool IsFinite() const;

        constexpr bool operator==(const Color& rhs) const;
        constexpr bool operator!=(const Color& rhs) const;

        explicit constexpr operator Vector3() const;
        explicit constexpr operator Vector4() const;

        constexpr Color& operator=(const Vector3& rhs);

        //! Color to u32 => 0xAABBGGRR.
        constexpr u32 ToU32() const;

        //! Color to u32 => 0xAABBGGRR, RGB convert from Linear to Gamma corrected values.
        u32 ToU32LinearToGamma() const;

        //! Color from u32 => 0xAABBGGRR.
        constexpr void FromU32(u32 c);

        //! Color from u32 => 0xAABBGGRR, RGB convert from Gamma corrected to Linear values.
        void FromU32GammaToLinear(u32 c);

        //! Convert SRGB gamma space to linear space
        static float ConvertSrgbGammaToLinear(float x);

        //! Convert SRGB linear space to gamma space
        static float ConvertSrgbLinearToGamma(float x);

        //! Clamps the color to the range [0..1]
        void Saturate();

        //! Returns a color which was clamped in the range [0..1]
        Color GetSaturated() const;

        //! Convert color from linear to gamma corrected space.
        Color LinearToGamma() const;

        //! Convert color from gamma corrected to linear space.
        Color GammaToLinear() const;

        //! Comparison functions, not implemented as operators since that would probably be a little dangerous. These
        //! functions return true only if all components pass the comparison test.
        //! @{
        bool IsLessThan(const Color& rhs) const;
        bool IsLessEqualThan(const Color& rhs) const;
        bool IsGreaterThan(const Color& rhs) const;
        bool IsGreaterEqualThan(const Color& rhs) const;
        //! @}

        //! Linear interpolation between this color and a destination.
        //! @return (*this)*(1-t) + dest*t
        Color Lerp(const Color& dest, float t) const;

        //! Dot product of two colors, uses all 4 components.
        float Dot(const Color& rhs) const;

        //! Dot product of two colors, using only the r,g,b components.
        float Dot3(const Color& rhs) const;

        Color operator-() const;
        Color operator+(const Color& rhs) const;
        Color operator-(const Color& rhs) const;
        Color operator*(const Color& rhs) const;
        Color operator/(const Color& rhs) const;
        Color operator*(float multiplier) const;
        Color operator/(float divisor) const;

        Color& operator+=(const Color& rhs);
        Color& operator-=(const Color& rhs);
        Color& operator*=(const Color& rhs);
        Color& operator/=(const Color& rhs);
        Color& operator*=(float multiplier);
        Color& operator/=(float divisor);

    private:

        Vector4 m_color;

    };

    //! @name Arithmetic on a palette literal, yielding a Color.
    //! Color's own arithmetic operators are members, so an implicit ColorSwatch -> Color conversion
    //! cannot apply to the left-hand operand. These free functions restore that, keeping expressions
    //! like `AZ::Colors::White * 0.5f` working.
    //! @{
    Color operator*(const ColorSwatch& lhs, float multiplier);
    Color operator/(const ColorSwatch& lhs, float divisor);
    Color operator+(const ColorSwatch& lhs, const Color& rhs);
    Color operator-(const ColorSwatch& lhs, const Color& rhs);
    Color operator*(const ColorSwatch& lhs, const Color& rhs);
    //! @}
}

#include <AzCore/Math/Color.inl>
