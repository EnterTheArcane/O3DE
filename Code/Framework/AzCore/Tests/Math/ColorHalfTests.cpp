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

#include <AzCore/std/containers/array.h>

#include <cmath>
#include <cstring>
#include <limits>

using namespace AZ;

namespace UnitTest
{
    namespace
    {
        const float ColorHalfTestNaN = AZStd::numeric_limits<float>::quiet_NaN();
        constexpr float ColorHalfTestInfinity = AZStd::numeric_limits<float>::infinity();
    } // namespace

    TEST(MATH_ColorHalf, Layout)
    {
        static_assert(sizeof(ColorHalf) == 8);
        static_assert(alignof(ColorHalf) == 8);
        static_assert(AZStd::is_trivially_copyable_v<ColorHalf>);

        const ColorHalf color(1.0f, 2.0f, 3.0f, 4.0f);
        AZStd::array<u16, 4> channels{};
        memcpy(channels.data(), &color, sizeof(color));
        EXPECT_THAT(channels, ::testing::ElementsAre(Half(1.0f).GetBits(), Half(2.0f).GetBits(), Half(3.0f).GetBits(), Half(4.0f).GetBits()));
    }

    TEST(MATH_ColorHalf, ConstexprConversionFromPalette)
    {
        constexpr ColorHalf white = Colors::White;
        static_assert(white.GetR() == 1.0f && white.GetA() == 1.0f);
        static_assert(white == ColorHalf::One);
        static_assert(white.IsOpaque() && white.IsFinite());
        static_assert(ColorHalf{}.GetA() == 1.0f, "default construction is opaque black");
        static_assert(ColorHalf::Zero.IsTransparent());
        static_assert(white.GetRHalf().GetBits() == 0x3C00, "0x3C00 is binary16 1.0");
    }

    //! The reason this type exists rather than reusing Color64: channels are not normalized,
    //! so values outside [0, 1] survive and the arithmetic must not saturate the way the UNORM types do.
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

    //! Overflowing binary16 produces an infinity rather than wrapping or clamping,
    //! and IsFinite is the way callers detect it.
    TEST(MATH_ColorHalf, OverflowProducesInfinity)
    {
        EXPECT_FALSE(ColorHalf(1.0e30f, 0.0f, 0.0f, 1.0f).IsFinite());
        EXPECT_TRUE(ColorHalf(65504.0f, 0.0f, 0.0f, 1.0f).IsFinite()) << "the largest finite binary16 value";
        EXPECT_FALSE(ColorHalf(ColorHalfTestInfinity, 0.0f, 0.0f, 1.0f).IsFinite());
        EXPECT_TRUE(ColorHalf::One.IsFinite());

        // 65504 * 2 overflows the type, so the product is infinite rather than saturating.
        EXPECT_FALSE((ColorHalf(65504.0f, 0.0f, 0.0f, 1.0f) * 2.0f).IsFinite());
    }

    //! Deliberately the opposite of Color32/Color64::Lerp, which clamp.
    //! Extrapolation is useful for HDR, so the asymmetry is intentional and worth pinning.
    TEST(MATH_ColorHalf, LerpExtrapolates)
    {
        constexpr ColorHalf white = ColorHalf::One;
        static_assert(white.Lerp(ColorHalf::Zero, 0.0f) == white);
        static_assert(white.Lerp(ColorHalf::Zero, 1.0f) == ColorHalf::Zero);
        static_assert(white.Lerp(ColorHalf::Zero, 0.5f).GetR() == 0.5f);
        static_assert(white.Lerp(ColorHalf::Zero, 2.0f).GetR() == -1.0f, "t is NOT clamped");
        static_assert(white.Lerp(ColorHalf::Zero, -1.0f).GetR() == 2.0f, "t is NOT clamped");
    }

