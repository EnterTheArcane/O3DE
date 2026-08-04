/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color64.h>

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Math/Colors.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AZTestShared/Math/MathTestHelpers.h>

#include <AzCore/std/bit.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>

#include <cmath>
#include <cstring>
#include <limits>

using namespace AZ;

namespace UnitTest
{
    namespace
    {
        const float Color64TestNaN = AZStd::numeric_limits<float>::quiet_NaN();
        constexpr float Color64TestInfinity = AZStd::numeric_limits<float>::infinity();
    } // namespace

    TEST(MATH_Color64, Layout)
    {
        static_assert(sizeof(Color64) == 8);
        static_assert(alignof(Color64) == 8);
        static_assert(AZStd::is_trivially_copyable_v<Color64>);

        const Color64 color(0x1111, 0x2222, 0x3333, 0x4444);
        AZStd::array<u16, 4> channels{};
        memcpy(channels.data(), &color, sizeof(color));
        EXPECT_THAT(channels, ::testing::ElementsAre(0x1111, 0x2222, 0x3333, 0x4444));

        u64 packed = 0;
        memcpy(&packed, &color, sizeof(packed));
        EXPECT_EQ(packed, 0x4444333322221111ull);
    }

    TEST(MATH_Color64, ConstexprConversionFromPalette)
    {
        constexpr AZ::Color64 white = AZ::Colors::White;
        static_assert(white == Color64(65535, 65535, 65535, 65535));
        static_assert(white.GetR() == 1.0f, "255 must widen to exactly 1.0");
        static_assert(Color64(Colors::Navy).GetB16() == 128 * 257);
        static_assert(Color64{}.GetA16() == Color64::MaxChannel, "default construction is opaque black");
        static_assert(Color64::Zero.IsTransparent());
        static_assert(Color64::One.IsOpaque());
    }

    //! Bit replication (v * 257) rather than a left shift is what makes 8 -> 16-bit widening reach the true endpoint.
    //! A shift would map 0xFF to 0xFF00 and lose 1.0.
    TEST(MATH_Color64, BitReplicationReachesTheEndpoints)
    {
        static_assert(Color64(Internal::NamedColor(255, 255, 255, 255)).GetR16() == 65535, "not 0xFF00");
        static_assert(Color64(Internal::NamedColor(0, 0, 0, 0)).GetR16() == 0);
        static_assert(Color64(Internal::NamedColor(1, 0, 0, 255)).GetR16() == 257);
        static_assert(Color64(Internal::NamedColor(128, 0, 0, 255)).GetR16() == 32896);
        static_assert(Color64(Colors::White).GetR() == 1.0f);
    }

