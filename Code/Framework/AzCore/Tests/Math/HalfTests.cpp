/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Half.h>

#include <AzCore/UnitTest/TestTypes.h>

#include <cmath>
#include <limits>

using namespace AZ;

namespace UnitTest
{
    namespace
    {
        constexpr float g_halfTestInfinity = AZStd::numeric_limits<float>::infinity();
        const float g_halfTestNaN = AZStd::numeric_limits<float>::quiet_NaN();
    } // namespace

    TEST(MATH_Half, Layout)
    {
        static_assert(sizeof(Half) == 2);
        static_assert(AZStd::is_trivially_copyable_v<Half>);
        static_assert(Half::FromBits(0x1234).GetBits() == 0x1234, "FromBits must not convert");
    }

    TEST(MATH_Half, KnownBitPatterns)
    {
        static_assert(Half::FromFloat(1.0f) == 0x3C00);
        static_assert(Half::FromFloat(-1.0f) == 0xBC00);
        static_assert(Half::FromFloat(0.5f) == 0x3800);
        static_assert(Half::FromFloat(2.0f) == 0x4000);
        static_assert(Half::FromFloat(65504.0f) == 0x7BFF, "largest finite binary16 value");
        static_assert(Half::ToFloat(0x3C00) == 1.0f);
        static_assert(static_cast<float>(Half::CreateOne()) == 1.0f);
    }

    //! The sign of zero has to survive, because -0.0f and +0.0f have different bit patterns and a
    //! naive conversion that goes through a comparison against zero would collapse them.
    TEST(MATH_Half, SignedZero)
    {
        static_assert(Half::FromFloat(0.0f) == 0x0000);
        static_assert(Half::FromFloat(-0.0f) == 0x8000);
        static_assert(Half::CreateZero().IsZero());
        EXPECT_TRUE(Half(-0.0f).IsZero());
        EXPECT_TRUE(Half(-0.0f).IsNegative());
        EXPECT_FALSE(Half(0.0f).IsNegative());
        EXPECT_EQ(std::signbit(static_cast<float>(Half(-0.0f))), true);
    }

    //! Round to nearest, ties to even. The interesting cases are the two ties that straddle a
    //! representation boundary: one rounds up out of the finite range, the other down to zero.
    TEST(MATH_Half, RoundingTiesToEven)
    {
        static_assert(Half::FromFloat(65520.0f) == 0x7C00, "tie above the max rounds up into infinity");
        static_assert(Half::FromFloat(65519.0f) == 0x7BFF, "just below that tie stays finite");
        static_assert(Half::FromFloat(1.0f / 33554432.0f) == 0x0000, "2^-25 is a tie, rounds to even (zero)");
        static_assert(Half::FromFloat(1.0f / 16777216.0f) == 0x0001, "2^-24 is the smallest subnormal");

        // A tie in the middle of the normal range must also go to even, not always up.
        // 1.0 + 2^-11 sits exactly between 0x3C00 and 0x3C01; even is 0x3C00.
        static_assert(Half::FromFloat(1.0f + 0.00048828125f) == 0x3C00);
        // 1.0 + 3 * 2^-11 sits exactly between 0x3C01 and 0x3C02; even is 0x3C02.
        static_assert(Half::FromFloat(1.0f + 0.00146484375f) == 0x3C02);
    }

    //! The subnormal/normal transition is where the exponent encoding changes meaning, so both
    //! sides of it are checked explicitly.
    TEST(MATH_Half, SubnormalNormalBoundary)
    {
        EXPECT_EQ(Half::FromFloat(std::ldexp(1.0f, -14)), 0x0400) << "2^-14 is the smallest normal";
        EXPECT_EQ(Half::ToFloat(0x0400), std::ldexp(1.0f, -14));

        // The largest subnormal is (1023/1024) * 2^-14 == 2^-14 - 2^-24, immediately below it.
        EXPECT_FLOAT_EQ(Half::ToFloat(0x03FF), std::ldexp(1023.0f, -24));
        EXPECT_LT(Half::ToFloat(0x03FF), Half::ToFloat(0x0400));
        EXPECT_EQ(Half::FromFloat(std::ldexp(1023.0f, -24)), 0x03FF);
    }

    TEST(MATH_Half, OverflowAndUnderflow)
    {
        EXPECT_TRUE(Half(1.0e30f).IsInfinity());
        EXPECT_TRUE(Half(-1.0e30f).IsInfinity());
        EXPECT_TRUE(Half(-1.0e30f).IsNegative()) << "overflow must keep its sign";
        EXPECT_TRUE(Half(1.0e-30f).IsZero());
        EXPECT_TRUE(Half(-1.0e-30f).IsZero());
        EXPECT_TRUE(Half(-1.0e-30f).IsNegative()) << "underflow must keep its sign";
    }