    //! Values that binary16 can represent exactly must survive untouched.
    //! Values it cannot must land within one ulp.
    //! This is the precision contract that makes the named constant conversion implicit even though it is not bit-exact.
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
        for (const Internal::NamedColor named : {Colors::White, Colors::Navy, Colors::Gold, Colors::DarkSlateGrey})
        {
            const ColorHalf converted = named;
            EXPECT_FLOAT_EQ(converted.GetA(), 1.0f);
            EXPECT_NEAR(converted.GetR(), static_cast<float>(named.m_r) / 255.0f, 1.0e-3f);
        }
    }

    //! HDR sits outside the Color32 -> Color64 -> Color widening chain:
    //! only the direction that is genuinely lossless (binary16 -> float) is implicit.
    TEST(MATH_ColorHalf, ConversionPolicy)
    {
        static_assert(AZStd::is_convertible_v<ColorHalf, Color>);
        static_assert(AZStd::is_convertible_v<Internal::NamedColor, ColorHalf>);

        static_assert(!AZStd::is_convertible_v<Color, ColorHalf>);
        static_assert(!AZStd::is_convertible_v<Color32, ColorHalf>, "1/255 is not exact in binary16");
        static_assert(!AZStd::is_convertible_v<Color64, ColorHalf>);
        static_assert(AZStd::is_constructible_v<ColorHalf, Color>);
        static_assert(AZStd::is_constructible_v<ColorHalf, Color32>);
        static_assert(AZStd::is_constructible_v<ColorHalf, Color64>);
    }

    TEST(MATH_ColorHalf, EqualityMatchesIeeeFloatSemantics)
    {
        constexpr ColorHalf positiveZero = ColorHalf::Zero;
        const ColorHalf negativeZero(-0.0f, -0.0f, -0.0f, -0.0f);
        EXPECT_TRUE(positiveZero == negativeZero) << "+0 equals -0, matching Half and AZ::Color";
        EXPECT_NE(positiveZero.GetRHalf().GetBits(), negativeZero.GetRHalf().GetBits())
            << "...even though their bit patterns differ";
        EXPECT_EQ(AZStd::hash<ColorHalf>{}(positiveZero), AZStd::hash<ColorHalf>{}(negativeZero))
            << "equal colors must hash equal";

        const ColorHalf withNaN(ColorHalfTestNaN, 0.0f, 0.0f, 1.0f);
        EXPECT_TRUE(withNaN != withNaN) << "a NaN channel makes a color unequal to itself";
    }

    //! The Color conversions may use 4-wide hardware conversion, so pin that every channel lands in
    //! its own lane and matches the scalar conversion exactly, including the special values.
    TEST(MATH_ColorHalf, ColorConversionMatchesScalarChannels)
    {
        const Color color(0.25f, 1.5f, -0.5f, 0.75f);
        const ColorHalf narrowed(color);
        EXPECT_EQ(narrowed.GetRHalf().GetBits(), Half(0.25f).GetBits());
        EXPECT_EQ(narrowed.GetGHalf().GetBits(), Half(1.5f).GetBits());
        EXPECT_EQ(narrowed.GetBHalf().GetBits(), Half(-0.5f).GetBits());
        EXPECT_EQ(narrowed.GetAHalf().GetBits(), Half(0.75f).GetBits());

        const Color widened = narrowed;
        EXPECT_EQ(widened.GetR(), 0.25f);
        EXPECT_EQ(widened.GetG(), 1.5f);
        EXPECT_EQ(widened.GetB(), -0.5f);
        EXPECT_EQ(widened.GetA(), 0.75f);

        const ColorHalf special{Color(std::ldexp(1.0f, -24), ColorHalfTestInfinity, 65504.0f, 0.0f)};
        EXPECT_EQ(special.GetRHalf().GetBits(), 0x0001) << "the smallest subnormal survives the wide path";
        EXPECT_TRUE(special.GetGHalf().IsInfinity());
        EXPECT_EQ(special.GetBHalf().GetBits(), 0x7BFF);
    }

    TEST(MATH_ColorHalf, Accessors)
    {
        constexpr ColorHalf white = Colors::White;
        static_assert(white.WithAlpha(0.0f).IsTransparent());
        static_assert(ColorHalf::FromBits(0x3C00, 0x3C00, 0x3C00, 0x3C00) == white);
        static_assert(white.GetRHalf() == Half::One);
        static_assert(AZStd::hash<ColorHalf>{}(white) == 0x3C003C003C003C00ull);
        static_assert((2.0f * white).GetR() == (white * 2.0f).GetR());

        constexpr ColorHalf premultiplied = ColorHalf(1.0f, 1.0f, 1.0f, 0.5f).Premultiply();
        static_assert(premultiplied.GetR() == 0.5f);
        static_assert(premultiplied.GetA() == 0.5f, "alpha itself is not scaled");

        EXPECT_NEAR(white.GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_TRUE(white.IsClose(ColorHalf(1.0f, 1.0f, 1.0f, 1.0f)));
        EXPECT_THAT(white.ToVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));

        ColorHalf mutableHalf;
        mutableHalf.SetR(0.5f);
        mutableHalf.SetG(0.25f);
        EXPECT_FLOAT_EQ(mutableHalf.GetR(), 0.5f);
        EXPECT_FLOAT_EQ(mutableHalf.GetG(), 0.25f);
    }

    //! The runtime arithmetic may use 4-wide vector paths, while constant evaluation always
    //! computes per channel. Every lane must match the scalar channel computation bit for bit,
    //! except NaN results, which only have to agree in class.
    TEST(MATH_ColorHalf, RuntimeOperatorsMatchScalarChannels)
    {
        int mismatches = 0;
        auto lanesMatch = [](Half actual, Half expected) -> bool
        {
            if (actual.GetBits() == expected.GetBits())
            {
                return true;
            }
            return actual.IsNaN() && expected.IsNaN();
        };
        auto colorsMatch = [&](const ColorHalf actual, const ColorHalf expected) -> bool
        {
            return lanesMatch(actual.GetRHalf(), expected.GetRHalf())
                && lanesMatch(actual.GetGHalf(), expected.GetGHalf())
                && lanesMatch(actual.GetBHalf(), expected.GetBHalf())
                && lanesMatch(actual.GetAHalf(), expected.GetAHalf());
        };
        auto check = [&](const ColorHalf lhs, const ColorHalf rhs, float scalar)
        {
            const ColorHalf expectedSum(
                Half(lhs.GetR() + rhs.GetR()),
                Half(lhs.GetG() + rhs.GetG()),
                Half(lhs.GetB() + rhs.GetB()),
                Half(lhs.GetA() + rhs.GetA()));
            const ColorHalf expectedDifference(
                Half(lhs.GetR() - rhs.GetR()),
                Half(lhs.GetG() - rhs.GetG()),
                Half(lhs.GetB() - rhs.GetB()),
                Half(lhs.GetA() - rhs.GetA()));
            const ColorHalf expectedProduct(
                Half(lhs.GetR() * rhs.GetR()),
                Half(lhs.GetG() * rhs.GetG()),
                Half(lhs.GetB() * rhs.GetB()),
                Half(lhs.GetA() * rhs.GetA()));
            const ColorHalf expectedScaled(
                Half(lhs.GetR() * scalar),
                Half(lhs.GetG() * scalar),
                Half(lhs.GetB() * scalar),
                Half(lhs.GetA() * scalar));
            const ColorHalf expectedDivided(
                Half(lhs.GetR() / scalar),
                Half(lhs.GetG() / scalar),
                Half(lhs.GetB() / scalar),
                Half(lhs.GetA() / scalar));
            const ColorHalf expectedNegated(
                Half(-lhs.GetR()),
                Half(-lhs.GetG()),
                Half(-lhs.GetB()),
                Half(-lhs.GetA()));
            // Lerp steps are separate statements so the compiler cannot contract them into a
            // fused multiply-add the constexpr path would not use.
            auto lerpHalf = [](float from, float to, float t) -> Half
            {
                const float diff = to - from;
                const float scaledDiff = diff * t;
                return Half(from + scaledDiff);
            };
            const ColorHalf expectedStepped(
                lerpHalf(lhs.GetR(), rhs.GetR(), scalar),
                lerpHalf(lhs.GetG(), rhs.GetG(), scalar),
                lerpHalf(lhs.GetB(), rhs.GetB(), scalar),
                lerpHalf(lhs.GetA(), rhs.GetA(), scalar));
            const float alpha = lhs.GetA();
            const ColorHalf expectedPremultiplied(
                Half(lhs.GetR() * alpha),
                Half(lhs.GetG() * alpha),
                Half(lhs.GetB() * alpha),
                lhs.GetAHalf());
            const bool allMatch =
                colorsMatch(lhs + rhs, expectedSum)
                && colorsMatch(lhs - rhs, expectedDifference)
                && colorsMatch(lhs * rhs, expectedProduct)
                && colorsMatch(lhs * scalar, expectedScaled)
                && colorsMatch(lhs / scalar, expectedDivided)
                && colorsMatch(-lhs, expectedNegated)
                && colorsMatch(lhs.Lerp(rhs, scalar), expectedStepped)
                && colorsMatch(lhs.Premultiply(), expectedPremultiplied);
            if (!allMatch)
            {
                ++mismatches;
            }
        };

        // Every special value class in every lane, crossed with every other.
        const u16 specialBits[] = {
            0x0000, 0x8000, 0x0001, 0x8001, 0x03FF, 0x0400, 0x3C00, 0xBC00, 0x7BFF, 0xFBFF, 0x7C00, 0xFC00, 0x7E00,
        };
        for (const u16 a : specialBits)
        {
            for (const u16 b : specialBits)
            {
                check(ColorHalf::FromBits(a, b, a, b), ColorHalf::FromBits(b, a, b, a), Half::FromBits(a));
            }
        }

        // A seeded sweep over arbitrary channel bit patterns.
        u64 state = 0xDEADBEEFCAFEF00Dull;
        for (int i = 0; i < 200000; ++i)
        {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            const u64 lhsBits = state;
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            const u64 rhsBits = state;
            const ColorHalf lhs = ColorHalf::FromBits(
                static_cast<u16>(lhsBits),
                static_cast<u16>(lhsBits >> 16),
                static_cast<u16>(lhsBits >> 32),
                static_cast<u16>(lhsBits >> 48));
            const ColorHalf rhs = ColorHalf::FromBits(
                static_cast<u16>(rhsBits),
                static_cast<u16>(rhsBits >> 16),
                static_cast<u16>(rhsBits >> 32),
                static_cast<u16>(rhsBits >> 48));
            check(lhs, rhs, rhs.GetA());
        }
        EXPECT_EQ(mismatches, 0);
    }
} // namespace UnitTest