    //! Modulation computes (a * b + 32767) / 65535 in a 32-bit intermediate.
    //! At the maximum that is 65535 * 65535 + 32767 == 4294868992, only ~98M below the u32 ceiling,
    //! so the headroom is genuinely tight and an overflow would silently wrap.
    //! Sweep the range to prove it does not.
    TEST(MATH_Color64, ModulationDoesNotOverflowItsIntermediate)
    {
        static_assert(
            Color64::One * Color64::One == Color64::One,
            "the worst case for the 32-bit intermediate");

        int overflows = 0;
        int deviations = 0;
        for (long a = 0; a <= 65535; a += 257)
        {
            for (long b = 0; b <= 65535; b += 257)
            {
                const Color64 result = Color64(static_cast<u16>(a), 0, 0, 65535) * Color64(static_cast<u16>(b), 0, 0, 65535);
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

        static_assert(
            Color64(Colors::Navy) * Color64(Colors::White) == Color64(Colors::Navy),
            "White is the modulation identity");
    }

    TEST(MATH_Color64, AdditionAndSubtractionSaturate)
    {
        static_assert((Color64::One + Color64::One) == Color64::One);
        static_assert((Color64::Zero - Color64::One) == Color64(0, 0, 0, 0));

        EXPECT_EQ((Color64(60000, 0, 0, 65535) + Color64(10000, 0, 0, 0)).GetR16(), 65535) << "must saturate, not wrap";
        EXPECT_EQ((Color64(1000, 0, 0, 65535) - Color64(2000, 0, 0, 0)).GetR16(), 0) << "must saturate, not wrap";
    }

    //! Same regression guard as Color32: AZ::GetClamp passes NaN through unchanged and
    //! static_cast<u16> of NaN is undefined behavior, so the narrowing path handles it itself.
    TEST(MATH_Color64, NarrowingRoundsClampsAndIsDefinedForNaN)
    {
        EXPECT_EQ(Color64(Color(1.0f, 1.0f, 1.0f, 1.0f)).GetR16(), 65535);
        EXPECT_EQ(Color64(Color(0.0f, 0.0f, 0.0f, 0.0f)).GetR16(), 0);
        EXPECT_EQ(Color64(Color(0.5f, 0.5f, 0.5f, 1.0f)).GetR16(), 32768) << "rounds to nearest";

        const Color outOfRange(4.0f, -2.0f, 0.0f, 1.0f);
        EXPECT_EQ(Color64(outOfRange).GetR16(), 65535);
        EXPECT_EQ(Color64(outOfRange).GetG16(), 0);

        EXPECT_EQ(Color64(Color(Color64TestNaN, Color64TestNaN, Color64TestNaN, Color64TestNaN)).GetR16(), 0);
        EXPECT_EQ(Color64(Color(Color64TestInfinity, Color64TestInfinity, Color64TestInfinity, Color64TestInfinity)).GetR16(), 65535);
        EXPECT_EQ((Color64::One * -1.0f).GetR16(), 0);
    }

    TEST(MATH_Color64, LerpClampsItsParameter)
    {
        constexpr Color64 white = Colors::White;
        static_assert(white.Lerp(Color64::Zero, 0.0f) == white);
        static_assert(white.Lerp(Color64::Zero, 1.0f) == Color64::Zero);
        static_assert(white.Lerp(Color64::Zero, 2.0f) == Color64::Zero, "t above 1 clamps");
        static_assert(white.Lerp(Color64::Zero, -1.0f) == white, "t below 0 clamps");
        EXPECT_EQ(white.Lerp(Color64::Zero, Color64TestNaN).GetR16(), 65535) << "an unordered t must not be UB";
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
        static_assert(Color64::One.Premultiply() == Color64::One, "opaque is a no-op");
    }

    TEST(MATH_Color64, ConversionPolicy)
    {
        static_assert(AZStd::is_convertible_v<Internal::NamedColor, Color64>);
        static_assert(AZStd::is_convertible_v<Color64, Color>);
        static_assert(!AZStd::is_convertible_v<Color, Color64>);
        static_assert(AZStd::is_constructible_v<Color64, Color>);
    }

    TEST(MATH_Color64, Accessors)
    {
        constexpr Color64 white = Colors::White;
        static_assert(white.Inverted() == Color64(0, 0, 0, 65535), "alpha is preserved");
        static_assert(white.WithAlpha(0).IsTransparent());
        static_assert(AZStd::hash<Color64>{}(white) == 0xFFFFFFFFFFFFFFFFull);
        static_assert(Color64(10, 20, 30, 40).GetMaxComponent() == 30);
        static_assert(Color64(10, 20, 30, 40).GetMinComponent() == 10);
        static_assert(Color64(Colors::Black) < white);
        static_assert((2.0f * Color64(1000, 0, 0, 65535)).GetR16() == (Color64(1000, 0, 0, 65535) * 2.0f).GetR16());

        EXPECT_NEAR(white.GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_THAT(white.ToVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));

        Color64 mutable64;
        mutable64.SetR16(7);
        mutable64.SetG16(9);
        EXPECT_EQ(mutable64.GetR16(), 7);
        EXPECT_EQ(mutable64.GetG16(), 9);
    }

    //! Round trip through the normalized float accessors must be exact for every channel value.
    //! Runtime rather than constexpr: 65536 iterations would trip MSVC's constexpr step limit.
    TEST(MATH_Color64, FloatChannelRoundTripsEveryWordValue)
    {
        int failures = 0;
        for (int i = 0; i <= 65535; ++i)
        {
            Color64 roundTrip;
            roundTrip.SetR(Color64(static_cast<u16>(i), 0, 0, 0).GetR());
            if (roundTrip.GetR16() != i)
            {
                ++failures;
            }
        }
        EXPECT_EQ(failures, 0);
    }

    class Color64ReflectionFixture : public LeakDetectionFixture
    {
    };

    TEST_F(Color64ReflectionFixture, IsRegisteredWithSerializeContext)
    {
        SerializeContext serializeContext;
        const SerializeContext::ClassData* classData = serializeContext.FindClassData(azrtti_typeid<Color64>());
        ASSERT_NE(classData, nullptr) << "Color64 is not reflected; check MathReflect in MathReflection.cpp";
        EXPECT_STREQ(classData->m_name, "Color64");
    }

    TEST_F(Color64ReflectionFixture, ReflectsScriptingSurfaceToBehaviorContext)
    {
        BehaviorContext behaviorContext;
        Color64::Reflect(&behaviorContext);

        const auto classIter = behaviorContext.m_typeToClassMap.find(azrtti_typeid<Color64>());
        ASSERT_NE(classIter, behaviorContext.m_typeToClassMap.end());
        const BehaviorClass* behaviorClass = classIter->second;
        ASSERT_NE(behaviorClass, nullptr);

        for (const char* property : {"r", "g", "b", "a"})
        {
            EXPECT_NE(behaviorClass->m_properties.find(property), behaviorClass->m_properties.end())
                << "missing scripting property " << property;
        }
        EXPECT_NE(behaviorClass->m_methods.find("GetLuminance"), behaviorClass->m_methods.end());
    }

    TEST_F(Color64ReflectionFixture, RoundTripsThroughObjectStream)
    {
        SerializeContext serializeContext;
        const Color64 original(11, 22, 33, 44);

        for (const auto streamType : {ObjectStream::ST_XML, ObjectStream::ST_BINARY})
        {
            AZStd::vector<char> buffer;
            IO::ByteContainerStream<AZStd::vector<char>> stream(&buffer);
            EXPECT_TRUE(Utils::SaveObjectToStream(stream, streamType, &original, &serializeContext));

            stream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
            Color64 loaded;
            EXPECT_TRUE(Utils::LoadObjectFromStreamInPlace(stream, loaded, &serializeContext));
            EXPECT_EQ(loaded, original);
        }
    }

    //! The runtime operators may use vector instructions, while constant evaluation always uses the
    //! scalar reference. The channel domain is too large to sweep exhaustively, so this crosses
    //! every boundary value with every other and then runs a seeded random sweep.
    TEST(MATH_Color64, RuntimeOperatorsMatchScalarReference)
    {
        auto saturatingAdd = [](u32 x, u32 y) -> u16
        {
            const u32 sum = x + y;
            if (sum > 65535u)
            {
                return 65535;
            }
            return static_cast<u16>(sum);
        };
        auto saturatingSubtract = [](u32 x, u32 y) -> u16
        {
            if (x > y)
            {
                return static_cast<u16>(x - y);
            }
            return 0;
        };
        auto modulate = [](u32 x, u32 y) -> u16
        {
            return static_cast<u16>((static_cast<AZ::u64>(x) * y + 32767u) / 65535u);
        };

        int mismatches = 0;
        auto check = [&](u32 a, u32 b)
        {
            const u32 a2 = 65535 - a;
            const u32 b2 = 65535 - b;
            const Color64 lhs(static_cast<u16>(a), static_cast<u16>(a2), static_cast<u16>(a), static_cast<u16>(a2));
            const Color64 rhs(static_cast<u16>(b), static_cast<u16>(b2), static_cast<u16>(b2), static_cast<u16>(b));

            const Color64 sum = lhs + rhs;
            const Color64 difference = lhs - rhs;
            const Color64 product = lhs * rhs;
            const bool sumMatches =
                sum.GetR16() == saturatingAdd(a, b)
                && sum.GetG16() == saturatingAdd(a2, b2)
                && sum.GetB16() == saturatingAdd(a, b2)
                && sum.GetA16() == saturatingAdd(a2, b);
            const bool differenceMatches =
                difference.GetR16() == saturatingSubtract(a, b)
                && difference.GetG16() == saturatingSubtract(a2, b2)
                && difference.GetB16() == saturatingSubtract(a, b2)
                && difference.GetA16() == saturatingSubtract(a2, b);
            const bool productMatches =
                product.GetR16() == modulate(a, b)
                && product.GetG16() == modulate(a2, b2)
                && product.GetB16() == modulate(a, b2)
                && product.GetA16() == modulate(a2, b);
            if (!sumMatches || !differenceMatches || !productMatches)
            {
                ++mismatches;
            }
        };

        const u32 boundaryValues[] = {
            0, 1, 2, 127, 128, 255, 256, 257, 32766, 32767, 32768, 32769, 65024, 65279, 65533, 65534, 65535,
        };
        for (const u32 a : boundaryValues)
        {
            for (const u32 b : boundaryValues)
            {
                check(a, b);
            }
        }

        u32 state = 0x9E3779B9u;
        for (int i = 0; i < 1000000; ++i)
        {
            state = state * 1664525u + 1013904223u;
            check(state & 0xFFFFu, state >> 16);
        }
        EXPECT_EQ(mismatches, 0);
    }

    //! The conversions, Lerp, scaling and Inverted may also use vector paths. Expected values are
    //! computed with the scalar reference steps, kept as separate statements so the compiler
    //! cannot contract them into fused multiply-adds the constexpr path would not use.
    TEST(MATH_Color64, RuntimeConversionsMatchScalarReference)
    {
        auto clampUnit = [](float v) -> float
        {
            if (v > 0.0f)
            {
                if (v < 1.0f)
                {
                    return v;
                }
                return 1.0f;
            }
            return 0.0f;
        };
        auto fromFloat = [&](float v) -> u16
        {
            const float scaled = clampUnit(v) * 65535.0f;
            const float biased = scaled + 0.5f;
            return static_cast<u16>(biased);
        };
        auto lerpChannel = [&](u16 from, u16 to, float t) -> u16
        {
            const float clamped = clampUnit(t);
            const float diff = static_cast<float>(to) - static_cast<float>(from);
            const float scaledDiff = diff * clamped;
            const float sum = static_cast<float>(from) + scaledDiff;
            const float biased = sum + 0.5f;
            return static_cast<u16>(biased);
        };
        auto scaleChannel = [&](u16 x, float s) -> u16
        {
            const float normalized = static_cast<float>(x) * (1.0f / 65535.0f);
            const float scaled = normalized * s;
            return fromFloat(scaled);
        };
        auto divideChannel = [&](u16 x, float s) -> u16
        {
            const float normalized = static_cast<float>(x) * (1.0f / 65535.0f);
            const float divided = normalized / s;
            return fromFloat(divided);
        };

        int mismatches = 0;

        // Widening and inversion for every channel value, with distinct lanes.
        for (u32 v = 0; v < 65536; ++v)
        {
            const u16 lane0 = static_cast<u16>(v);
            const u16 lane1 = static_cast<u16>(65535 - v);
            const Color64 color(lane0, lane1, lane0, lane1);
            const Color widened = color;
            const bool widenMatches =
                widened.GetR() == static_cast<float>(lane0) * (1.0f / 65535.0f)
                && widened.GetG() == static_cast<float>(lane1) * (1.0f / 65535.0f)
                && widened.GetB() == static_cast<float>(lane0) * (1.0f / 65535.0f)
                && widened.GetA() == static_cast<float>(lane1) * (1.0f / 65535.0f);
            const Color64 inverted = color.Inverted();
            const bool invertMatches =
                inverted.GetR16() == static_cast<u16>(65535 - lane0)
                && inverted.GetG16() == static_cast<u16>(65535 - lane1)
                && inverted.GetB16() == static_cast<u16>(65535 - lane0)
                && inverted.GetA16() == lane1;
            if (!widenMatches || !invertMatches)
            {
                ++mismatches;
            }
        }

        // Narrowing, Lerp and scaling: every special value in every lane, then a seeded sweep.
        const float specialValues[] = {
            0.0f, -0.0f, 1.0f, -1.0f, 2.0f, 0.5f, 0.4999999f,
            1.0f / 65535.0f, 0.5f / 65535.0f, 65534.5f / 65535.0f, 1.0e-40f,
            Color64TestNaN, Color64TestInfinity, -Color64TestInfinity,
        };
        auto check = [&](float f0, float f1, float f2, float f3, u32 bits0, u32 bits1, float scalar)
        {
            const Color color(f0, f1, f2, f3);
            const Color64 narrowed(color);
            const bool narrowMatches =
                narrowed.GetR16() == fromFloat(f0)
                && narrowed.GetG16() == fromFloat(f1)
                && narrowed.GetB16() == fromFloat(f2)
                && narrowed.GetA16() == fromFloat(f3);

            const Color64 from(static_cast<u16>(bits0), static_cast<u16>(bits0 >> 16), static_cast<u16>(bits1), static_cast<u16>(bits1 >> 16));
            const Color64 to(static_cast<u16>(bits1), static_cast<u16>(bits0), static_cast<u16>(bits1 >> 16), static_cast<u16>(bits0 >> 16));
            const Color64 stepped = from.Lerp(to, scalar);
            const bool lerpMatches =
                stepped.GetR16() == lerpChannel(from.GetR16(), to.GetR16(), scalar)
                && stepped.GetG16() == lerpChannel(from.GetG16(), to.GetG16(), scalar)
                && stepped.GetB16() == lerpChannel(from.GetB16(), to.GetB16(), scalar)
                && stepped.GetA16() == lerpChannel(from.GetA16(), to.GetA16(), scalar);

            const Color64 scaled = from * scalar;
            const Color64 divided = from / scalar;
            const bool scaleMatches =
                scaled.GetR16() == scaleChannel(from.GetR16(), scalar)
                && scaled.GetG16() == scaleChannel(from.GetG16(), scalar)
                && scaled.GetB16() == scaleChannel(from.GetB16(), scalar)
                && scaled.GetA16() == scaleChannel(from.GetA16(), scalar);
            const bool divideMatches =
                divided.GetR16() == divideChannel(from.GetR16(), scalar)
                && divided.GetG16() == divideChannel(from.GetG16(), scalar)
                && divided.GetB16() == divideChannel(from.GetB16(), scalar)
                && divided.GetA16() == divideChannel(from.GetA16(), scalar);
            if (!narrowMatches || !lerpMatches || !scaleMatches || !divideMatches)
            {
                ++mismatches;
            }
        };

        for (const float special : specialValues)
        {
            check(special, 0.5f, -special, 1.0f, 0x12345678u, 0x9ABCDEF0u, special);
        }
        u32 state = 0xC0FFEE42u;
        auto next = [&]() -> u32
        {
            state = state * 1664525u + 1013904223u;
            return state;
        };
        for (int i = 0; i < 200000; ++i)
        {
            const float f0 = AZStd::bit_cast<float>(next());
            const float f1 = AZStd::bit_cast<float>(next());
            const float f2 = AZStd::bit_cast<float>(next());
            const float f3 = AZStd::bit_cast<float>(next());
            const u32 bits0 = next();
            const u32 bits1 = next();
            const float scalar = AZStd::bit_cast<float>(next());
            check(f0, f1, f2, f3, bits0, bits1, scalar);
        }
        EXPECT_EQ(mismatches, 0);
    }
} // namespace UnitTest
