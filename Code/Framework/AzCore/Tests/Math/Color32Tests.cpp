/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color32.h>

#include <AzCore/Math/Colors.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AZTestShared/Math/MathTestHelpers.h>

#include <cmath>
#include <limits>

using namespace AZ;

namespace UnitTest
{
    namespace
    {
        const float g_color32TestNaN = AZStd::numeric_limits<float>::quiet_NaN();
        constexpr float g_color32TestInfinity = AZStd::numeric_limits<float>::infinity();
    } // namespace

    TEST(MATH_Color32, Layout)
    {
        static_assert(sizeof(Color32) == 4);
        static_assert(alignof(Color32) == 4);
        static_assert(AZStd::is_trivially_copyable_v<Color32>);
        static_assert(offsetof(Color32, m_r) == 0);
        static_assert(offsetof(Color32, m_g) == 1);
        static_assert(offsetof(Color32, m_b) == 2);
        static_assert(offsetof(Color32, m_a) == 3);

        // The packed and array views must agree with the named components. This pins the
        // little-endian layout that AsU32*() depends on being folded back into a single load.
        const Color32 color(0x11, 0x22, 0x33, 0x44);
        EXPECT_EQ(color.m_channels[0], 0x11);
        EXPECT_EQ(color.m_channels[3], 0x44);
        EXPECT_EQ(color.m_packed, 0x44332211u);
    }

    //! The feature this whole type exists for: a named color usable as a Color32 in a constant
    //! expression, in the exact spelling it has to support.
    TEST(MATH_Color32, ConstexprConversionFromPalette)
    {
        constexpr AZ::Color32 white = AZ::Colors::White;
        static_assert(white == Color32(255, 255, 255, 255));
        static_assert(Color32(Colors::Navy) == Color32(0, 0, 128, 255));
        static_assert(Color32{}.GetA8() == 255, "default construction is opaque black");
        static_assert(Color32(1, 2, 3).GetA8() == 255, "alpha defaults to opaque");
        static_assert(Color32::CreateZero().IsTransparent());
        static_assert(Color32::CreateOne().IsOpaque());
        static_assert(Color32::CreateGray(128) == Color32(128, 128, 128, 255));
    }

    TEST(MATH_Color32, ByteOrderPacking)
    {
        constexpr Color32 color(0x11, 0x22, 0x33, 0x44); // r, g, b, a
        static_assert(color.AsU32Rgba() == 0x11223344u);
        static_assert(color.AsU32Argb() == 0x44112233u);
        static_assert(color.AsU32Abgr() == 0x44332211u);
        static_assert(color.AsU32Bgra() == 0x33221144u);
        static_assert(color.AsU32() == color.AsU32Abgr(), "AsU32 is the AZ::Color::ToU32 spelling");

        static_assert(Color32::FromU32Rgba(0x11223344u) == color);
        static_assert(Color32::FromU32Argb(0x44112233u) == color);
        static_assert(Color32::FromU32Abgr(0x44332211u) == color);
        static_assert(Color32::FromU32Bgra(0x33221144u) == color);
        static_assert(Color32::FromU32(0x44332211u) == color);
    }

    //! A Color32 must pack identically to the AZ::Color it converts to, or migrating an existing
    //! ToU32() call site would silently reorder channels.
    TEST(MATH_Color32, PackingMatchesAzColor)
    {
        for (const ColorSwatch swatch : { Colors::White, Colors::Black, Colors::Red, Colors::Navy, Colors::Orange })
        {
            const Color32 packed = swatch;
            const Color asColor = swatch;
            EXPECT_EQ(packed.AsU32(), asColor.ToU32());
        }
    }

    //! Narrowing rounds to nearest and clamps. This deliberately differs from AZ::Color::GetR8(),
    //! which truncates and can wrap above 1.0, so assert the difference rather than imply it.
    TEST(MATH_Color32, NarrowingRoundsAndClamps)
    {
        EXPECT_EQ(Color32(Color(0.5f, 0.5f, 0.5f, 1.0f)).GetR8(), 128) << "127.5 rounds up, not down to 127";
        EXPECT_EQ(Color32(Color(1.0f, 1.0f, 1.0f, 1.0f)).GetR8(), 255);
        EXPECT_EQ(Color32(Color(0.0f, 0.0f, 0.0f, 0.0f)).GetR8(), 0);

        const Color outOfRange(4.0f, -2.0f, 0.0f, 1.0f);
        EXPECT_EQ(Color32(outOfRange).GetR8(), 255) << "above 1.0 clamps rather than wrapping";
        EXPECT_EQ(Color32(outOfRange).GetG8(), 0) << "below 0.0 clamps rather than wrapping";
    }

