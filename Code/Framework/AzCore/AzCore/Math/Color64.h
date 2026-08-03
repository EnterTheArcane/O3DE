/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Color.h>
#include <AzCore/Math/ColorSwatch.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/string/fixed_string.h>

namespace AZ
{
    //! A 64 bit color: 4 unsigned 16 bit channels, normalized over [0, 65535], with no color space
    //! association. Maps directly to RHI::Format::R16G16B16A16_UNORM.
    //!
    //! This is the high precision *low dynamic range* color. It cannot represent values outside
    //! [0, 1]; for HDR use ColorHalf, which stores IEEE binary16 and maps to R16G16B16A16_FLOAT.
    //!
    //! Conversions follow the rule that widening is implicit and narrowing is explicit:
    //!
    //!     Color64 wide = AZ::Colors::White;   // implicit: ColorSwatch is narrower, so this is exact
    //!     Color   full = wide;                // implicit: u16 -> float is exact
    //!     Color64 narrowed{ someColor };       // explicit: float -> u16 rounds and clamps
    //!
    //! @note Active union member rule. The named components (m_r/m_g/m_b/m_a) are ALWAYS the active
    //!       union member: every constexpr constructor initializes them and every constexpr accessor
    //!       reads them. m_channels[] and m_packed are runtime-only views -- using either in a constant
    //!       expression is a compile error rather than a silent wrong answer.
    struct Color64 final
    {
        AZ_TYPE_INFO(Color64, "{2F84C7E1-9B36-4A58-8D07-C41E5B93A6D2}");

        //! AzCore reflection.
        static AZCORE_API void Reflect(ReflectContext* context);

        //! Largest representable channel value. A channel of MaxChannel means 1.0.
        static constexpr u16 MaxChannel = 65535;

        constexpr Color64() = default;

        constexpr Color64(u16 r, u16 g, u16 b, u16 a = MaxChannel)
            : m_r(r), m_g(g), m_b(b), m_a(a)
        {
        }

        //! Widening from a compile-time color literal. Implicit and exact: an 8 bit channel expands to
        //! 16 bits by bit replication (v * 257), so 0xFF becomes 0xFFFF rather than 0xFF00.
        constexpr Color64(const ColorSwatch& swatch)
            : m_r(FromU8(swatch.GetR8()))
            , m_g(FromU8(swatch.GetG8()))
            , m_b(FromU8(swatch.GetB8()))
            , m_a(FromU8(swatch.GetA8()))
        {
        }

        //! Narrowing from a floating point color. Explicit: rounds to nearest and clamps to [0, 1].
        explicit constexpr Color64(const Color& color)
            : m_r(FromFloat(color.GetR()))
            , m_g(FromFloat(color.GetG()))
            , m_b(FromFloat(color.GetB()))
            , m_a(FromFloat(color.GetA()))
        {
        }

        static constexpr Color64 CreateZero() { return Color64(0, 0, 0, 0); }
        static constexpr Color64 CreateOne() { return Color64(MaxChannel, MaxChannel, MaxChannel, MaxChannel); }

        static constexpr Color64 CreateFromRgba(u16 r, u16 g, u16 b, u16 a) { return Color64(r, g, b, a); }

        //! Creates a color with r, g and b set to the same value.
        static constexpr Color64 CreateGray(u16 rgb, u16 a = MaxChannel) { return Color64(rgb, rgb, rgb, a); }

        //! @name Unpacking from a u64, by explicit channel order.
        //! The name states the channel order from most significant word to least significant, so
        //! FromU64Argb reads 0xAAAARRRRGGGGBBBB.
        //! @{
        static constexpr Color64 FromU64Rgba(u64 v) { return Color64(Word3(v), Word2(v), Word1(v), Word0(v)); }
        static constexpr Color64 FromU64Argb(u64 v) { return Color64(Word2(v), Word1(v), Word0(v), Word3(v)); }
        static constexpr Color64 FromU64Abgr(u64 v) { return Color64(Word0(v), Word1(v), Word2(v), Word3(v)); }
        static constexpr Color64 FromU64Bgra(u64 v) { return Color64(Word1(v), Word2(v), Word3(v), Word0(v)); }
        //! @}

