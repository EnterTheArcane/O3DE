/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/string/string_view.h>

namespace AZ
{
    namespace Internal
    {
        //! Deliberately not constexpr. Reaching this from a constant expression is a compile error,
        //! which is how a malformed compile-time hex literal gets reported.
        inline bool InvalidHexLiteral()
        {
            AZ_Assert(false, "Malformed hex color literal passed to ColorSwatch::FromHex");
            return false;
        }
    } // namespace Internal

    //! A compile-time color literal: 4 unsigned bytes, no color space association.
    //!
    //! ColorSwatch is the narrowest color representation in AzCore, which is what lets it convert
    //! *implicitly* to every other color type without any of those conversions being lossy. It exists
    //! so that a single named palette (see Colors.h) can be consumed at compile time as any color type:
    //!
    //!     constexpr AZ::Color32 white = AZ::Colors::White;   // exact, no runtime work
    //!     AZ::Color            blue   = AZ::Colors::Blue;    // exact
    //!
    //! Each color type declares its own implicit constructor from ColorSwatch, so this header knows
    //! nothing about them and there is no include cycle.
    //!
    //! @note Active union member rule. During constant evaluation exactly one union member is active,
    //!       and reading an inactive one is ill-formed. The named components (m_r/m_g/m_b/m_a) are
    //!       ALWAYS the active member: every constexpr constructor initializes them and every constexpr
    //!       accessor reads them. m_channels[] and m_packed are runtime-only views -- using them in a
    //!       constant expression is a compile error, not a silent wrong answer. AsU32*() is therefore
    //!       computed from the components; compilers fold that back into a single 32-bit load.
    //!
    //! @note Unlike AZ::Color, the default constructor initializes to opaque black rather than leaving
    //!       the components uninitialized.
    struct ColorSwatch final
    {
        AZ_TYPE_INFO(ColorSwatch, "{6D1A5B4C-8E37-4F92-A0C6-3B7E2D948F15}");

        //! AzCore reflection.
        static AZCORE_API void Reflect(ReflectContext* context);

        constexpr ColorSwatch() = default;

        constexpr ColorSwatch(u8 r, u8 g, u8 b, u8 a = 255)
            : m_r(r), m_g(g), m_b(b), m_a(a)
        {
        }

        //! Creates a fully transparent black swatch.
        static constexpr ColorSwatch CreateZero()
        {
            return ColorSwatch(0, 0, 0, 0);
        }

        //! Creates an opaque white swatch.
        static constexpr ColorSwatch CreateOne()
        {
            return ColorSwatch(255, 255, 255, 255);
        }

        static constexpr ColorSwatch CreateFromRgba(u8 r, u8 g, u8 b, u8 a)
        {
            return ColorSwatch(r, g, b, a);
        }

        //! Creates a swatch with r, g and b set to the same value.
        static constexpr ColorSwatch CreateGray(u8 rgb, u8 a = 255)
        {
            return ColorSwatch(rgb, rgb, rgb, a);
        }

        //! @name Unpacking from a u32, by explicit byte order.
        //! The name states the component order from most significant byte to least significant, so
        //! FromU32Argb(0xAARRGGBB). Prefer these over ambiguous hand-rolled shifts at call sites.
        //! @{
        static constexpr ColorSwatch FromU32Rgba(u32 value)
        {
            return ColorSwatch(Byte3(value), Byte2(value), Byte1(value), Byte0(value));
        }

        static constexpr ColorSwatch FromU32Argb(u32 value)
        {
            return ColorSwatch(Byte2(value), Byte1(value), Byte0(value), Byte3(value));
        }

        static constexpr ColorSwatch FromU32Abgr(u32 value)
        {
            return ColorSwatch(Byte0(value), Byte1(value), Byte2(value), Byte3(value));
        }

        static constexpr ColorSwatch FromU32Bgra(u32 value)
        {
            return ColorSwatch(Byte1(value), Byte2(value), Byte3(value), Byte0(value));
        }
        //! @}

