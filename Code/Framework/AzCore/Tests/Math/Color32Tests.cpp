/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color32.h>

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Math/Colors.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
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
        const float Color32TestNaN = AZStd::numeric_limits<float>::quiet_NaN();
        constexpr float Color32TestInfinity = AZStd::numeric_limits<float>::infinity();
    } // namespace

    //! The channels are private, so the layout is pinned through the object representation rather than offsetof.
    //! That is the stronger check anyway: it pins the exact in-memory texel layout,
    //! and it would catch a big endian host or an unexpected padding byte.
    TEST(MATH_Color32, Layout)
    {
        static_assert(sizeof(Color32) == 4);
        static_assert(alignof(Color32) == 4);
        static_assert(AZStd::is_trivially_copyable_v<Color32>);

        const Color32 color(0x11, 0x22, 0x33, 0x44);
        AZStd::array<u8, 4> bytes{};
        memcpy(bytes.data(), &color, sizeof(color));
        EXPECT_THAT(bytes, ::testing::ElementsAre(0x11, 0x22, 0x33, 0x44));

        u32 packed = 0;
        memcpy(&packed, &color, sizeof(packed));
        EXPECT_EQ(packed, 0x44332211u);
    }

    //! The feature this whole type exists for: a named color usable as a Color32
    //! in a constant expression, in the exact spelling it has to support.
    TEST(MATH_Color32, ConstexprConversionFromPalette)
    {
        constexpr AZ::Color32 white = AZ::Colors::White;
        static_assert(white == Color32(255, 255, 255, 255));
        static_assert(Color32(Colors::Navy) == Color32(0, 0, 128, 255));
        static_assert(Color32{}.GetA8() == 255, "default construction is opaque black");
        static_assert(Color32(1, 2, 3).GetA8() == 255, "alpha defaults to opaque");
        static_assert(Color32::Zero.IsTransparent());
        static_assert(Color32::One.IsOpaque());
    }

    TEST(MATH_Color32, ByteOrderPacking)
    {
        constexpr Color32 color(0x11, 0x22, 0x33, 0x44); // r, g, b, a
        static_assert(color.ToRgba() == 0x11223344u);
        static_assert(color.ToArgb() == 0x44112233u);
        static_assert(color.ToAbgr() == 0x44332211u);
        static_assert(color.ToBgra() == 0x33221144u);
        static_assert(color.ToU32() == color.ToAbgr(), "ToU32 is the AZ::Color::ToU32 spelling");

        static_assert(Color32::FromRgba(0x11223344u) == color);
        static_assert(Color32::FromArgb(0x44112233u) == color);
        static_assert(Color32::FromAbgr(0x44332211u) == color);
        static_assert(Color32::FromBgra(0x33221144u) == color);
        static_assert(Color32::FromU32(0x44332211u) == color);
    }

    //! A Color32 must pack identically to the AZ::Color it converts to,
    //! or migrating an existing ToU32() call site would silently reorder channels.
    TEST(MATH_Color32, PackingMatchesAzColor)
    {
        for (const Internal::NamedColor named : { Colors::White, Colors::Black, Colors::Red, Colors::Navy, Colors::Orange })
        {
            const Color32 packed = named;
            const Color asColor = named;
            EXPECT_EQ(packed.ToU32(), asColor.ToU32());
        }
    }

    //! Narrowing rounds to nearest and clamps.
    //! This deliberately differs from AZ::Color::GetR8(), which truncates and can wrap above 1.0,
    //! so assert the difference rather than imply it.
    TEST(MATH_Color32, NarrowingRoundsAndClamps)
    {
        EXPECT_EQ(Color32(Color(0.5f, 0.5f, 0.5f, 1.0f)).GetR8(), 128) << "127.5 rounds up, not down to 127";
        EXPECT_EQ(Color32(Color(1.0f, 1.0f, 1.0f, 1.0f)).GetR8(), 255);
        EXPECT_EQ(Color32(Color(0.0f, 0.0f, 0.0f, 0.0f)).GetR8(), 0);

        const Color outOfRange(4.0f, -2.0f, 0.0f, 1.0f);
        EXPECT_EQ(Color32(outOfRange).GetR8(), 255) << "above 1.0 clamps rather than wrapping";
        EXPECT_EQ(Color32(outOfRange).GetG8(), 0) << "below 0.0 clamps rather than wrapping";
    }

    //! Regression guard. AZ::GetClamp returns NaN unchanged
    //! (both of its comparisons are false for an unordered operand)
    //! and static_cast<u8> of NaN is undefined behavior, so the narrowing path
    //! must order its own comparisons such that NaN lands on 0.
    TEST(MATH_Color32, NarrowingIsDefinedForNaNAndInfinity)
    {
        EXPECT_EQ(Color32(Color(Color32TestNaN, Color32TestNaN, Color32TestNaN, Color32TestNaN)).GetR8(), 0);
        EXPECT_EQ(Color32(Color(Color32TestInfinity, Color32TestInfinity, Color32TestInfinity, Color32TestInfinity)).GetR8(), 255);
        EXPECT_EQ(Color32(Color(-Color32TestInfinity, -Color32TestInfinity, -Color32TestInfinity, -Color32TestInfinity)).GetR8(), 0);

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

        static_assert(
            Color32(Colors::White) * Color32(Colors::White) == Color32(Colors::White),
            "White is the modulation identity");
        static_assert(Color32(Colors::Navy) * Color32(Colors::White) == Color32(Colors::Navy));
    }

    TEST(MATH_Color32, AdditionAndSubtractionSaturate)
    {
        static_assert((Color32(Colors::White) + Color32(Colors::White)) == Color32(Colors::White));
        static_assert((Color32::Zero - Color32(Colors::White)) == Color32(0, 0, 0, 0));

        EXPECT_EQ((Color32(200, 0, 0, 255) + Color32(100, 0, 0, 0)).GetR8(), 255) << "300 must saturate, not wrap to 44";
        EXPECT_EQ((Color32(50, 0, 0, 255) - Color32(100, 0, 0, 0)).GetR8(), 0) << "-50 must saturate, not wrap to 206";
    }

    //! Lerp clamps t. ColorHalf deliberately does not, so this asymmetry is worth pinning.
    TEST(MATH_Color32, LerpClampsItsParameter)
    {
        constexpr Color32 white = Colors::White;
        static_assert(white.Lerp(Color32::Zero, 0.0f) == white);
        static_assert(white.Lerp(Color32::Zero, 1.0f) == Color32::Zero);
        static_assert(white.Lerp(Color32::Zero, 2.0f) == Color32::Zero, "t above 1 clamps");
        static_assert(white.Lerp(Color32::Zero, -1.0f) == white, "t below 0 clamps");
        EXPECT_EQ(white.Lerp(Color32::Zero, Color32TestNaN).GetR8(), 255) << "an unordered t must not be UB";
        EXPECT_EQ(white.Lerp(Color32::Zero, 0.5f).GetR8(), 128);
    }

    //! Source-over compositing: the two alpha endpoints are the cases that must be exact,
    //! because a rounding error there shows up as a visible seam.
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

    //! Widening to Color64 uses bit replication, which is what makes the 8 <-> 16-bit round trip lossless.
    //! Prove it for every channel value rather than for a few samples.
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

    //! Widening is implicit because it is exact.
    //! Narrowing stays explicit so precision is never lost silently during overload resolution.
    TEST(MATH_Color32, ConversionPolicy)
    {
        static_assert(AZStd::is_convertible_v<Internal::NamedColor, Color32>);
        static_assert(AZStd::is_convertible_v<Color32, Color64>);
        static_assert(AZStd::is_convertible_v<Color32, Color>);

        static_assert(!AZStd::is_convertible_v<Color, Color32>);
        static_assert(!AZStd::is_convertible_v<Color64, Color32>);
        static_assert(AZStd::is_constructible_v<Color32, Color>);
        static_assert(AZStd::is_constructible_v<Color32, Color64>);
    }

    //! Reflection has to actually register, or the type is invisible to serialization, the editor and scripting.
    //!
    //! The SerializeContext half deliberately does not call Reflect itself:
    //! because Color32::Reflect is wired into MathReflect, a freshly constructed SerializeContext already knows the type.
    //! Asserting that is a stronger statement than asserting Reflect() compiles, since it also covers the MathReflection.cpp registration.
    //! (Calling Reflect again here would be a double registration and would trip the duplicate-Uuid assert)
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
        static_assert(color.GetMaxComponent() == 30 && color.GetMinComponent() == 10);
        static_assert(color.WithAlpha(255).IsOpaque());
        static_assert(Color32(Colors::White).Inverted() == Color32(0, 0, 0, 255), "alpha is preserved");
        static_assert(AZStd::hash<Color32>{}(Colors::Navy) == Color32(Colors::Navy).ToRgba());
        static_assert(Color32(Colors::Black) < Color32(Colors::White));

        EXPECT_NEAR(Color32(Colors::White).GetLuminance(), 1.0f, Constants::Tolerance);
        EXPECT_NEAR(Color32(128, 0, 0, 255).GetR(), 128.0f / 255.0f, Constants::Tolerance);
        EXPECT_THAT(Color32(Colors::White).ToVector4(), IsClose(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));

        Color32 mutable32;
        mutable32.SetR8(7);
        mutable32.SetG8(9);
        EXPECT_EQ(mutable32.GetR8(), 7);
        EXPECT_EQ(mutable32.GetG8(), 9);
    }

    //! Round trip through the normalized float accessors must be exact for every channel value.
    TEST(MATH_Color32, FloatChannelRoundTripsEveryByteValue)
    {
        constexpr auto allValuesRoundTrip = []() constexpr
        {
            for (int i = 0; i <= 255; ++i)
            {
                Color32 roundTrip;
                roundTrip.SetR(Color32(static_cast<u8>(i), 0, 0, 0).GetR());
                if (roundTrip.GetR8() != i)
                {
                    return false;
                }
            }
            return true;
        };
        static_assert(allValuesRoundTrip());
    }

    //! Interior alpha values, checked against the documented formula:
    //! channel = round(src * srcAlpha / 255) + round(dst * (255 - srcAlpha) / 255), saturated.
    TEST(MATH_Color32, OverMatchesDocumentedFormula)
    {
        const Color32 dst(10, 20, 30, 255);
        for (const int alpha : {1, 51, 127, 128, 200, 254})
        {
            const Color32 src(200, 150, 100, static_cast<u8>(alpha));
            const Color32 blended = dst.Over(src);
            const auto reference = [alpha](int srcChannel, int dstChannel)
            {
                const int modulatedSrc = (srcChannel * alpha + 127) / 255;
                const int modulatedDst = (dstChannel * (255 - alpha) + 127) / 255;
                int sum = modulatedSrc + modulatedDst;
                if (sum > 255)
                {
                    sum = 255;
                }
                return sum;
            };
            EXPECT_EQ(blended.GetR8(), reference(200, 10)) << "alpha " << alpha;
            EXPECT_EQ(blended.GetG8(), reference(150, 20)) << "alpha " << alpha;
            EXPECT_EQ(blended.GetB8(), reference(100, 30)) << "alpha " << alpha;
        }
    }

    TEST_F(Color32ReflectionFixture, RoundTripsThroughObjectStream)
    {
        SerializeContext serializeContext;
        const Color32 original(11, 22, 33, 44);

        for (const auto streamType : {ObjectStream::ST_XML, ObjectStream::ST_BINARY})
        {
            AZStd::vector<char> buffer;
            IO::ByteContainerStream<AZStd::vector<char>> stream(&buffer);
            EXPECT_TRUE(Utils::SaveObjectToStream(stream, streamType, &original, &serializeContext));

            stream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
            Color32 loaded;
            EXPECT_TRUE(Utils::LoadObjectFromStreamInPlace(stream, loaded, &serializeContext));
            EXPECT_EQ(loaded, original);
        }
    }

    //! JSON is the path prefabs use, and it works off the reflected fields
    //! rather than any ObjectStream serializer, so it is pinned separately.
    TEST_F(Color32ReflectionFixture, RoundTripsThroughJsonSerialization)
    {
        SerializeContext serializeContext;
        JsonRegistrationContext jsonRegistrationContext;
        JsonSystemComponent::Reflect(&jsonRegistrationContext);

        const Color32 original(11, 22, 33, 44);
        rapidjson::Document document;

        JsonSerializerSettings storeSettings;
        storeSettings.m_serializeContext = &serializeContext;
        storeSettings.m_registrationContext = &jsonRegistrationContext;
        const auto storeCode = JsonSerialization::Store(document, document.GetAllocator(), original, storeSettings);
        EXPECT_NE(storeCode.GetProcessing(), JsonSerializationResult::Processing::Halted);

        JsonDeserializerSettings loadSettings;
        loadSettings.m_serializeContext = &serializeContext;
        loadSettings.m_registrationContext = &jsonRegistrationContext;
        Color32 loaded;
        const auto loadCode = JsonSerialization::Load(loaded, document, loadSettings);
        EXPECT_NE(loadCode.GetProcessing(), JsonSerializationResult::Processing::Halted);
        EXPECT_EQ(loaded, original);

        jsonRegistrationContext.EnableRemoveReflection();
        JsonSystemComponent::Reflect(&jsonRegistrationContext);
        jsonRegistrationContext.DisableRemoveReflection();
    }

    //! The runtime operators may use vector instructions, while constant evaluation always uses the
    //! scalar reference. Every channel pair goes through +, - and * with distinct values in every
    //! lane, so a lane-crossing or off-by-one rounding bug in either path cannot hide.
    TEST(MATH_Color32, RuntimeOperatorsMatchScalarReference)
    {
        auto saturatingAdd = [](u32 x, u32 y) -> u8
        {
            const u32 sum = x + y;
            if (sum > 255u)
            {
                return 255;
            }
            return static_cast<u8>(sum);
        };
        auto saturatingSubtract = [](u32 x, u32 y) -> u8
        {
            if (x > y)
            {
                return static_cast<u8>(x - y);
            }
            return 0;
        };
        auto modulate = [](u32 x, u32 y) -> u8
        {
            return static_cast<u8>((x * y + 127u) / 255u);
        };

        int mismatches = 0;
        for (u32 a = 0; a < 256; ++a)
        {
            for (u32 b = 0; b < 256; ++b)
            {
                const u32 a2 = 255 - a;
                const u32 a3 = a ^ 0x5Au;
                const u32 b2 = 255 - b;
                const u32 b3 = b ^ 0xA5u;
                const Color32 lhs(static_cast<u8>(a), static_cast<u8>(a2), static_cast<u8>(a3), static_cast<u8>(a));
                const Color32 rhs(static_cast<u8>(b), static_cast<u8>(b2), static_cast<u8>(b3), static_cast<u8>(b));

                const Color32 sum = lhs + rhs;
                const Color32 difference = lhs - rhs;
                const Color32 product = lhs * rhs;
                const bool sumMatches =
                    sum.GetR8() == saturatingAdd(a, b)
                    && sum.GetG8() == saturatingAdd(a2, b2)
                    && sum.GetB8() == saturatingAdd(a3, b3)
                    && sum.GetA8() == saturatingAdd(a, b);
                const bool differenceMatches =
                    difference.GetR8() == saturatingSubtract(a, b)
                    && difference.GetG8() == saturatingSubtract(a2, b2)
                    && difference.GetB8() == saturatingSubtract(a3, b3)
                    && difference.GetA8() == saturatingSubtract(a, b);
                const bool productMatches =
                    product.GetR8() == modulate(a, b)
                    && product.GetG8() == modulate(a2, b2)
                    && product.GetB8() == modulate(a3, b3)
                    && product.GetA8() == modulate(a, b);
                if (!sumMatches || !differenceMatches || !productMatches)
                {
                    ++mismatches;
                }
            }
        }
        EXPECT_EQ(mismatches, 0);
    }

    //! The conversions, Lerp, scaling and Inverted may also use vector paths. Expected values are
    //! computed with the scalar reference steps, kept as separate statements so the compiler
    //! cannot contract them into fused multiply-adds the constexpr path would not use.
    TEST(MATH_Color32, RuntimeConversionsMatchScalarReference)
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
        auto fromFloat = [&](float v) -> u8
        {
            const float scaled = clampUnit(v) * 255.0f;
            const float biased = scaled + 0.5f;
            return static_cast<u8>(biased);
        };
        auto lerpChannel = [&](u8 from, u8 to, float t) -> u8
        {
            const float clamped = clampUnit(t);
            const float diff = static_cast<float>(to) - static_cast<float>(from);
            const float scaledDiff = diff * clamped;
            const float sum = static_cast<float>(from) + scaledDiff;
            const float biased = sum + 0.5f;
            return static_cast<u8>(biased);
        };
        auto scaleChannel = [&](u8 x, float s) -> u8
        {
            const float normalized = static_cast<float>(x) * (1.0f / 255.0f);
            const float scaled = normalized * s;
            return fromFloat(scaled);
        };
        auto divideChannel = [&](u8 x, float s) -> u8
        {
            const float normalized = static_cast<float>(x) * (1.0f / 255.0f);
            const float divided = normalized / s;
            return fromFloat(divided);
        };

        int mismatches = 0;

        // Widening and inversion for every channel value, with distinct lanes.
        for (u32 v = 0; v < 256; ++v)
        {
            const u8 lane0 = static_cast<u8>(v);
            const u8 lane1 = static_cast<u8>(255 - v);
            const u8 lane2 = static_cast<u8>(v ^ 0x5A);
            const Color32 color(lane0, lane1, lane2, lane0);
            const Color widened = color;
            const bool widenMatches = widened.GetR() ==
                static_cast<float>(lane0) * (1.0f / 255.0f)
                && widened.GetG() == static_cast<float>(lane1) * (1.0f / 255.0f)
                && widened.GetB() == static_cast<float>(lane2) * (1.0f / 255.0f)
                && widened.GetA() == static_cast<float>(lane0) * (1.0f / 255.0f);
            const Color32 inverted = color.Inverted();
            const bool invertMatches = inverted.GetR8() ==
                static_cast<u8>(255 - lane0)
                && inverted.GetG8() == static_cast<u8>(255 - lane1)
                && inverted.GetB8() == static_cast<u8>(255 - lane2)
                && inverted.GetA8() == lane0;
            if (!widenMatches || !invertMatches)
            {
                ++mismatches;
            }
        }

        // Narrowing, Lerp and scaling: every special value in every lane, then a seeded sweep.
        const float specialValues[] = {
            0.0f, -0.0f, 1.0f, -1.0f, 2.0f, 0.5f, 0.4999999f,
            1.0f / 255.0f, 0.5f / 255.0f, 254.5f / 255.0f, 1.0e-40f,
            Color32TestNaN, Color32TestInfinity, -Color32TestInfinity,
        };
        auto check = [&](float f0, float f1, float f2, float f3, u32 bits, float scalar)
        {
            const Color color(f0, f1, f2, f3);
            const Color32 narrowed(color);
            const bool narrowMatches =
                narrowed.GetR8() == fromFloat(f0)
                && narrowed.GetG8() == fromFloat(f1)
                && narrowed.GetB8() == fromFloat(f2)
                && narrowed.GetA8() == fromFloat(f3);

            const Color32 from = Color32::FromRgba(bits);
            const Color32 to = Color32::FromBgra(bits);
            const Color32 stepped = from.Lerp(to, scalar);
            const bool lerpMatches =
                stepped.GetR8() == lerpChannel(from.GetR8(), to.GetR8(), scalar)
                && stepped.GetG8() == lerpChannel(from.GetG8(), to.GetG8(), scalar)
                && stepped.GetB8() == lerpChannel(from.GetB8(), to.GetB8(), scalar)
                && stepped.GetA8() == lerpChannel(from.GetA8(), to.GetA8(), scalar);

            const Color32 scaled = from * scalar;
            const Color32 divided = from / scalar;
            const bool scaleMatches =
                scaled.GetR8() == scaleChannel(from.GetR8(), scalar)
                && scaled.GetG8() == scaleChannel(from.GetG8(), scalar)
                && scaled.GetB8() == scaleChannel(from.GetB8(), scalar)
                && scaled.GetA8() == scaleChannel(from.GetA8(), scalar);
            const bool divideMatches =
                divided.GetR8() == divideChannel(from.GetR8(), scalar)
                && divided.GetG8() == divideChannel(from.GetG8(), scalar)
                && divided.GetB8() == divideChannel(from.GetB8(), scalar)
                && divided.GetA8() == divideChannel(from.GetA8(), scalar);
            if (!narrowMatches || !lerpMatches || !scaleMatches || !divideMatches)
            {
                ++mismatches;
            }
        };

        for (const float special : specialValues)
        {
            check(special, 0.5f, -special, 1.0f, 0x12345678u, special);
        }
        u32 state = 0xC001C0DEu;
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
            const u32 bits = next();
            const float scalar = AZStd::bit_cast<float>(next());
            check(f0, f1, f2, f3, bits, scalar);
        }
        EXPECT_EQ(mismatches, 0);
    }
} // namespace UnitTest
