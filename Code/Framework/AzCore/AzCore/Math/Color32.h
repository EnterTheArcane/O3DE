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
#include <AzCore/Math/ColorSwatch.h>
#include <AzCore/Math/Internal/ColorTables.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/string/fixed_string.h>

namespace AZ
{
    //! A 32 bit color: 4 unsigned 8 bit channels, normalized over [0, 255], with no color space
    //! association. Maps directly to RHI::Format::R8G8B8A8_UNORM.
    //!
    //! Conversions follow the rule that widening is implicit and narrowing is explicit:
    //!
    //!     constexpr Color32 white = AZ::Colors::White;   // implicit and exact, at compile time
    //!     Color64 wide = white;                          // implicit: u8 -> u16 is exact (v * 257)
    //!     Color   full = white;                          // implicit: u8 -> float is exact
    //!     Color32 narrowed{ someColor };                  // explicit: float -> u8 rounds and clamps
    //!
    //! @note Unlike AZ::Color::GetR8(), which truncates and can wrap for values above 1.0, narrowing
    //!       into Color32 rounds to nearest and clamps.
    //!
    //! @note Active union member rule. The named components (m_r/m_g/m_b/m_a) are ALWAYS the active
    //!       union member: every constexpr constructor initializes them and every constexpr accessor
    //!       reads them. m_channels[] and m_packed are runtime-only views -- using either in a constant
    //!       expression is a compile error rather than a silent wrong answer.
    struct Color32 final
    {
        AZ_TYPE_INFO(Color32, "{9E4B71C3-5A26-4D8F-B013-7C2A6F85D934}");

        //! AzCore reflection.
        static AZCORE_API void Reflect(ReflectContext* context);

        //! Largest representable channel value. A channel of MaxChannel means 1.0.
        static constexpr u8 MaxChannel = 255;

        constexpr Color32() = default;

        constexpr Color32(u8 r, u8 g, u8 b, u8 a = MaxChannel)
            : m_r(r)
            , m_g(g)
            , m_b(b)
            , m_a(a)
        {
        }

        //! Widening from a compile-time color literal. Implicit and exact -- both are 8 bit.
        constexpr Color32(const ColorSwatch& swatch)
            : m_r(swatch.GetR8())
            , m_g(swatch.GetG8())
            , m_b(swatch.GetB8())
            , m_a(swatch.GetA8())
        {
        }

        //! Narrowing from a 64 bit color. Explicit. Exact for any value that originated as 8 bit,
        //! because u8 -> u16 replicates bits (v * 257) and this takes the high byte back.
        explicit constexpr Color32(const Color64& color)
            : m_r(static_cast<u8>(color.GetR16() >> 8))
            , m_g(static_cast<u8>(color.GetG16() >> 8))
            , m_b(static_cast<u8>(color.GetB16() >> 8))
            , m_a(static_cast<u8>(color.GetA16() >> 8))
        {
        }

        //! Narrowing from a floating point color. Explicit: rounds to nearest and clamps to [0, 1].
        explicit constexpr Color32(const Color& color)
            : m_r(FromFloat(color.GetR()))
            , m_g(FromFloat(color.GetG()))
            , m_b(FromFloat(color.GetB()))
            , m_a(FromFloat(color.GetA()))
        {
        }

        static constexpr Color32 CreateZero() { return Color32(0, 0, 0, 0); }
        static constexpr Color32 CreateOne() { return Color32(MaxChannel, MaxChannel, MaxChannel, MaxChannel); }

        static constexpr Color32 CreateFromRgba(u8 r, u8 g, u8 b, u8 a) { return Color32(r, g, b, a); }

        //! Creates a color with r, g and b set to the same value.
        static constexpr Color32 CreateGray(u8 rgb, u8 a = MaxChannel) { return Color32(rgb, rgb, rgb, a); }

        //! @name Unpacking from a u32, by explicit byte order.
        //! The name states the channel order from most significant byte to least significant, so
        //! FromU32Argb reads 0xAARRGGBB. Prefer these over hand-rolled shifts at call sites.
        //! @{
        static constexpr Color32 FromU32Rgba(u32 v) { return Color32(Byte3(v), Byte2(v), Byte1(v), Byte0(v)); }
        static constexpr Color32 FromU32Argb(u32 v) { return Color32(Byte2(v), Byte1(v), Byte0(v), Byte3(v)); }
        static constexpr Color32 FromU32Abgr(u32 v) { return Color32(Byte0(v), Byte1(v), Byte2(v), Byte3(v)); }
        static constexpr Color32 FromU32Bgra(u32 v) { return Color32(Byte1(v), Byte2(v), Byte3(v), Byte0(v)); }
        //! @}