        //! Matches AZ::Color::FromU32 -- reads 0xAABBGGRR (COLREF), i.e. R8G8B8A8_UNORM in memory.
        static constexpr ColorSwatch FromU32(u32 value)
        {
            return FromU32Abgr(value);
        }

        //! Parses "#RGB", "#RRGGBB" or "#RRGGBBAA". The leading '#' is optional.
        //! A malformed literal is a compile error when evaluated at compile time, and asserts at runtime.
        static constexpr ColorSwatch FromHex(AZStd::string_view hex)
        {
            AZStd::string_view digits = (!hex.empty() && hex.front() == '#') ? hex.substr(1) : hex;

            if (digits.size() == 3)
            {
                const u8 r = HexDigit(digits[0]);
                const u8 g = HexDigit(digits[1]);
                const u8 b = HexDigit(digits[2]);
                // Expand each nibble by replication so 0xF becomes 0xFF.
                return ColorSwatch(static_cast<u8>(r * 17), static_cast<u8>(g * 17), static_cast<u8>(b * 17), 255);
            }
            if (digits.size() == 6)
            {
                return ColorSwatch(HexByte(digits, 0), HexByte(digits, 2), HexByte(digits, 4), 255);
            }
            if (digits.size() == 8)
            {
                return ColorSwatch(HexByte(digits, 0), HexByte(digits, 2), HexByte(digits, 4), HexByte(digits, 6));
            }

            return Internal::InvalidHexLiteral() ? ColorSwatch() : ColorSwatch();
        }

        //! @name Component access, as bytes.
        //! @{
        constexpr u8 GetR8() const { return m_r; }
        constexpr u8 GetG8() const { return m_g; }
        constexpr u8 GetB8() const { return m_b; }
        constexpr u8 GetA8() const { return m_a; }

        constexpr void SetR8(u8 r) { m_r = r; }
        constexpr void SetG8(u8 g) { m_g = g; }
        constexpr void SetB8(u8 b) { m_b = b; }
        constexpr void SetA8(u8 a) { m_a = a; }
        //! @}

        //! @name Component access, normalized to [0, 1].
        //! @{
        constexpr float GetR() const { return ToFloat(m_r); }
        constexpr float GetG() const { return ToFloat(m_g); }
        constexpr float GetB() const { return ToFloat(m_b); }
        constexpr float GetA() const { return ToFloat(m_a); }
        //! @}

        //! Indexed component access in r, g, b, a order.
        constexpr u8 GetElement(AZ::u32 index) const
        {
            switch (index)
            {
            case 0:  return m_r;
            case 1:  return m_g;
            case 2:  return m_b;
            default: return m_a;
            }
        }

        constexpr void SetElement(AZ::u32 index, u8 value)
        {
            switch (index)
            {
            case 0:  m_r = value; break;
            case 1:  m_g = value; break;
            case 2:  m_b = value; break;
            default: m_a = value; break;
            }
        }

        constexpr u8 operator[](AZ::u32 index) const { return GetElement(index); }

        //! @name Packing to a u32, by explicit byte order.
        //! The name states the component order from most significant byte to least significant.
        //! @{
        constexpr u32 AsU32Rgba() const { return Pack(m_r, m_g, m_b, m_a); }
        constexpr u32 AsU32Argb() const { return Pack(m_a, m_r, m_g, m_b); }
        constexpr u32 AsU32Abgr() const { return Pack(m_a, m_b, m_g, m_r); }
        constexpr u32 AsU32Bgra() const { return Pack(m_b, m_g, m_r, m_a); }
        //! @}

        //! Matches AZ::Color::ToU32 -- produces 0xAABBGGRR (COLREF), i.e. R8G8B8A8_UNORM in memory.
        constexpr u32 AsU32() const { return AsU32Abgr(); }

        //! @note Not yet constexpr: AZ::Vector3/Vector4 are not literal types.
        constexpr Vector3 AsVector3() const { return Vector3(GetR(), GetG(), GetB()); }
        constexpr Vector4 AsVector4() const { return Vector4(GetR(), GetG(), GetB(), GetA()); }

