/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/ColorSwatch.h>

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Colors.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AZTestShared/Math/MathTestHelpers.h>

using namespace AZ;

namespace UnitTest
{
    TEST(MATH_ColorSwatch, Layout)
    {
        static_assert(sizeof(ColorSwatch) == 4);
        static_assert(alignof(ColorSwatch) == 4);
        static_assert(AZStd::is_trivially_copyable_v<ColorSwatch>);
        static_assert(offsetof(ColorSwatch, m_r) == 0);
        static_assert(offsetof(ColorSwatch, m_g) == 1);
        static_assert(offsetof(ColorSwatch, m_b) == 2);
        static_assert(offsetof(ColorSwatch, m_a) == 3);

        // The packed and array views must agree with the named components. This pins the
        // little-endian layout that AsU32*() depends on being folded back into a single load.
        const ColorSwatch swatch(0x11, 0x22, 0x33, 0x44);
        EXPECT_EQ(swatch.m_channels[0], 0x11);
        EXPECT_EQ(swatch.m_channels[3], 0x44);
        EXPECT_EQ(swatch.m_packed, 0x44332211u);
    }

    TEST(MATH_ColorSwatch, Construction)
    {
        static_assert(ColorSwatch(1, 2, 3).GetA8() == 255, "alpha defaults to opaque");
        static_assert(ColorSwatch{}.GetA8() == 255, "default construction is opaque black");
        static_assert(ColorSwatch{}.GetR8() == 0);
        static_assert(ColorSwatch::CreateZero().IsTransparent());
        static_assert(ColorSwatch::CreateOne() == ColorSwatch(255, 255, 255, 255));
        static_assert(ColorSwatch::CreateGray(128) == ColorSwatch(128, 128, 128, 255));
        static_assert(ColorSwatch::CreateFromRgba(10, 20, 30, 40).GetB8() == 30);
    }

    TEST(MATH_ColorSwatch, NormalizedAccessors)
    {
        static_assert(ColorSwatch(255, 0, 0, 255).GetR() == 1.0f, "255 must normalize to exactly 1.0");
        static_assert(ColorSwatch(0, 0, 0, 0).GetA() == 0.0f);
        EXPECT_NEAR(ColorSwatch(128, 0, 0).GetR(), 128.0f / 255.0f, Constants::Tolerance);
    }

    //! Byte order is the single most error-prone thing about a packed color, and the reason these
    //! are named rather than left to hand-rolled shifts at call sites. Use an asymmetric value so a
    //! transposed pair cannot pass by coincidence, and assert against literals rather than against
    //! another permutation.
    TEST(MATH_ColorSwatch, ByteOrderPacking)
    {
        constexpr ColorSwatch swatch(0x11, 0x22, 0x33, 0x44); // r, g, b, a
        static_assert(swatch.AsU32Rgba() == 0x11223344u);
        static_assert(swatch.AsU32Argb() == 0x44112233u);
        static_assert(swatch.AsU32Abgr() == 0x44332211u);
        static_assert(swatch.AsU32Bgra() == 0x33221144u);
        static_assert(swatch.AsU32() == swatch.AsU32Abgr(), "AsU32 is the AZ::Color::ToU32 spelling");

        static_assert(ColorSwatch::FromU32Rgba(0x11223344u) == swatch);
        static_assert(ColorSwatch::FromU32Argb(0x44112233u) == swatch);
        static_assert(ColorSwatch::FromU32Abgr(0x44332211u) == swatch);
        static_assert(ColorSwatch::FromU32Bgra(0x33221144u) == swatch);
        static_assert(ColorSwatch::FromU32(0x44332211u) == swatch);
    }

    //! A swatch must pack identically to the AZ::Color it converts to, or any existing ToU32() call
    //! site that migrates to the palette would silently reorder its channels.
    TEST(MATH_ColorSwatch, PackingMatchesAzColor)
    {
        for (const ColorSwatch swatch :
            { Colors::White, Colors::Black, Colors::Red, Colors::Navy, Colors::Orange, Colors::DarkSlateGrey })
        {
            const Color asColor = swatch;
            EXPECT_EQ(swatch.AsU32(), asColor.ToU32());
        }
    }