        //! @name Channel access, as 16 bit integers.
        //! @{
        constexpr u16 GetR16() const { return m_r; }
        constexpr u16 GetG16() const { return m_g; }
        constexpr u16 GetB16() const { return m_b; }
        constexpr u16 GetA16() const { return m_a; }

        constexpr void SetR16(u16 r) { m_r = r; }
        constexpr void SetG16(u16 g) { m_g = g; }
        constexpr void SetB16(u16 b) { m_b = b; }
        constexpr void SetA16(u16 a) { m_a = a; }
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

        constexpr u16 GetElement(u32 index) const
        {
            switch (index)
            {
            case 0:  return m_r;
            case 1:  return m_g;
            case 2:  return m_b;
            default: return m_a;
            }
        }

        constexpr void SetElement(u32 index, u16 value)
        {
            switch (index)
            {
            case 0:  m_r = value; break;
            case 1:  m_g = value; break;
            case 2:  m_b = value; break;
            default: m_a = value; break;
            }
        }

        constexpr u16 operator[](u32 index) const { return GetElement(index); }

        //! @name Packing to a u64, by explicit channel order.
        //! @{
        constexpr u64 AsU64Rgba() const { return Pack(m_r, m_g, m_b, m_a); }
        constexpr u64 AsU64Argb() const { return Pack(m_a, m_r, m_g, m_b); }
        constexpr u64 AsU64Abgr() const { return Pack(m_a, m_b, m_g, m_r); }
        constexpr u64 AsU64Bgra() const { return Pack(m_b, m_g, m_r, m_a); }
        //! @}

        //! Widening to a floating point color. Implicit and exact.
        constexpr operator Color() const { return Color(GetR(), GetG(), GetB(), GetA()); }

        //! @note Not yet constexpr: AZ::Vector3/Vector4 are not literal types.
        constexpr Vector3 AsVector3() const { return Vector3(GetR(), GetG(), GetB()); }
        constexpr Vector4 AsVector4() const { return Vector4(GetR(), GetG(), GetB(), GetA()); }

        //! Renders as "#RRRRGGGGBBBBAAAA".
        constexpr AZStd::fixed_string<17> AsHexString() const
        {
            char buffer[17] = {};
            buffer[0] = '#';
            WriteHex16(buffer, 1, m_r);
            WriteHex16(buffer, 5, m_g);
            WriteHex16(buffer, 9, m_b);
            WriteHex16(buffer, 13, m_a);
            return AZStd::fixed_string<17>(buffer, 17);
        }

        constexpr Color64 WithAlpha(u16 a) const { return Color64(m_r, m_g, m_b, a); }

        constexpr Color64 Inverted() const
        {
            return Color64(static_cast<u16>(MaxChannel - m_r), static_cast<u16>(MaxChannel - m_g),
                static_cast<u16>(MaxChannel - m_b), m_a);
        }

        //! Relative luminance using the Rec. 709 primaries. Assumes linear channel values.
        constexpr float GetLuminance() const { return 0.2126f * GetR() + 0.7152f * GetG() + 0.0722f * GetB(); }

        constexpr u16 GetMaxComponent() const { return GetMax(GetMax(m_r, m_g), m_b); }
        constexpr u16 GetMinComponent() const { return GetMin(GetMin(m_r, m_g), m_b); }

        constexpr bool IsOpaque() const { return m_a == MaxChannel; }
        constexpr bool IsTransparent() const { return m_a == 0; }

        //! Linear interpolation towards @p dest. @p t is clamped to [0, 1].
        constexpr Color64 Lerp(const Color64& dest, float t) const
        {
            const float clamped = ClampUnitInterval(t);
            return Color64(LerpChannel(m_r, dest.m_r, clamped), LerpChannel(m_g, dest.m_g, clamped),
                LerpChannel(m_b, dest.m_b, clamped), LerpChannel(m_a, dest.m_a, clamped));
        }