        //! Matches AZ::Color::FromU32 -- reads 0xAABBGGRR (COLREF), i.e. R8G8B8A8_UNORM in memory.
        static constexpr Color32 FromU32(u32 v) { return FromU32Abgr(v); }

        //! Parses "#RGB", "#RRGGBB" or "#RRGGBBAA". The leading '#' is optional.
        static constexpr Color32 FromHex(AZStd::string_view hex) { return Color32(ColorSwatch::FromHex(hex)); }

        //! @name Channel access, as bytes.
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

        //! @name Channel access, normalized to [0, 1].
        //! @{
        constexpr float GetR() const { return ToFloat(m_r); }
        constexpr float GetG() const { return ToFloat(m_g); }
        constexpr float GetB() const { return ToFloat(m_b); }
        constexpr float GetA() const { return ToFloat(m_a); }

        constexpr void SetR(float r) { m_r = FromFloat(r); }
        constexpr void SetG(float g) { m_g = FromFloat(g); }
        constexpr void SetB(float b) { m_b = FromFloat(b); }
        constexpr void SetA(float a) { m_a = FromFloat(a); }
        //! @}

        constexpr u8 GetElement(u32 index) const
        {
            switch (index)
            {
            case 0:  return m_r;
            case 1:  return m_g;
            case 2:  return m_b;
            default: return m_a;
            }
        }

        constexpr void SetElement(u32 index, u8 value)
        {
            switch (index)
            {
            case 0:  m_r = value; break;
            case 1:  m_g = value; break;
            case 2:  m_b = value; break;
            default: m_a = value; break;
            }
        }

        constexpr u8 operator[](u32 index) const { return GetElement(index); }

        //! @name Packing to a u32, by explicit byte order.
        //! AsU32Rgba is 0xRRGGBBAA, AsU32Abgr is 0xAABBGGRR (= R8G8B8A8_UNORM in memory), and
        //! AsU32Argb is 0xAARRGGBB (the order LyShine and AtomFont use).
        //! @{
        constexpr u32 AsU32Rgba() const { return Pack(m_r, m_g, m_b, m_a); }
        constexpr u32 AsU32Argb() const { return Pack(m_a, m_r, m_g, m_b); }
        constexpr u32 AsU32Abgr() const { return Pack(m_a, m_b, m_g, m_r); }
        constexpr u32 AsU32Bgra() const { return Pack(m_b, m_g, m_r, m_a); }
        //! @}

        //! Matches AZ::Color::ToU32 -- produces 0xAABBGGRR (COLREF), i.e. R8G8B8A8_UNORM in memory.
        constexpr u32 AsU32() const { return AsU32Abgr(); }

        //! Widening to a 64 bit color. Implicit and exact, by bit replication.
        constexpr operator Color64() const { return Color64(ToU16(m_r), ToU16(m_g), ToU16(m_b), ToU16(m_a)); }

        //! Widening to a floating point color. Implicit and exact.
        constexpr operator Color() const { return Color(GetR(), GetG(), GetB(), GetA()); }

        //! @note Not yet constexpr: AZ::Vector3/Vector4 are not literal types.
        constexpr Vector3 AsVector3() const { return Vector3(GetR(), GetG(), GetB()); }
        constexpr Vector4 AsVector4() const { return Vector4(GetR(), GetG(), GetB(), GetA()); }

        constexpr ColorSwatch AsColorSwatch() const { return ColorSwatch(m_r, m_g, m_b, m_a); }

        //! Renders as "#RRGGBBAA".
        constexpr AZStd::fixed_string<9> AsHexString() const
        {
            char buffer[9] = {};
            buffer[0] = '#';
            WriteHex8(buffer, 1, m_r);
            WriteHex8(buffer, 3, m_g);
            WriteHex8(buffer, 5, m_b);
            WriteHex8(buffer, 7, m_a);
            return AZStd::fixed_string<9>(buffer, 9);
        }

        //! @name sRGB transfer function.
        //! These are explicit conversions, not a property of the type: Color32 itself carries no color
        //! space association, so it is the call site that decides what its channels mean.
        //! @{

        //! Interprets the channels as sRGB gamma encoded and returns the linear equivalent.
        //! Exact, and usable in a constant expression: it is a table lookup rather than a pow().
        constexpr Color SrgbToLinear() const
        {
            return Color(Internal::SrgbU8ToLinear(m_r), Internal::SrgbU8ToLinear(m_g),
                Internal::SrgbU8ToLinear(m_b), ToFloat(m_a));
        }