    TEST(MATH_ColorSwatch, HexParsing)
    {
        static_assert(ColorSwatch::FromHex("#FF0000") == ColorSwatch(255, 0, 0, 255));
        static_assert(ColorSwatch::FromHex("FF0000") == ColorSwatch(255, 0, 0, 255), "the leading # is optional");
        static_assert(ColorSwatch::FromHex("#11223344") == ColorSwatch(0x11, 0x22, 0x33, 0x44), "8 digits set alpha");
        static_assert(ColorSwatch::FromHex("#aabbcc") == ColorSwatch(0xAA, 0xBB, 0xCC), "lower case digits");
        static_assert(ColorSwatch::FromHex("#AaBbCc") == ColorSwatch(0xAA, 0xBB, 0xCC), "mixed case digits");
        static_assert(ColorSwatch::FromHex("#FF0000").GetA8() == 255, "6 digits leave alpha opaque");

        // The 3 digit form replicates each nibble, so F becomes FF rather than F0. Getting this
        // wrong would make every short-form color noticeably darker.
        static_assert(ColorSwatch::FromHex("#F00") == ColorSwatch(255, 0, 0, 255));
        static_assert(ColorSwatch::FromHex("#08F") == ColorSwatch(0x00, 0x88, 0xFF, 255));
    }

    //! GetElement takes an index rather than a named channel, so the out-of-range behaviour is part
    //! of the contract: anything past b reads alpha rather than reading out of bounds.
    TEST(MATH_ColorSwatch, IndexedAccess)
    {
        constexpr ColorSwatch swatch(10, 20, 30, 40);
        static_assert(swatch.GetElement(0) == 10);
        static_assert(swatch.GetElement(1) == 20);
        static_assert(swatch.GetElement(2) == 30);
        static_assert(swatch.GetElement(3) == 40);
        static_assert(swatch.GetElement(99) == 40, "an out of range index saturates to alpha");
        static_assert(swatch[2] == 30);

        ColorSwatch mutableSwatch;
        mutableSwatch.SetR8(7);
        mutableSwatch.SetElement(1, 9);
        mutableSwatch.SetElement(99, 11);
        EXPECT_EQ(mutableSwatch.GetR8(), 7);
        EXPECT_EQ(mutableSwatch.GetG8(), 9);
        EXPECT_EQ(mutableSwatch.GetA8(), 11);
    }

