/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>

namespace AZ
{
    class Vector3;
    class Vector4;
} // namespace AZ

namespace AZ::Internal
{
    //! Storage for a named color constant: four unsigned bytes, no color space association.
    //!
    //! This exists only so the palette below can be written once and consumed as any AzCore color type at compile time.
    //! It is not meant to be used directly, which is why it lives in AZ::Internal
    //! and carries nothing beyond what a color type needs to construct itself from.
    //!
    //! Every conversion out of it widens, and is therefore exact and implicit.
    //! That holds only because this is the narrowest color representation in AzCore,
    //! so nothing here should grow wider than a byte per channel.
    struct NamedColor final
    {
        constexpr NamedColor(u8 r, u8 g, u8 b, u8 a)
            : m_r(r)
            , m_g(g)
            , m_b(b)
            , m_a(a)
        {
        }

        constexpr NamedColor(u8 r, u8 g, u8 b)
            : NamedColor(r, g, b, 255)
        {
        }

        //! @name Transitional API
        //! The named constants used to be AZ::Color, so member calls like AZ::Colors::White.ToU32() compiled.
        //! These keep that code building while call sites migrate.
        //! @{
        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call GetR() on it")
        constexpr float GetR() const
        {
            return static_cast<float>(m_r) * (1.0f / 255.0f);
        }

        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call GetG() on it")
        constexpr float GetG() const
        {
            return static_cast<float>(m_g) * (1.0f / 255.0f);
        }

        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call GetB() on it")
        constexpr float GetB() const
        {
            return static_cast<float>(m_b) * (1.0f / 255.0f);
        }

        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call GetA() on it")
        constexpr float GetA() const
        {
            return static_cast<float>(m_a) * (1.0f / 255.0f);
        }

        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call ToU32() on it, or use AZ::Color32")
        constexpr u32 ToU32() const
        {
            return (static_cast<u32>(m_a) << 24)
                | (static_cast<u32>(m_b) << 16)
                | (static_cast<u32>(m_g) << 8)
                | static_cast<u32>(m_r);
        }

        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call GetAsVector3() on it")
        Vector3 GetAsVector3() const; // defined in Color.h

        // AZ_DEPRECATED_MESSAGE("AZ::Colors constants are no longer AZ::Color. Construct AZ::Color(AZ::Colors::X) and call GetAsVector4() on it")
        Vector4 GetAsVector4() const; // defined in Color.h
        //! @}

        u8 m_r;
        u8 m_g;
        u8 m_b;
        u8 m_a;
    };
} // namespace AZ::Internal

