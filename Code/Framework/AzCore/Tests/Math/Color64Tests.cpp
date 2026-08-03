/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color64.h>

#include <AzCore/Math/Colors.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AZTestShared/Math/MathTestHelpers.h>

#include <cmath>
#include <limits>

using namespace AZ;

namespace UnitTest
{
    namespace
    {
        const float g_color64TestNaN = AZStd::numeric_limits<float>::quiet_NaN();
        constexpr float g_color64TestInfinity = AZStd::numeric_limits<float>::infinity();
    } // namespace

    TEST(MATH_Color64, Layout)
    {
        static_assert(sizeof(Color64) == 8);
        static_assert(alignof(Color64) == 8);
        static_assert(AZStd::is_trivially_copyable_v<Color64>);
        static_assert(offsetof(Color64, m_r) == 0);
        static_assert(offsetof(Color64, m_g) == 2);
        static_assert(offsetof(Color64, m_b) == 4);
        static_assert(offsetof(Color64, m_a) == 6);

        // Pins the little-endian layout the packed view depends on.
        const Color64 color(0x1111, 0x2222, 0x3333, 0x4444);
        EXPECT_EQ(color.m_channels[1], 0x2222);
        EXPECT_EQ(color.m_packed, 0x4444333322221111ull);
    }

    TEST(MATH_Color64, ConstexprConversionFromPalette)
    {
        constexpr AZ::Color64 white = AZ::Colors::White;
        static_assert(white == Color64(65535, 65535, 65535, 65535));
        static_assert(white.GetR() == 1.0f, "255 must widen to exactly 1.0");
        static_assert(Color64(Colors::Navy).GetB16() == 128 * 257);
        static_assert(Color64{}.GetA16() == Color64::MaxChannel, "default construction is opaque black");
        static_assert(Color64::CreateZero().IsTransparent());
        static_assert(Color64::CreateOne().IsOpaque());
    }

    //! Bit replication (v * 257) rather than a left shift is what makes 8 -> 16 bit widening
    //! reach the true endpoint. A shift would map 0xFF to 0xFF00 and lose 1.0.
    TEST(MATH_Color64, BitReplicationReachesTheEndpoints)
    {
        static_assert(Color64(ColorSwatch(255, 255, 255, 255)).GetR16() == 65535, "not 0xFF00");
        static_assert(Color64(ColorSwatch(0, 0, 0, 0)).GetR16() == 0);
        static_assert(Color64(ColorSwatch(1, 0, 0, 255)).GetR16() == 257);
        static_assert(Color64(ColorSwatch(128, 0, 0, 255)).GetR16() == 32896);
        static_assert(Color64(Colors::White).GetR() == 1.0f);
    }

    TEST(MATH_Color64, WordOrderPacking)
    {
        constexpr Color64 color(0x1111, 0x2222, 0x3333, 0x4444); // r, g, b, a
        static_assert(color.AsU64Rgba() == 0x1111222233334444ull);
        static_assert(color.AsU64Argb() == 0x4444111122223333ull);
        static_assert(color.AsU64Abgr() == 0x4444333322221111ull);
        static_assert(color.AsU64Bgra() == 0x3333222211114444ull);

        static_assert(Color64::FromU64Rgba(0x1111222233334444ull) == color);
        static_assert(Color64::FromU64Argb(0x4444111122223333ull) == color);
        static_assert(Color64::FromU64Abgr(0x4444333322221111ull) == color);
        static_assert(Color64::FromU64Bgra(0x3333222211114444ull) == color);
    }

    //! Modulation computes (a * b + 32767) / 65535 in a 32 bit intermediate. At the maximum that is
    //! 65535 * 65535 + 32767 == 4294868992, only ~98M below the u32 ceiling, so the headroom is
    //! genuinely tight and an overflow would silently wrap. Sweep the range to prove it does not.
    TEST(MATH_Color64, ModulationDoesNotOverflowItsIntermediate)
    {
        static_assert(Color64::CreateOne() * Color64::CreateOne() == Color64::CreateOne(),
            "the worst case for the 32 bit intermediate");

        int overflows = 0;
        int deviations = 0;
        for (long a = 0; a <= 65535; a += 257)
        {
            for (long b = 0; b <= 65535; b += 257)
            {
                const Color64 result =
                    Color64(static_cast<u16>(a), 0, 0, 65535) * Color64(static_cast<u16>(b), 0, 0, 65535);
                const long actual = result.GetR16();
                const long ideal = std::lround(a * b / 65535.0);
                if (actual > 65535)
                {
                    ++overflows;
                }
                if (actual != ideal)
                {
                    ++deviations;
                }
            }
        }
        EXPECT_EQ(overflows, 0);
        EXPECT_EQ(deviations, 0) << "modulation must equal round(a * b / 65535) exactly";

        static_assert(Color64(Colors::Navy) * Color64(Colors::White) == Color64(Colors::Navy),
            "White is the modulation identity");
    }

    TEST(MATH_Color64, AdditionAndSubtractionSaturate)
    {
        static_assert((Color64::CreateOne() + Color64::CreateOne()) == Color64::CreateOne());
        static_assert((Color64::CreateZero() - Color64::CreateOne()) == Color64(0, 0, 0, 0));

        EXPECT_EQ((Color64(60000, 0, 0, 65535) + Color64(10000, 0, 0, 0)).GetR16(), 65535) << "must saturate, not wrap";
        EXPECT_EQ((Color64(1000, 0, 0, 65535) - Color64(2000, 0, 0, 0)).GetR16(), 0) << "must saturate, not wrap";
    }