        //! Returns a copy with the alpha replaced.
        constexpr ColorSwatch WithAlpha(u8 a) const { return ColorSwatch(m_r, m_g, m_b, a); }

        //! Returns a copy with r, g and b inverted. Alpha is unchanged.
        constexpr ColorSwatch Inverted() const
        {
            return ColorSwatch(
                static_cast<u8>(255 - m_r), static_cast<u8>(255 - m_g), static_cast<u8>(255 - m_b), m_a);
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear component values.
        constexpr float GetLuminance() const
        {
            return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB();
        }

        constexpr bool IsOpaque() const { return m_a == 255; }
        constexpr bool IsTransparent() const { return m_a == 0; }

        constexpr bool operator==(const ColorSwatch& rhs) const
        {
            return m_r == rhs.m_r && m_g == rhs.m_g && m_b == rhs.m_b && m_a == rhs.m_a;
        }

        constexpr bool operator!=(const ColorSwatch& rhs) const { return !(*this == rhs); }

        //! @name Ordering, so a swatch can be used as an associative container key.
        //! The order is stable and total but otherwise arbitrary -- it is not perceptual.
        //! @{
        constexpr bool operator<(const ColorSwatch& rhs) const { return AsU32Rgba() < rhs.AsU32Rgba(); }
        constexpr bool operator>(const ColorSwatch& rhs) const { return rhs < *this; }
        constexpr bool operator<=(const ColorSwatch& rhs) const { return !(rhs < *this); }
        constexpr bool operator>=(const ColorSwatch& rhs) const { return !(*this < rhs); }
        //! @}

        union
        {
            //! Always the active member. See the active union member rule above.
            struct
            {
                u8 m_r{ 0 };
                u8 m_g{ 0 };
                u8 m_b{ 0 };
                u8 m_a{ 255 };
            };
            //! Runtime-only views. Reading either in a constant expression is a compile error.
            u8 m_channels[4];
            u32 m_packed;
        };

    private:
        static constexpr float ToFloat(u8 value) { return static_cast<float>(value) * (1.0f / 255.0f); }

        static constexpr u32 Pack(u8 b3, u8 b2, u8 b1, u8 b0)
        {
            return (static_cast<u32>(b3) << 24) | (static_cast<u32>(b2) << 16) | (static_cast<u32>(b1) << 8) |
                static_cast<u32>(b0);
        }

        static constexpr u8 Byte0(u32 v) { return static_cast<u8>(v & 0xFF); }
        static constexpr u8 Byte1(u32 v) { return static_cast<u8>((v >> 8) & 0xFF); }
        static constexpr u8 Byte2(u32 v) { return static_cast<u8>((v >> 16) & 0xFF); }
        static constexpr u8 Byte3(u32 v) { return static_cast<u8>((v >> 24) & 0xFF); }

        static constexpr u8 HexDigit(char c)
        {
            if (c >= '0' && c <= '9') { return static_cast<u8>(c - '0'); }
            if (c >= 'a' && c <= 'f') { return static_cast<u8>(c - 'a' + 10); }
            if (c >= 'A' && c <= 'F') { return static_cast<u8>(c - 'A' + 10); }
            return Internal::InvalidHexLiteral() ? u8(0) : u8(0);
        }

        static constexpr u8 HexByte(AZStd::string_view digits, size_t index)
        {
            return static_cast<u8>(HexDigit(digits[index]) * 16 + HexDigit(digits[index + 1]));
        }
    };

    static_assert(sizeof(ColorSwatch) == 4, "ColorSwatch must be exactly 4 bytes");
    static_assert(alignof(ColorSwatch) == 4, "ColorSwatch must be 4 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<ColorSwatch>, "ColorSwatch must be trivially copyable");
} // namespace AZ

template<>
struct AZStd::hash<AZ::ColorSwatch>
{
    using result_type = AZStd::size_t;
    constexpr result_type operator()(const AZ::ColorSwatch& value) const
    {
        // Computed from the components rather than m_packed so this is usable at compile time.
        return static_cast<result_type>(value.AsU32Rgba());
    }
};