        //! Source-over alpha compositing of @p src on top of this color.
        constexpr Color64 Over(const Color64& src) const
        {
            const u32 sa = src.m_a;
            const u32 inv = MaxChannel - sa;
            return Color64(OverChannel(src.m_r, sa, m_r, inv), OverChannel(src.m_g, sa, m_g, inv),
                OverChannel(src.m_b, sa, m_b, inv),
                static_cast<u16>(sa + Modulate(static_cast<u16>(inv), m_a)));
        }

        //! Multiplies r, g and b by alpha.
        constexpr Color64 Premultiply() const
        {
            return Color64(Modulate(m_r, m_a), Modulate(m_g, m_a), Modulate(m_b, m_a), m_a);
        }

        //! @name Arithmetic.
        //! Integer color arithmetic is not float color arithmetic: addition and subtraction
        //! **saturate** rather than wrap, and color * color modulates, i.e. (a * b) / MaxChannel.
        //! @{
        constexpr Color64 operator+(const Color64& rhs) const
        {
            return Color64(AddSat(m_r, rhs.m_r), AddSat(m_g, rhs.m_g), AddSat(m_b, rhs.m_b), AddSat(m_a, rhs.m_a));
        }

        constexpr Color64 operator-(const Color64& rhs) const
        {
            return Color64(SubSat(m_r, rhs.m_r), SubSat(m_g, rhs.m_g), SubSat(m_b, rhs.m_b), SubSat(m_a, rhs.m_a));
        }

        constexpr Color64 operator*(const Color64& rhs) const
        {
            return Color64(
                Modulate(m_r, rhs.m_r), Modulate(m_g, rhs.m_g), Modulate(m_b, rhs.m_b), Modulate(m_a, rhs.m_a));
        }

        constexpr Color64 operator*(float scale) const
        {
            return Color64(ScaleChannel(m_r, scale), ScaleChannel(m_g, scale), ScaleChannel(m_b, scale),
                ScaleChannel(m_a, scale));
        }

        constexpr Color64 operator/(float divisor) const { return (*this) * (1.0f / divisor); }

        constexpr Color64& operator+=(const Color64& rhs) { return *this = *this + rhs; }
        constexpr Color64& operator-=(const Color64& rhs) { return *this = *this - rhs; }
        constexpr Color64& operator*=(const Color64& rhs) { return *this = *this * rhs; }
        constexpr Color64& operator*=(float scale) { return *this = *this * scale; }
        constexpr Color64& operator/=(float divisor) { return *this = *this / divisor; }
        //! @}

        constexpr bool operator==(const Color64& rhs) const
        {
            return m_r == rhs.m_r && m_g == rhs.m_g && m_b == rhs.m_b && m_a == rhs.m_a;
        }

        constexpr bool operator!=(const Color64& rhs) const { return !(*this == rhs); }

        //! @name Ordering, so a color can be used as an associative container key.
        //! The order is stable and total but arbitrary -- it is not perceptual.
        //! @{
        constexpr bool operator<(const Color64& rhs) const { return AsU64Rgba() < rhs.AsU64Rgba(); }
        constexpr bool operator>(const Color64& rhs) const { return rhs < *this; }
        constexpr bool operator<=(const Color64& rhs) const { return !(rhs < *this); }
        constexpr bool operator>=(const Color64& rhs) const { return !(*this < rhs); }
        //! @}

        union
        {
            //! Always the active member. See the active union member rule above.
            struct
            {
                u16 m_r{ 0 };
                u16 m_g{ 0 };
                u16 m_b{ 0 };
                u16 m_a{ MaxChannel };
            };
            //! Runtime-only views. Reading either in a constant expression is a compile error.
            u16 m_channels[4];
            u64 m_packed;
        };