        //! Encodes a linear color into sRGB gamma space. Alpha is linear and is not transformed.
        static constexpr Color32 LinearToSrgb(const Color& linear)
        {
            return Color32(Internal::LinearToSrgbU8(linear.GetR()), Internal::LinearToSrgbU8(linear.GetG()),
                Internal::LinearToSrgbU8(linear.GetB()), FromFloat(linear.GetA()));
        }
        //! @}

        constexpr Color32 WithAlpha(u8 a) const { return Color32(m_r, m_g, m_b, a); }

        constexpr Color32 Inverted() const
        {
            return Color32(static_cast<u8>(MaxChannel - m_r), static_cast<u8>(MaxChannel - m_g),
                static_cast<u8>(MaxChannel - m_b), m_a);
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear channel values.
        constexpr float GetLuminance() const { return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB(); }

        constexpr u8 GetMaxComponent() const { return GetMax(GetMax(m_r, m_g), m_b); }
        constexpr u8 GetMinComponent() const { return GetMin(GetMin(m_r, m_g), m_b); }

        constexpr bool IsOpaque() const { return m_a == MaxChannel; }
        constexpr bool IsTransparent() const { return m_a == 0; }

        //! Linear interpolation towards @p dest. @p t is clamped to [0, 1].
        constexpr Color32 Lerp(const Color32& dest, float t) const
        {
            const float clamped = ClampUnitInterval(t);
            return Color32(LerpChannel(m_r, dest.m_r, clamped), LerpChannel(m_g, dest.m_g, clamped),
                LerpChannel(m_b, dest.m_b, clamped), LerpChannel(m_a, dest.m_a, clamped));
        }

        //! Source-over alpha compositing of @p src on top of this color.
        constexpr Color32 Over(const Color32& src) const
        {
            const u32 sa = src.m_a;
            const u32 inv = MaxChannel - sa;
            return Color32(OverChannel(src.m_r, sa, m_r, inv), OverChannel(src.m_g, sa, m_g, inv),
                OverChannel(src.m_b, sa, m_b, inv),
                static_cast<u8>(sa + Modulate(static_cast<u8>(inv), m_a)));
        }

        //! Multiplies r, g and b by alpha.
        constexpr Color32 Premultiply() const
        {
            return Color32(Modulate(m_r, m_a), Modulate(m_g, m_a), Modulate(m_b, m_a), m_a);
        }

        //! @name Arithmetic.
        //! Integer color arithmetic is not float color arithmetic: addition and subtraction
        //! **saturate** rather than wrap, and color * color modulates, i.e. (a * b) / MaxChannel.
        //! @{
        constexpr Color32 operator+(const Color32& rhs) const
        {
            return Color32(AddSat(m_r, rhs.m_r), AddSat(m_g, rhs.m_g), AddSat(m_b, rhs.m_b), AddSat(m_a, rhs.m_a));
        }

        constexpr Color32 operator-(const Color32& rhs) const
        {
            return Color32(SubSat(m_r, rhs.m_r), SubSat(m_g, rhs.m_g), SubSat(m_b, rhs.m_b), SubSat(m_a, rhs.m_a));
        }

        constexpr Color32 operator*(const Color32& rhs) const
        {
            return Color32(
                Modulate(m_r, rhs.m_r), Modulate(m_g, rhs.m_g), Modulate(m_b, rhs.m_b), Modulate(m_a, rhs.m_a));
        }

        constexpr Color32 operator*(float scale) const
        {
            return Color32(ScaleChannel(m_r, scale), ScaleChannel(m_g, scale), ScaleChannel(m_b, scale),
                ScaleChannel(m_a, scale));
        }

        constexpr Color32 operator/(float divisor) const { return (*this) * (1.0f / divisor); }

        constexpr Color32& operator+=(const Color32& rhs) { return *this = *this + rhs; }
        constexpr Color32& operator-=(const Color32& rhs) { return *this = *this - rhs; }
        constexpr Color32& operator*=(const Color32& rhs) { return *this = *this * rhs; }
        constexpr Color32& operator*=(float scale) { return *this = *this * scale; }
        constexpr Color32& operator/=(float divisor) { return *this = *this / divisor; }
        //! @}

        constexpr bool operator==(const Color32& rhs) const
        {
            return m_r == rhs.m_r && m_g == rhs.m_g && m_b == rhs.m_b && m_a == rhs.m_a;
        }

        constexpr bool operator!=(const Color32& rhs) const { return !(*this == rhs); }

        //! @name Ordering, so a color can be used as an associative container key.
        //! The order is stable and total but arbitrary -- it is not perceptual.
        //! @{
        constexpr bool operator<(const Color32& rhs) const { return AsU32Rgba() < rhs.AsU32Rgba(); }
        constexpr bool operator>(const Color32& rhs) const { return rhs < *this; }
        constexpr bool operator<=(const Color32& rhs) const { return !(rhs < *this); }
        constexpr bool operator>=(const Color32& rhs) const { return !(*this < rhs); }
        //! @}

        union
        {
            //! Always the active member. See the active union member rule above.
            struct
            {
                u8 m_r{ 0 };
                u8 m_g{ 0 };
                u8 m_b{ 0 };
                u8 m_a{ MaxChannel };
            };
            //! Runtime-only views. Reading either in a constant expression is a compile error.
            u8 m_channels[4];
            u32 m_packed;
        };

    private:
        static constexpr float ToFloat(u8 v) { return static_cast<float>(v) * (1.0f / 255.0f); }

        //! Bit replication, so 0xFF maps to 0xFFFF exactly.
        static constexpr u16 ToU16(u8 v) { return static_cast<u16>(v * 257); }

        //! Clamps to [0, 1] with defined behaviour for NaN.
        //!
        //! AZ::GetClamp cannot be used here: it tests `value < min` then `value > max`, both of which
        //! are false for NaN, so it returns NaN unchanged -- and static_cast<u8> of NaN is undefined
        //! behaviour. Ordering the tests so that an unordered input falls through to 0 makes a NaN
        //! channel deterministically transparent/black instead.
        static constexpr float ClampUnitInterval(float v)
        {
            return (v > 0.0f) ? ((v < 1.0f) ? v : 1.0f) : 0.0f;
        }

        static constexpr u8 FromFloat(float v)
        {
            return static_cast<u8>(ClampUnitInterval(v) * 255.0f + 0.5f);
        }

        static constexpr u32 Pack(u8 b3, u8 b2, u8 b1, u8 b0)
        {
            return (static_cast<u32>(b3) << 24) | (static_cast<u32>(b2) << 16) | (static_cast<u32>(b1) << 8) |
                static_cast<u32>(b0);
        }

        static constexpr u8 Byte0(u32 v) { return static_cast<u8>(v & 0xFF); }
        static constexpr u8 Byte1(u32 v) { return static_cast<u8>((v >> 8) & 0xFF); }
        static constexpr u8 Byte2(u32 v) { return static_cast<u8>((v >> 16) & 0xFF); }
        static constexpr u8 Byte3(u32 v) { return static_cast<u8>((v >> 24) & 0xFF); }

        static constexpr u8 AddSat(u8 a, u8 b)
        {
            const u32 sum = static_cast<u32>(a) + static_cast<u32>(b);
            return static_cast<u8>(sum > MaxChannel ? MaxChannel : sum);
        }

        static constexpr u8 SubSat(u8 a, u8 b) { return static_cast<u8>(a > b ? a - b : 0); }

        //! Exact normalized multiply: round(a * b / 255).
        static constexpr u8 Modulate(u8 a, u8 b)
        {
            return static_cast<u8>((static_cast<u32>(a) * static_cast<u32>(b) + 127u) / 255u);
        }

        static constexpr u8 ScaleChannel(u8 v, float scale) { return FromFloat(ToFloat(v) * scale); }

        static constexpr u8 LerpChannel(u8 a, u8 b, float t)
        {
            return static_cast<u8>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t + 0.5f);
        }

        static constexpr u8 OverChannel(u8 srcChannel, u32 srcAlpha, u8 dstChannel, u32 invAlpha)
        {
            return AddSat(Modulate(srcChannel, static_cast<u8>(srcAlpha)),
                Modulate(dstChannel, static_cast<u8>(invAlpha)));
        }

        static constexpr void WriteHex8(char* buffer, size_t offset, u8 value)
        {
            constexpr char digits[] = "0123456789ABCDEF";
            buffer[offset + 0] = digits[(value >> 4) & 0xF];
            buffer[offset + 1] = digits[value & 0xF];
        }
    };

    static_assert(sizeof(Color32) == 4, "Color32 must be exactly 4 bytes");
    static_assert(alignof(Color32) == 4, "Color32 must be 4 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<Color32>, "Color32 must be trivially copyable");
} // namespace AZ

namespace AZStd
{
    template<>
    struct hash<AZ::Color32>
    {
        using result_type = AZStd::size_t;
        constexpr result_type operator()(const AZ::Color32& value) const
        {
            // Computed from the components rather than m_packed so this is usable at compile time.
            return static_cast<result_type>(value.AsU32Rgba());
        }
    };
} // namespace AZStd