namespace AZ
{
    //! Named colors, from the CSS specification:
    //! https://www.w3.org/TR/2011/REC-SVG11-20110816/types.html#ColorKeywords
    //!
    //! These are AZ::Internal::NamedColor rather than AZ::Color so that they are usable in constant
    //! expressions and convert implicitly, and exactly, to every AzCore color type:
    //!
    //!     constexpr AZ::Color64 red = AZ::Colors::Red;
    //!     constexpr AZ::Color32 white = AZ::Colors::White;
    //!     AZ::Color blue = AZ::Colors::Blue;
    namespace Colors
    {
#define AZ_NAMED_COLOR(_name, _r, _g, _b) inline constexpr Internal::NamedColor _name{ _r, _g, _b }
        AZ_NAMED_COLOR(AliceBlue, 240, 248, 255);
        AZ_NAMED_COLOR(AntiqueWhite, 250, 235, 215);
        AZ_NAMED_COLOR(Aqua, 0, 255, 255);
        AZ_NAMED_COLOR(Aquamarine, 127, 255, 212);
        AZ_NAMED_COLOR(Azure, 240, 255, 255);
        AZ_NAMED_COLOR(Beige, 245, 245, 220);
        AZ_NAMED_COLOR(Bisque, 255, 228, 196);
        AZ_NAMED_COLOR(Black, 0, 0, 0);
        AZ_NAMED_COLOR(BlanchedAlmond, 255, 235, 205);
        AZ_NAMED_COLOR(Blue, 0, 0, 255);
        AZ_NAMED_COLOR(BlueViolet, 138, 43, 226);
        AZ_NAMED_COLOR(Brown, 165, 42, 42);
        AZ_NAMED_COLOR(BurlyWood, 222, 184, 135);
        AZ_NAMED_COLOR(CadetBlue, 95, 158, 160);
        AZ_NAMED_COLOR(Chartreuse, 127, 255, 0);
        AZ_NAMED_COLOR(Chocolate, 210, 105, 30);
        AZ_NAMED_COLOR(Coral, 255, 127, 80);
        AZ_NAMED_COLOR(CornflowerBlue, 100, 149, 237);
        AZ_NAMED_COLOR(Cornsilk, 255, 248, 220);
        AZ_NAMED_COLOR(Crimson, 220, 20, 60);
        AZ_NAMED_COLOR(Cyan, 0, 255, 255);
        AZ_NAMED_COLOR(DarkBlue, 0, 0, 139);
        AZ_NAMED_COLOR(DarkCyan, 0, 139, 139);
        AZ_NAMED_COLOR(DarkGoldenrod, 184, 134, 11);
        AZ_NAMED_COLOR(DarkGray, 169, 169, 169);
        AZ_NAMED_COLOR(DarkGreen, 0, 100, 0);
        AZ_NAMED_COLOR(DarkGrey, 169, 169, 169);
        AZ_NAMED_COLOR(DarkKhaki, 189, 183, 107);
        AZ_NAMED_COLOR(DarkMagenta, 139, 0, 139);
        AZ_NAMED_COLOR(DarkOliveGreen, 85, 107, 47);
        AZ_NAMED_COLOR(DarkOrange, 255, 140, 0);
        AZ_NAMED_COLOR(DarkOrchid, 153, 50, 204);
        AZ_NAMED_COLOR(DarkRed, 139, 0, 0);
        AZ_NAMED_COLOR(DarkSalmon, 233, 150, 122);
        AZ_NAMED_COLOR(DarkSeaGreen, 143, 188, 143);
        AZ_NAMED_COLOR(DarkSlateBlue, 72, 61, 139);
        AZ_NAMED_COLOR(DarkSlateGray, 47, 79, 79);
        AZ_NAMED_COLOR(DarkSlateGrey, 47, 79, 79);
        AZ_NAMED_COLOR(DarkTurquoise, 0, 206, 209);
        AZ_NAMED_COLOR(DarkViolet, 148, 0, 211);
        AZ_NAMED_COLOR(DeepPink, 255, 20, 147);
        AZ_NAMED_COLOR(DeepSkyBlue, 0, 191, 255);
        AZ_NAMED_COLOR(DimGray, 105, 105, 105);
        AZ_NAMED_COLOR(DimGrey, 105, 105, 105);
        AZ_NAMED_COLOR(DodgerBlue, 30, 144, 255);
        AZ_NAMED_COLOR(FireBrick, 178, 34, 34);
        AZ_NAMED_COLOR(FloralWhite, 255, 250, 240);
        AZ_NAMED_COLOR(ForestGreen, 34, 139, 34);
        AZ_NAMED_COLOR(Fuchsia, 255, 0, 255);
        AZ_NAMED_COLOR(Gainsboro, 220, 220, 220);
        AZ_NAMED_COLOR(GhostWhite, 248, 248, 255);
        AZ_NAMED_COLOR(Gold, 255, 215, 0);
        AZ_NAMED_COLOR(Goldenrod, 218, 165, 32);
        AZ_NAMED_COLOR(Gray, 128, 128, 128);
        AZ_NAMED_COLOR(Green, 0, 128, 0);
        AZ_NAMED_COLOR(GreenYellow, 173, 255, 47);
        AZ_NAMED_COLOR(Grey, 128, 128, 128);
        AZ_NAMED_COLOR(Honeydew, 240, 255, 240);
        AZ_NAMED_COLOR(HotPink, 255, 105, 180);
        AZ_NAMED_COLOR(IndianRed, 205, 92, 92);
        AZ_NAMED_COLOR(Indigo, 75, 0, 130);
        AZ_NAMED_COLOR(Ivory, 255, 255, 240);
        AZ_NAMED_COLOR(Khaki, 240, 230, 140);
        AZ_NAMED_COLOR(Lavender, 230, 230, 250);
        AZ_NAMED_COLOR(LavenderBlush, 255, 240, 245);
        AZ_NAMED_COLOR(LawnGreen, 124, 252, 0);
        AZ_NAMED_COLOR(LemonChiffon, 255, 250, 205);
        AZ_NAMED_COLOR(LightBlue, 173, 216, 230);
        AZ_NAMED_COLOR(LightCoral, 240, 128, 128);
        AZ_NAMED_COLOR(LightCyan, 224, 255, 255);
        AZ_NAMED_COLOR(LightGoldenrodYellow, 250, 250, 210);
        AZ_NAMED_COLOR(LightGray, 211, 211, 211);
        AZ_NAMED_COLOR(LightGreen, 144, 238, 144);
        AZ_NAMED_COLOR(LightGrey, 211, 211, 211);
        AZ_NAMED_COLOR(LightPink, 255, 182, 193);
        AZ_NAMED_COLOR(LightSalmon, 255, 160, 122);
        AZ_NAMED_COLOR(LightSeaGreen, 32, 178, 170);
        AZ_NAMED_COLOR(LightSkyBlue, 135, 206, 250);
        AZ_NAMED_COLOR(LightSlateGray, 119, 136, 153);
        AZ_NAMED_COLOR(LightSlateGrey, 119, 136, 153);
        AZ_NAMED_COLOR(LightSteelBlue, 176, 196, 222);
        AZ_NAMED_COLOR(LightYellow, 255, 255, 224);
        AZ_NAMED_COLOR(Lime, 0, 255, 0);
        AZ_NAMED_COLOR(LimeGreen, 50, 205, 50);
        AZ_NAMED_COLOR(Linen, 250, 240, 230);
        AZ_NAMED_COLOR(Magenta, 255, 0, 255);
        AZ_NAMED_COLOR(Maroon, 128, 0, 0);
        AZ_NAMED_COLOR(MediumAquamarine, 102, 205, 170);
        AZ_NAMED_COLOR(MediumBlue, 0, 0, 205);
        AZ_NAMED_COLOR(MediumOrchid, 186, 85, 211);
        AZ_NAMED_COLOR(MediumPurple, 147, 112, 219);
        AZ_NAMED_COLOR(MediumSeaGreen, 60, 179, 113);
        AZ_NAMED_COLOR(MediumSlateBlue, 123, 104, 238);
        AZ_NAMED_COLOR(MediumSpringGreen, 0, 250, 154);
        AZ_NAMED_COLOR(MediumTurquoise, 72, 209, 204);
        AZ_NAMED_COLOR(MediumVioletRed, 199, 21, 133);
        AZ_NAMED_COLOR(MidnightBlue, 25, 25, 112);
        AZ_NAMED_COLOR(MintCream, 245, 255, 250);
        AZ_NAMED_COLOR(MistyRose, 255, 228, 225);
        AZ_NAMED_COLOR(Moccasin, 255, 228, 181);
        AZ_NAMED_COLOR(NavajoWhite, 255, 222, 173);
        AZ_NAMED_COLOR(Navy, 0, 0, 128);
        AZ_NAMED_COLOR(OldLace, 253, 245, 230);
        AZ_NAMED_COLOR(Olive, 128, 128, 0);
        AZ_NAMED_COLOR(OliveDrab, 107, 142, 35);
        AZ_NAMED_COLOR(Orange, 255, 165, 0);
        AZ_NAMED_COLOR(OrangeRed, 255, 69, 0);
        AZ_NAMED_COLOR(Orchid, 218, 112, 214);
        AZ_NAMED_COLOR(PaleGoldenrod, 238, 232, 170);
        AZ_NAMED_COLOR(PaleGreen, 152, 251, 152);
        AZ_NAMED_COLOR(PaleTurquoise, 175, 238, 238);
        AZ_NAMED_COLOR(PaleVioletRed, 219, 112, 147);
        AZ_NAMED_COLOR(PapayaWhip, 255, 239, 213);
        AZ_NAMED_COLOR(PeachPuff, 255, 218, 185);
        AZ_NAMED_COLOR(Peru, 205, 133, 63);
        AZ_NAMED_COLOR(Pink, 255, 192, 203);
        AZ_NAMED_COLOR(Plum, 221, 160, 221);
        AZ_NAMED_COLOR(PowderBlue, 176, 224, 230);
        AZ_NAMED_COLOR(Purple, 128, 0, 128);
        AZ_NAMED_COLOR(RebeccaPurple, 102, 51, 153);
        AZ_NAMED_COLOR(Red, 255, 0, 0);
        AZ_NAMED_COLOR(RosyBrown, 188, 143, 143);
        AZ_NAMED_COLOR(RoyalBlue, 65, 105, 225);
        AZ_NAMED_COLOR(SaddleBrown, 139, 69, 19);
        AZ_NAMED_COLOR(Salmon, 250, 128, 114);
        AZ_NAMED_COLOR(SandyBrown, 244, 164, 96);
        AZ_NAMED_COLOR(SeaGreen, 46, 139, 87);
        AZ_NAMED_COLOR(Seashell, 255, 245, 238);
        AZ_NAMED_COLOR(Sienna, 160, 82, 45);
        AZ_NAMED_COLOR(Silver, 192, 192, 192);
        AZ_NAMED_COLOR(SkyBlue, 135, 206, 235);
        AZ_NAMED_COLOR(SlateBlue, 106, 90, 205);
        AZ_NAMED_COLOR(SlateGray, 112, 128, 144);
        AZ_NAMED_COLOR(SlateGrey, 112, 128, 144);
        AZ_NAMED_COLOR(Snow, 255, 250, 250);
        AZ_NAMED_COLOR(SpringGreen, 0, 255, 127);
        AZ_NAMED_COLOR(SteelBlue, 70, 130, 180);
        AZ_NAMED_COLOR(Tan, 210, 180, 140);
        AZ_NAMED_COLOR(Teal, 0, 128, 128);
        AZ_NAMED_COLOR(Thistle, 216, 191, 216);
        AZ_NAMED_COLOR(Tomato, 255, 99, 71);
        AZ_NAMED_COLOR(Turquoise, 64, 224, 208);
        AZ_NAMED_COLOR(Violet, 238, 130, 238);
        AZ_NAMED_COLOR(Wheat, 245, 222, 179);
        AZ_NAMED_COLOR(White, 255, 255, 255);
        AZ_NAMED_COLOR(WhiteSmoke, 245, 245, 245);
        AZ_NAMED_COLOR(Yellow, 255, 255, 0);
        AZ_NAMED_COLOR(YellowGreen, 154, 205, 50);
#undef AZ_NAMED_COLOR
    } // namespace Colors
} // namespace AZ