    //! Same regression guard as Color32: AZ::GetClamp passes NaN through unchanged and
    //! static_cast<u16> of NaN is undefined behaviour, so the narrowing path handles it itself.
    TEST(MATH_Color64, NarrowingRoundsClampsAndIsDefinedForNaN)
    {
        EXPECT_EQ(Color64(Color(1.0f, 1.0f, 1.0f, 1.0f)).GetR16(), 65535);
        EXPECT_EQ(Color64(Color(0.0f, 0.0f, 0.0f, 0.0f)).GetR16(), 0);
        EXPECT_EQ(Color64(Color(0.5f, 0.5f, 0.5f, 1.0f)).GetR16(), 32768) << "rounds to nearest";

        const Color outOfRange(4.0f, -2.0f, 0.0f, 1.0f);
        EXPECT_EQ(Color64(outOfRange).GetR16(), 65535);
        EXPECT_EQ(Color64(outOfRange).GetG16(), 0);

        EXPECT_EQ(Color64(Color(g_color64TestNaN, g_color64TestNaN, g_color64TestNaN, g_color64TestNaN)).GetR16(), 0);
        EXPECT_EQ(Color64(Color(g_color64TestInfinity, g_color64TestInfinity, g_color64TestInfinity, g_color64TestInfinity)).GetR16(), 65535);
        EXPECT_EQ((Color64::CreateOne() * -1.0f).GetR16(), 0);
    }

    TEST(MATH_Color64, LerpClampsItsParameter)
    {
        constexpr Color64 white = Colors::White;
        static_assert(white.Lerp(Color64::CreateZero(), 0.0f) == white);
        static_assert(white.Lerp(Color64::CreateZero(), 1.0f) == Color64::CreateZero());
        static_assert(white.Lerp(Color64::CreateZero(), 2.0f) == Color64::CreateZero(), "t above 1 clamps");
        static_assert(white.Lerp(Color64::CreateZero(), -1.0f) == white, "t below 0 clamps");
        EXPECT_EQ(white.Lerp(Color64::CreateZero(), g_color64TestNaN).GetR16(), 65535) << "an unordered t must not be UB";
    }

    TEST(MATH_Color64, OverCompositingEndpoints)
    {
        constexpr Color64 dst(1000, 2000, 3000, 65535);
        constexpr Color64 src(50000, 50000, 50000, 65535);

        EXPECT_EQ(dst.Over(src), src) << "an opaque source completely replaces the destination";
        EXPECT_EQ(dst.Over(Color64(50000, 50000, 50000, 0)), dst) << "a transparent source is a no-op";
    }

    TEST(MATH_Color64, Premultiply)
    {
        constexpr Color64 premultiplied = Color64(65535, 65535, 65535, 32768).Premultiply();
        static_assert(premultiplied.GetA16() == 32768, "alpha itself is not scaled");
        EXPECT_NEAR(premultiplied.GetR(), 0.5f, 0.001f);
        static_assert(Color64::CreateOne().Premultiply() == Color64::CreateOne(), "opaque is a no-op");
    }

    TEST(MATH_Color64, ConversionPolicy)
    {
        static_assert(AZStd::is_convertible_v<ColorSwatch, Color64>);
        static_assert(AZStd::is_convertible_v<Color64, Color>);
        static_assert(!AZStd::is_convertible_v<Color, Color64>);
        static_assert(AZStd::is_constructible_v<Color64, Color>);
    }

    TEST(MATH_Color64, Accessors)
    {
        constexpr Color64 white = Colors::White;
        static_assert(white.Inverted() == Color64(0, 0, 0, 65535), "alpha is preserved");
        static_assert(white.WithAlpha(0).IsTransparent());
        static_assert(white.AsHexString() == "#FFFFFFFFFFFFFFFF");
        static_assert(AZStd::hash<Color64>{}(white) == 0xFFFFFFFFFFFFFFFFull);
        static_assert(Color64(10, 20, 30, 40).GetMaxComponent() == 30);
        static_assert(Color64(10, 20, 30, 40).GetMinComponent() == 10);
        static_assert(Color64(10, 20, 30, 40)[2] == 30);
        static_assert(Color64(Colors::Black) < white);

        EXPECT_NEAR(white.GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_THAT(white.AsVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));

        Color64 mutable64;
        mutable64.SetR16(7);
        mutable64.SetElement(1, 9);
        EXPECT_EQ(mutable64.GetR16(), 7);
        EXPECT_EQ(mutable64.GetG16(), 9);
    }

    //! These conversions cross from the packed integer types into the SIMD backed ones. They only
    //! became constant expressions once Vector3/Vector4/Color did, so they are the assertions that
    //! break first if that ever regresses.
    TEST(MATH_Color64, ConvertsToVectorTypesInConstantExpressions)
    {
        static_assert(Color64(65535, 0, 0, 65535).AsVector4() == Vector4(1.0f, 0.0f, 0.0f, 1.0f));
        static_assert(Color64(65535, 0, 0, 65535).AsVector3() == Vector3(1.0f, 0.0f, 0.0f));

        constexpr Color widened = Color64(65535, 0, 0, 65535);
        static_assert(widened.GetR() == 1.0f);
        static_assert(Color64(widened) == Color64(65535, 0, 0, 65535));
    }

} // namespace UnitTest