    TEST(MATH_ColorSwatch, Helpers)
    {
        static_assert(Colors::White.Inverted() == ColorSwatch(0, 0, 0, 255), "alpha is preserved");
        static_assert(Colors::White.WithAlpha(0).IsTransparent());
        static_assert(Colors::White.IsOpaque());
        static_assert(AZStd::hash<ColorSwatch>{}(Colors::Navy) == Colors::Navy.AsU32Rgba());

        EXPECT_NEAR(Colors::White.GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_NEAR(Colors::Black.GetLuminance(), 0.0f, Constants::Tolerance);
        EXPECT_GT(Colors::Lime.GetLuminance(), Colors::Red.GetLuminance()) << "Rec. 709 weights green highest";
        EXPECT_GT(Colors::Red.GetLuminance(), Colors::Blue.GetLuminance()) << "...and blue lowest";
    }

    TEST(MATH_ColorSwatch, Comparison)
    {
        static_assert(Colors::Red != Colors::Blue);
        static_assert(Colors::Black < Colors::White);
        static_assert(Colors::White > Colors::Black);
        static_assert(Colors::White >= Colors::White && Colors::White <= Colors::White);
    }

    TEST(MATH_ColorSwatch, ConvertsToVectors)
    {
        EXPECT_THAT(Colors::White.AsVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
        EXPECT_THAT(Colors::Red.AsVector3(), IsClose(Vector3(1.0f, 0.0f, 0.0f)));
    }

    // ---------------------------------------------------------------------------------------------
    // The named palette in Colors.h
    // ---------------------------------------------------------------------------------------------

    //! The CSS specification defines these pairs as the same color. They are written out twice in
    //! the palette, so a typo in one spelling would go unnoticed without this.
    TEST(MATH_Colors, SpellingVariantsAreIdentical)
    {
        static_assert(Colors::Gray == Colors::Grey);
        static_assert(Colors::LightGray == Colors::LightGrey);
        static_assert(Colors::DarkGray == Colors::DarkGrey);
        static_assert(Colors::DimGray == Colors::DimGrey);
        static_assert(Colors::SlateGray == Colors::SlateGrey);
        static_assert(Colors::LightSlateGray == Colors::LightSlateGrey);
        static_assert(Colors::DarkSlateGray == Colors::DarkSlateGrey);

        // These are distinct CSS names that happen to share a value.
        static_assert(Colors::Aqua == Colors::Cyan);
        static_assert(Colors::Fuchsia == Colors::Magenta);
    }

    //! Spot check values against the CSS specification. Transcription errors in a 148 entry table
    //! are the realistic failure mode, and the greys are the easiest to get one digit wrong in.
    TEST(MATH_Colors, MatchesCssSpecification)
    {
        static_assert(Colors::White == ColorSwatch(255, 255, 255, 255));
        static_assert(Colors::Black == ColorSwatch(0, 0, 0, 255));
        static_assert(Colors::Red == ColorSwatch(255, 0, 0, 255));
        static_assert(Colors::Lime == ColorSwatch(0, 255, 0, 255));
        static_assert(Colors::Blue == ColorSwatch(0, 0, 255, 255));
        static_assert(Colors::Green == ColorSwatch(0, 128, 0, 255), "CSS Green is half intensity, not Lime");
        static_assert(Colors::Orange == ColorSwatch(255, 165, 0, 255));
        static_assert(Colors::RebeccaPurple == ColorSwatch(102, 51, 153, 255));
        static_assert(Colors::CornflowerBlue == ColorSwatch(100, 149, 237, 255));
        static_assert(Colors::DarkSlateGrey == ColorSwatch(47, 79, 79, 255));
    }

    //! Every entry is authored as an RGB triple, so alpha must come from the constructor default.
    //! A single stray 4-argument entry would otherwise produce a silently translucent constant.
    TEST(MATH_Colors, EveryNamedColorIsOpaque)
    {
        const ColorSwatch palette[] = { Colors::White, Colors::Silver, Colors::Gray, Colors::Black, Colors::Red,
            Colors::Maroon, Colors::Lime, Colors::Green, Colors::Blue, Colors::Navy, Colors::Yellow, Colors::Orange,
            Colors::Olive, Colors::Purple, Colors::Fuchsia, Colors::Teal, Colors::Aqua, Colors::IndianRed,
            Colors::Crimson, Colors::HotPink, Colors::DeepPink, Colors::Coral, Colors::Tomato, Colors::Gold,
            Colors::Lavender, Colors::Violet, Colors::Indigo, Colors::RebeccaPurple, Colors::SpringGreen,
            Colors::SeaGreen, Colors::DarkCyan, Colors::Turquoise, Colors::SteelBlue, Colors::SkyBlue,
            Colors::CornflowerBlue, Colors::MidnightBlue, Colors::Cornsilk, Colors::Wheat, Colors::SaddleBrown,
            Colors::Sienna, Colors::Snow, Colors::Ivory, Colors::Beige, Colors::Gainsboro, Colors::DimGray,
            Colors::SlateGray, Colors::DarkSlateGrey };

        for (const ColorSwatch& swatch : palette)
        {
            EXPECT_TRUE(swatch.IsOpaque()) << "a palette entry must default to opaque";
        }
    }

    //! The palette exists to be usable in constant expressions; if these ever became runtime
    //! initialized, the whole design would have regressed without any other test noticing.
    TEST(MATH_Colors, AreUsableInConstantExpressions)
    {
        static_assert(Colors::White.GetR8() == 255);
        constexpr ColorSwatch accent = Colors::Orange;
        static_assert(accent.GetG8() == 165);
        static_assert(accent.WithAlpha(128).GetA8() == 128);
        constexpr u32 packed = Colors::Navy.AsU32Rgba();
        static_assert(packed == 0x000080FFu);
    }

    //! These conversions cross from the packed integer types into the SIMD backed ones. They only
    //! became constant expressions once Vector3/Vector4/Color did, so they are the assertions that
    //! break first if that ever regresses.
    TEST(MATH_ColorSwatch, ConvertsToVectorTypesInConstantExpressions)
    {
        static_assert(ColorSwatch(255, 128, 0).AsVector4() == Vector4(1.0f, 128.0f / 255.0f, 0.0f, 1.0f));
        static_assert(ColorSwatch(255, 128, 0).AsVector3() == Vector3(1.0f, 128.0f / 255.0f, 0.0f));
        static_assert(Color(Colors::White) == Color(1.0f, 1.0f, 1.0f, 1.0f));
    }

} // namespace UnitTest