    //! The exhaustive sweep below walks the half domain, so it never feeds a *float* NaN or infinity
    //! into FromFloat. Those take a separate early-out branch, so cover them directly.
    TEST(MATH_Half, FloatDomainNaNAndInfinity)
    {
        EXPECT_TRUE(Half(g_halfTestNaN).IsNaN());
        EXPECT_TRUE(Half(-g_halfTestNaN).IsNaN());
        EXPECT_TRUE(Half(-g_halfTestNaN).IsNegative()) << "a NaN's sign bit is carried through";
        EXPECT_FALSE(Half(g_halfTestNaN).IsInfinity());

        EXPECT_EQ(Half::FromFloat(g_halfTestInfinity), 0x7C00);
        EXPECT_EQ(Half::FromFloat(-g_halfTestInfinity), 0xFC00);
        EXPECT_TRUE(Half(g_halfTestInfinity).IsInfinity());
        EXPECT_FALSE(Half(g_halfTestInfinity).IsNaN());

        // A NaN with a small payload must not be truncated into an infinity.
        const float smallPayloadNaN = AZStd::bit_cast<float>(0x7F800001u);
        EXPECT_TRUE(Half(smallPayloadNaN).IsNaN()) << "mantissa truncation must not turn NaN into Inf";
    }

    TEST(MATH_Half, Classification)
    {
        static_assert(Half::CreateInfinity().IsInfinity() && !Half::CreateInfinity().IsNaN());
        static_assert(Half::CreateNaN().IsNaN() && !Half::CreateNaN().IsInfinity());
        static_assert(Half::CreateMax().IsFinite());
        static_assert(!Half::CreateInfinity().IsFinite() && !Half::CreateNaN().IsFinite());
        EXPECT_TRUE(std::isnan(static_cast<float>(Half::CreateNaN())));
        EXPECT_TRUE(std::isinf(static_cast<float>(Half::CreateInfinity())));
        EXPECT_FLOAT_EQ(static_cast<float>(Half::CreateMax()), 65504.0f);
    }

    //! The strongest available statement about the conversion: it is checked for every one of the
    //! 65536 representable values, against an independent reconstruction from the IEEE fields.
    //! This subsumes any hand-picked normal-range case, so those are deliberately not duplicated.
    TEST(MATH_Half, ExhaustiveRoundTripAndValue)
    {
        int roundTripFailures = 0;
        int valueFailures = 0;
        int subnormalsChecked = 0;

        for (u32 i = 0; i < 65536u; ++i)
        {
            const auto bits = static_cast<u16>(i);
            const float asFloat = Half::ToFloat(bits);
            const u16 backToBits = Half::FromFloat(asFloat);

            const bool isNaN = (bits & 0x7FFF) > 0x7C00;
            if (isNaN)
            {
                // A NaN need not preserve its payload, but it must stay a NaN.
                if ((backToBits & 0x7FFF) <= 0x7C00)
                {
                    ++roundTripFailures;
                }
                continue;
            }

            if (backToBits != bits)
            {
                ++roundTripFailures;
            }

            const u32 exponent = (bits >> 10) & 0x1F;
            const u32 mantissa = bits & 0x3FF;
            const float sign = (bits & 0x8000) ? -1.0f : 1.0f;
            float expected;
            if (exponent == 0)
            {
                expected = sign * std::ldexp(static_cast<float>(mantissa), -24);
                if (mantissa != 0)
                {
                    ++subnormalsChecked;
                }
            }
            else if (exponent == 0x1F)
            {
                expected = sign * g_halfTestInfinity;
            }
            else
            {
                expected =
                    sign * std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, static_cast<int>(exponent) - 15);
            }

            if (asFloat != expected)
            {
                ++valueFailures;
            }
        }

        EXPECT_EQ(roundTripFailures, 0);
        EXPECT_EQ(valueFailures, 0);
        EXPECT_EQ(subnormalsChecked, 2046) << "2 signs x 1023 non-zero subnormal mantissas";
    }

    //! Conversion must be monotonic: ordering the non-negative bit patterns must order the values.
    //! A rounding or exponent bug that the exhaustive value check somehow missed would break this.
    TEST(MATH_Half, IsMonotonicOverNonNegativeRange)
    {
        int inversions = 0;
        for (u16 bits = 0; bits < 0x7C00; ++bits) // zero up to, but excluding, infinity
        {
            if (!(Half::ToFloat(bits) < Half::ToFloat(static_cast<u16>(bits + 1))))
            {
                ++inversions;
            }
        }
        EXPECT_EQ(inversions, 0);
    }

    TEST(MATH_Half, EqualityIsBitwise)
    {
        // Documented as bit equality rather than IEEE equality, so pin both surprising consequences.
        EXPECT_TRUE(Half::CreateNaN() == Half::CreateNaN()) << "bitwise, so a NaN equals itself";
        EXPECT_TRUE(Half(0.0f) != Half(-0.0f)) << "bitwise, so +0 does not equal -0";
        EXPECT_FLOAT_EQ(static_cast<float>(Half(0.0f)), static_cast<float>(Half(-0.0f)))
            << "...but they compare equal once converted to float";
    }
} // namespace UnitTest