    private:
        //! Bit replication, so 0xFF maps to 0xFFFF exactly and the round trip through >> 8 is lossless.
        static constexpr u16 FromU8(u8 v) { return static_cast<u16>(v * 257); }

        static constexpr float ToFloat(u16 v) { return static_cast<float>(v) * (1.0f / 65535.0f); }

        //! Clamps to [0, 1] with defined behaviour for NaN.
        //!
        //! AZ::GetClamp cannot be used here: it tests `value < min` then `value > max`, both of which
        //! are false for NaN, so it returns NaN unchanged -- and static_cast<u16> of NaN is undefined
        //! behaviour. Ordering the tests so that an unordered input falls through to 0 makes a NaN
        //! channel deterministically transparent/black instead.
        static constexpr float ClampUnitInterval(float v)
        {
            return (v > 0.0f) ? ((v < 1.0f) ? v : 1.0f) : 0.0f;
        }

        static constexpr u16 FromFloat(float v)
        {
            return static_cast<u16>(ClampUnitInterval(v) * 65535.0f + 0.5f);
        }

        static constexpr u64 Pack(u16 w3, u16 w2, u16 w1, u16 w0)
        {
            return (static_cast<u64>(w3) << 48) | (static_cast<u64>(w2) << 32) | (static_cast<u64>(w1) << 16) |
                static_cast<u64>(w0);
        }

        static constexpr u16 Word0(u64 v) { return static_cast<u16>(v & 0xFFFF); }
        static constexpr u16 Word1(u64 v) { return static_cast<u16>((v >> 16) & 0xFFFF); }
        static constexpr u16 Word2(u64 v) { return static_cast<u16>((v >> 32) & 0xFFFF); }
        static constexpr u16 Word3(u64 v) { return static_cast<u16>((v >> 48) & 0xFFFF); }

        static constexpr u16 AddSat(u16 a, u16 b)
        {
            const u32 sum = static_cast<u32>(a) + static_cast<u32>(b);
            return static_cast<u16>(sum > MaxChannel ? MaxChannel : sum);
        }

        static constexpr u16 SubSat(u16 a, u16 b) { return static_cast<u16>(a > b ? a - b : 0); }

        //! Exact normalized multiply: round(a * b / 65535).
        static constexpr u16 Modulate(u16 a, u16 b)
        {
            return static_cast<u16>((static_cast<u32>(a) * static_cast<u32>(b) + 32767u) / 65535u);
        }

        static constexpr u16 ScaleChannel(u16 v, float scale)
        {
            return FromFloat(ToFloat(v) * scale);
        }

        static constexpr u16 LerpChannel(u16 a, u16 b, float t)
        {
            return static_cast<u16>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t + 0.5f);
        }

        static constexpr u16 OverChannel(u16 srcChannel, u32 srcAlpha, u16 dstChannel, u32 invAlpha)
        {
            return AddSat(Modulate(srcChannel, static_cast<u16>(srcAlpha)),
                Modulate(dstChannel, static_cast<u16>(invAlpha)));
        }

        static constexpr void WriteHex16(char* buffer, size_t offset, u16 value)
        {
            constexpr char digits[] = "0123456789ABCDEF";
            buffer[offset + 0] = digits[(value >> 12) & 0xF];
            buffer[offset + 1] = digits[(value >> 8) & 0xF];
            buffer[offset + 2] = digits[(value >> 4) & 0xF];
            buffer[offset + 3] = digits[value & 0xF];
        }
    };

    static_assert(sizeof(Color64) == 8, "Color64 must be exactly 8 bytes");
    static_assert(alignof(Color64) == 8, "Color64 must be 8 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<Color64>, "Color64 must be trivially copyable");
} // namespace AZ

namespace AZStd
{
    template<>
    struct hash<AZ::Color64>
    {
        using result_type = AZStd::size_t;
        constexpr result_type operator()(const AZ::Color64& value) const
        {
            // Computed from the components rather than m_packed so this is usable at compile time.
            return static_cast<result_type>(value.AsU64Rgba());
        }
    };
} // namespace AZStd