    //! Regression guard. AZ::GetClamp returns NaN unchanged (both of its comparisons are false for
    //! an unordered operand) and static_cast<u8> of NaN is undefined behaviour, so the narrowing
    //! path must order its own comparisons such that NaN lands on 0.
    TEST(MATH_Color32, NarrowingIsDefinedForNaNAndInfinity)
    {
        EXPECT_EQ(Color32(Color(g_color32TestNaN, g_color32TestNaN, g_color32TestNaN, g_color32TestNaN)).GetR8(), 0);
        EXPECT_EQ(Color32(Color(g_color32TestInfinity, g_color32TestInfinity, g_color32TestInfinity, g_color32TestInfinity)).GetR8(), 255);
        EXPECT_EQ(Color32(Color(-g_color32TestInfinity, -g_color32TestInfinity, -g_color32TestInfinity, -g_color32TestInfinity)).GetR8(), 0);

        // The same path backs the float scaling operators.
        EXPECT_EQ((Color32(Colors::White) / 0.0f).GetR8(), 255) << "divide by zero saturates";
        EXPECT_EQ((Color32(Colors::White) * -1.0f).GetR8(), 0) << "negative scale clamps to 0";
    }

    //! Modulation is the operation most likely to drift, so check it against the ideal
    //! round(a * b / 255) for every one of the 65536 input pairs rather than sampling.
    TEST(MATH_Color32, ModulationIsExactForEveryInputPair)
    {
        int overflows = 0;
        int deviations = 0;
        for (int a = 0; a < 256; ++a)
        {
            for (int b = 0; b < 256; ++b)
            {
                const Color32 result = Color32(static_cast<u8>(a), 0, 0, 255) * Color32(static_cast<u8>(b), 0, 0, 255);
                const int actual = result.GetR8();
                const int ideal = static_cast<int>(std::lround(a * b / 255.0));
                if (actual > 255)
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
        EXPECT_EQ(deviations, 0) << "modulation must equal round(a * b / 255) exactly";

        static_assert(Color32(Colors::White) * Color32(Colors::White) == Color32(Colors::White),
            "White is the modulation identity");
        static_assert(Color32(Colors::Navy) * Color32(Colors::White) == Color32(Colors::Navy));
    }

    TEST(MATH_Color32, AdditionAndSubtractionSaturate)
    {
        static_assert((Color32(Colors::White) + Color32(Colors::White)) == Color32(Colors::White));
        static_assert((Color32::CreateZero() - Color32(Colors::White)) == Color32(0, 0, 0, 0));

        EXPECT_EQ((Color32(200, 0, 0, 255) + Color32(100, 0, 0, 0)).GetR8(), 255) << "300 must saturate, not wrap to 44";
        EXPECT_EQ((Color32(50, 0, 0, 255) - Color32(100, 0, 0, 0)).GetR8(), 0) << "-50 must saturate, not wrap to 206";
    }

    //! Lerp clamps t. ColorHalf deliberately does not, so this asymmetry is worth pinning.
    TEST(MATH_Color32, LerpClampsItsParameter)
    {
        constexpr Color32 white = Colors::White;
        static_assert(white.Lerp(Color32::CreateZero(), 0.0f) == white);
        static_assert(white.Lerp(Color32::CreateZero(), 1.0f) == Color32::CreateZero());
        static_assert(white.Lerp(Color32::CreateZero(), 2.0f) == Color32::CreateZero(), "t above 1 clamps");
        static_assert(white.Lerp(Color32::CreateZero(), -1.0f) == white, "t below 0 clamps");
        EXPECT_EQ(white.Lerp(Color32::CreateZero(), g_color32TestNaN).GetR8(), 255) << "an unordered t must not be UB";
        EXPECT_EQ(white.Lerp(Color32::CreateZero(), 0.5f).GetR8(), 128);
    }

    //! Source-over compositing: the two alpha endpoints are the cases that must be exact, because
    //! a rounding error there shows up as a visible seam.
    TEST(MATH_Color32, OverCompositingEndpoints)
    {
        constexpr Color32 dst(10, 20, 30, 255);
        constexpr Color32 src(200, 200, 200, 255);

        EXPECT_EQ(dst.Over(src), src) << "an opaque source completely replaces the destination";
        EXPECT_EQ(dst.Over(Color32(200, 200, 200, 0)), dst) << "a transparent source leaves the destination alone";

        const Color32 halfBlend = dst.Over(Color32(200, 200, 200, 128));
        EXPECT_GT(halfBlend.GetR8(), dst.GetR8());
        EXPECT_LT(halfBlend.GetR8(), src.GetR8());
    }

    TEST(MATH_Color32, Premultiply)
    {
        constexpr Color32 premultiplied = Color32(255, 255, 255, 128).Premultiply();
        static_assert(premultiplied.GetR8() == 128);
        static_assert(premultiplied.GetA8() == 128, "alpha itself is not scaled");
        static_assert(Color32(255, 255, 255, 0).Premultiply() == Color32(0, 0, 0, 0));
        static_assert(Color32(Colors::White).Premultiply() == Color32(Colors::White), "opaque is a no-op");
    }

    //! Widening to Color64 uses bit replication, which is what makes the 8 <-> 16 bit round trip
    //! lossless. Prove it for every channel value rather than for a few samples.
    TEST(MATH_Color32, WideningToColor64RoundTripsEveryValue)
    {
        constexpr auto allValuesRoundTrip = []() constexpr
        {
            for (int i = 0; i <= 255; ++i)
            {
                const auto v = static_cast<u8>(i);
                const Color32 original(v, v, v, v);
                const Color64 widened = original;
                if (widened.GetR16() != static_cast<u16>(v * 257))
                {
                    return false;
                }
                if (Color32(widened) != original)
                {
                    return false;
                }
            }
            return true;
        };
        static_assert(allValuesRoundTrip(), "u8 -> u16 -> u8 must be exact for all 256 values");
        EXPECT_TRUE(allValuesRoundTrip());

        // The endpoint is the one bit replication exists for: 0xFF must widen to 0xFFFF, not 0xFF00.
        static_assert(Color64(Color32(255, 255, 255, 255)).GetR16() == 65535);
        static_assert(Color32(Colors::White).GetR() == 1.0f, "255 must normalize to exactly 1.0");
    }

    //! Widening is implicit because it is exact; narrowing stays explicit so precision is never lost
    //! silently during overload resolution.
    TEST(MATH_Color32, ConversionPolicy)
    {
        static_assert(AZStd::is_convertible_v<ColorSwatch, Color32>);
        static_assert(AZStd::is_convertible_v<Color32, Color64>);
        static_assert(AZStd::is_convertible_v<Color32, Color>);

        static_assert(!AZStd::is_convertible_v<Color, Color32>);
        static_assert(!AZStd::is_convertible_v<Color64, Color32>);
        static_assert(AZStd::is_constructible_v<Color32, Color>);
        static_assert(AZStd::is_constructible_v<Color32, Color64>);
    }

    TEST(MATH_Color32, HexRoundTrip)
    {
        static_assert(Color32::FromHex("#FF0000") == Color32(255, 0, 0, 255));
        static_assert(Color32::FromHex("#11223344") == Color32(0x11, 0x22, 0x33, 0x44));
        static_assert(Color32(0, 0, 128, 255).AsHexString() == "#000080FF");
        static_assert(Color32::FromHex(Color32(0x12, 0x34, 0x56, 0x78).AsHexString()) == Color32(0x12, 0x34, 0x56, 0x78));
    }

    //! The sRGB table is generated at compile time by a constexpr transfer function, so it has to be
    //! checked against the runtime reference AZ::Color uses, which evaluates pow() in double.
    TEST(MATH_Color32, SrgbMatchesTheReferenceTransferFunction)
    {
        double worstError = 0.0;
        for (int i = 0; i < 256; ++i)
        {
            const Color32 gamma(static_cast<u8>(i), 0, 0, 255);
            const float fromTable = gamma.SrgbToLinear().GetR();
            const float reference = Color::ConvertSrgbGammaToLinear(static_cast<float>(i) / 255.0f);
            worstError = AZ::GetMax(worstError, static_cast<double>(AZ::GetAbs(fromTable - reference)));
        }
        EXPECT_LT(worstError, 1.0e-6) << "the constexpr table must agree with pow() to float precision";
    }

    TEST(MATH_Color32, SrgbRoundTripsEveryValue)
    {
        for (int i = 0; i < 256; ++i)
        {
            const Color32 gamma(static_cast<u8>(i), static_cast<u8>(i), static_cast<u8>(i), 255);
            EXPECT_EQ(Color32::LinearToSrgb(gamma.SrgbToLinear()), gamma) << "failed at byte " << i;
        }
    }

    //! sRGB is a compile time operation, which AZ::Color::ConvertSrgbGammaToLinear can never be
    //! because it calls pow() in double.
    TEST(MATH_Color32, SrgbIsConstexpr)
    {
        static_assert(Color32(Colors::White).SrgbToLinear().GetR() == 1.0f);
        static_assert(Color32(Colors::Black).SrgbToLinear().GetR() == 0.0f);
        static_assert(Color32::LinearToSrgb(Color(1.0f, 1.0f, 1.0f, 1.0f)) == Color32(Colors::White));
        static_assert(Color32::LinearToSrgb(Color32(128, 128, 128, 255).SrgbToLinear()) == Color32(128, 128, 128, 255));

        // Below 0.04045 the transfer function is a plain linear segment, not the power curve.
        static_assert(Color32(10, 0, 0, 255).SrgbToLinear().GetR() < 0.0031f);
        // Alpha is linear and must pass through untouched.
        static_assert(Color32(0, 0, 0, 128).SrgbToLinear().GetA() == Color32(0, 0, 0, 128).GetA());
    }

    //! Reflection has to actually register, or the type is invisible to serialization, the editor
    //! and scripting.
    //!
    //! The SerializeContext half deliberately does not call Reflect itself: because Color32::Reflect
    //! is wired into MathReflect, a freshly constructed SerializeContext already knows the type.
    //! Asserting that is a stronger statement than asserting Reflect() compiles, since it also covers
    //! the MathReflection.cpp registration. (Calling Reflect again here would be a double
    //! registration and would trip the duplicate-Uuid assert.)
    class Color32ReflectionFixture : public LeakDetectionFixture
    {
    };

    TEST_F(Color32ReflectionFixture, IsRegisteredWithSerializeContext)
    {
        SerializeContext serializeContext;
        const SerializeContext::ClassData* classData = serializeContext.FindClassData(azrtti_typeid<Color32>());
        ASSERT_NE(classData, nullptr) << "Color32 is not reflected; check MathReflect in MathReflection.cpp";
        EXPECT_STREQ(classData->m_name, "Color32");
    }

    TEST_F(Color32ReflectionFixture, ReflectsScriptingSurfaceToBehaviorContext)
    {
        BehaviorContext behaviorContext;
        Color32::Reflect(&behaviorContext);

        const auto classIter = behaviorContext.m_typeToClassMap.find(azrtti_typeid<Color32>());
        ASSERT_NE(classIter, behaviorContext.m_typeToClassMap.end());
        const BehaviorClass* behaviorClass = classIter->second;
        ASSERT_NE(behaviorClass, nullptr);

        for (const char* property : { "r", "g", "b", "a" })
        {
            EXPECT_NE(behaviorClass->m_properties.find(property), behaviorClass->m_properties.end())
                << "missing scripting property " << property;
        }
        EXPECT_NE(behaviorClass->m_methods.find("GetLuminance"), behaviorClass->m_methods.end());
        EXPECT_NE(behaviorClass->m_methods.find("Inverted"), behaviorClass->m_methods.end());
    }

    TEST(MATH_Color32, Accessors)
    {
        constexpr Color32 color(10, 20, 30, 40);
        static_assert(color.GetElement(0) == 10 && color.GetElement(3) == 40);
        static_assert(color[2] == 30);
        static_assert(color.GetMaxComponent() == 30 && color.GetMinComponent() == 10);
        static_assert(color.WithAlpha(255).IsOpaque());
        static_assert(Color32(Colors::White).Inverted() == Color32(0, 0, 0, 255), "alpha is preserved");
        static_assert(AZStd::hash<Color32>{}(Colors::Navy) == Color32(Colors::Navy).AsU32Rgba());
        static_assert(Color32(Colors::Black) < Color32(Colors::White));

        EXPECT_NEAR(Color32(Colors::White).GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_NEAR(Color32(128, 0, 0, 255).GetR(), 128.0f / 255.0f, Constants::Tolerance);
        EXPECT_THAT(Color32(Colors::White).AsVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));

        Color32 mutable32;
        mutable32.SetR8(7);
        mutable32.SetElement(1, 9);
        EXPECT_EQ(mutable32.GetR8(), 7);
        EXPECT_EQ(mutable32.GetG8(), 9);
    }

    //! These conversions cross from the packed integer types into the SIMD backed ones. They only
    //! became constant expressions once Vector3/Vector4/Color did, so they are the assertions that
    //! break first if that ever regresses.
    TEST(MATH_Color32, ConvertsToVectorTypesInConstantExpressions)
    {
        static_assert(Color32(255, 128, 0, 255).AsVector4() == Vector4(1.0f, 128.0f / 255.0f, 0.0f, 1.0f));
        static_assert(Color32(255, 128, 0, 255).AsVector3() == Vector3(1.0f, 128.0f / 255.0f, 0.0f));

        // Widening to Color is implicit, narrowing back is explicit; both round trip exactly for
        // values that started life as bytes.
        constexpr Color widened = Color32(255, 128, 0, 255);
        static_assert(widened.GetR() == 1.0f);
        static_assert(Color32(widened) == Color32(255, 128, 0, 255));
    }

} // namespace UnitTest
