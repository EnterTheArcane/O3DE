/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/ColorHalf.h>

#include <AzCore/Math/Colors.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AZTestShared/Math/MathTestHelpers.h>

#include <limits>

using namespace AZ;

namespace UnitTest
{
    namespace
    {
        constexpr float g_colorHalfTestInfinity = AZStd::numeric_limits<float>::infinity();
    } // namespace

    TEST(MATH_ColorHalf, Layout)
    {
        static_assert(sizeof(ColorHalf) == 8);
        static_assert(alignof(ColorHalf) == 8);
        static_assert(AZStd::is_trivially_copyable_v<ColorHalf>);
        static_assert(offsetof(ColorHalf, m_r) == 0);
        static_assert(offsetof(ColorHalf, m_g) == 2);
        static_assert(offsetof(ColorHalf, m_b) == 4);
        static_assert(offsetof(ColorHalf, m_a) == 6);
    }

    TEST(MATH_ColorHalf, ConstexprConversionFromPalette)
    {
        constexpr ColorHalf white = Colors::White;
        static_assert(white.GetR() == 1.0f && white.GetA() == 1.0f);
        static_assert(white == ColorHalf::CreateOne());
        static_assert(white.IsOpaque() && white.IsFinite());
        static_assert(ColorHalf{}.GetA() == 1.0f, "default construction is opaque black");
        static_assert(ColorHalf::CreateZero().IsTransparent());
        static_assert(white.AsU64Rgba() == 0x3C003C003C003C00ull, "0x3C00 is binary16 1.0");
    }

    //! The reason this type exists rather than reusing Color64: channels are not normalized, so
    //! values outside [0, 1] survive and the arithmetic must not saturate the way the UNORM types do.
    TEST(MATH_ColorHalf, IsHighDynamicRange)
    {
        constexpr ColorHalf hdr(4.0f, 2.5f, 0.5f, 1.0f);
        static_assert(hdr.GetR() == 4.0f && hdr.GetG() == 2.5f);
        static_assert((hdr + hdr).GetR() == 8.0f, "addition must not saturate at 1.0");
        static_assert((hdr * 2.0f).GetR() == 8.0f);
        static_assert((hdr * hdr).GetR() == 16.0f, "multiplication scales, it does not modulate");
        static_assert((-hdr).GetR() == -4.0f, "negative channels are representable");
        static_assert((hdr - hdr).GetR() == 0.0f);
        static_assert(hdr.GetMaxComponent() == 4.0f && hdr.GetMinComponent() == 0.5f);
    }

    //! Overflowing binary16 produces an infinity rather than wrapping or clamping, and IsFinite is
    //! the way callers detect it.
    TEST(MATH_ColorHalf, OverflowProducesInfinity)
    {
        EXPECT_FALSE(ColorHalf(1.0e30f, 0.0f, 0.0f, 1.0f).IsFinite());
        EXPECT_TRUE(ColorHalf(65504.0f, 0.0f, 0.0f, 1.0f).IsFinite()) << "the largest finite binary16 value";
        EXPECT_FALSE(ColorHalf(g_colorHalfTestInfinity, 0.0f, 0.0f, 1.0f).IsFinite());
        EXPECT_TRUE(ColorHalf::CreateOne().IsFinite());

        // 65504 * 2 overflows the type, so the product is infinite rather than saturating.
        EXPECT_FALSE((ColorHalf(65504.0f, 0.0f, 0.0f, 1.0f) * 2.0f).IsFinite());
    }

    //! Deliberately the opposite of Color32/Color64::Lerp, which clamp. Extrapolation is useful for
    //! HDR, so the asymmetry is intentional and worth pinning.
    TEST(MATH_ColorHalf, LerpExtrapolates)
    {
        constexpr ColorHalf white = ColorHalf::CreateOne();
        static_assert(white.Lerp(ColorHalf::CreateZero(), 0.0f) == white);
        static_assert(white.Lerp(ColorHalf::CreateZero(), 1.0f) == ColorHalf::CreateZero());
        static_assert(white.Lerp(ColorHalf::CreateZero(), 0.5f).GetR() == 0.5f);
        static_assert(white.Lerp(ColorHalf::CreateZero(), 2.0f).GetR() == -1.0f, "t is NOT clamped");
        static_assert(white.Lerp(ColorHalf::CreateZero(), -1.0f).GetR() == 2.0f, "t is NOT clamped");
    }

    //! Values that binary16 can represent exactly must survive untouched; values it cannot must land
    //! within one ulp. This is the precision contract that makes the ColorSwatch conversion implicit
    //! even though it is not bit-exact.
    TEST(MATH_ColorHalf, PrecisionContract)
    {
        // Negative powers of two and their sums are exact in binary16.
        const ColorHalf exact{ Color(0.25f, 0.5f, 0.75f, 1.0f) };
        EXPECT_FLOAT_EQ(exact.GetR(), 0.25f);
        EXPECT_FLOAT_EQ(exact.GetG(), 0.5f);
        EXPECT_FLOAT_EQ(exact.GetB(), 0.75f);
        EXPECT_FLOAT_EQ(exact.GetA(), 1.0f);

        // 1/255 has no exact binary16 representation, but 255/255 does.
        const ColorHalf inexact{ Color32(1, 0, 0, 255) };
        EXPECT_NEAR(inexact.GetR(), 1.0f / 255.0f, 1.0e-5f);
        EXPECT_FLOAT_EQ(inexact.GetA(), 1.0f) << "255/255 is exactly 1.0 and must stay exact";

        // Every named color's alpha is 255, which must round trip exactly through the palette path.
        for (const ColorSwatch swatch : { Colors::White, Colors::Navy, Colors::Gold, Colors::DarkSlateGrey })
        {
            const ColorHalf converted = swatch;
            EXPECT_FLOAT_EQ(converted.GetA(), 1.0f);
            EXPECT_NEAR(converted.GetR(), swatch.GetR(), 1.0e-3f);
        }
    }

    //! HDR sits outside the Color32 -> Color64 -> Color widening chain: only the direction that is
    //! genuinely lossless (binary16 -> float) is implicit.
    TEST(MATH_ColorHalf, ConversionPolicy)
    {
        static_assert(AZStd::is_convertible_v<ColorHalf, Color>);
        static_assert(AZStd::is_convertible_v<ColorSwatch, ColorHalf>);

        static_assert(!AZStd::is_convertible_v<Color, ColorHalf>);
        static_assert(!AZStd::is_convertible_v<Color32, ColorHalf>, "1/255 is not exact in binary16");
        static_assert(!AZStd::is_convertible_v<Color64, ColorHalf>);
        static_assert(AZStd::is_constructible_v<ColorHalf, Color>);
        static_assert(AZStd::is_constructible_v<ColorHalf, Color32>);
        static_assert(AZStd::is_constructible_v<ColorHalf, Color64>);
    }

    TEST(MATH_ColorHalf, EqualityIsBitwise)
    {
        // Inherited from Half, and surprising enough in a color type to be worth pinning.
        constexpr ColorHalf positiveZero = ColorHalf::CreateZero();
        const ColorHalf negativeZero(-0.0f, -0.0f, -0.0f, -0.0f);
        EXPECT_TRUE(positiveZero != negativeZero) << "bitwise, so +0 does not equal -0";
        EXPECT_TRUE(positiveZero.IsClose(negativeZero)) << "...but IsClose compares numerically";
    }

    TEST(MATH_ColorHalf, Accessors)
    {
        constexpr ColorHalf white = Colors::White;
        static_assert(white.WithAlpha(0.0f).IsTransparent());
        static_assert(white.Inverted().GetR() == 0.0f);
        static_assert(white.Inverted().GetA() == 1.0f, "alpha is preserved");
        static_assert(ColorHalf::FromBits(0x3C00, 0x3C00, 0x3C00, 0x3C00) == white);
        static_assert(white.GetRHalf() == Half::CreateOne());
        static_assert(white[1] == 1.0f);
        static_assert(AZStd::hash<ColorHalf>{}(white) == 0x3C003C003C003C00ull);

        constexpr ColorHalf premultiplied = ColorHalf(1.0f, 1.0f, 1.0f, 0.5f).Premultiply();
        static_assert(premultiplied.GetR() == 0.5f);
        static_assert(premultiplied.GetA() == 0.5f, "alpha itself is not scaled");

        EXPECT_NEAR(white.GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_TRUE(white.IsClose(ColorHalf(1.0f, 1.0f, 1.0f, 1.0f)));
        EXPECT_THAT(white.AsVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));

        ColorHalf mutableHalf;
        mutableHalf.SetR(0.5f);
        mutableHalf.SetElement(1, 0.25f);
        EXPECT_FLOAT_EQ(mutableHalf.GetR(), 0.5f);
        EXPECT_FLOAT_EQ(mutableHalf.GetG(), 0.25f);
    }

    //! These conversions cross from the packed integer types into the SIMD backed ones. They only
    //! became constant expressions once Vector3/Vector4/Color did, so they are the assertions that
    //! break first if that ever regresses.
    TEST(MATH_ColorHalf, ConvertsToVectorTypesInConstantExpressions)
    {
        static_assert(ColorHalf(1.0f, 0.0f, 0.0f).AsVector4() == Vector4(1.0f, 0.0f, 0.0f, 1.0f));
        static_assert(ColorHalf(1.0f, 0.0f, 0.0f).AsVector3() == Vector3(1.0f, 0.0f, 0.0f));

        // Values exactly representable in binary16 survive the round trip; 1/255 would not, which is
        // why the Color32 -> ColorHalf direction stays documented as approximate.
        constexpr Color widened = ColorHalf(1.0f, 0.5f, 0.25f, 1.0f);
        static_assert(widened.GetG() == 0.5f);
        static_assert(ColorHalf(widened).GetB() == 0.25f);
    }

} // namespace UnitTest
