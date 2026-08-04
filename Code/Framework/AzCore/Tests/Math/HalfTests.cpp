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
        const float HalfTestNaN = AZStd::numeric_limits<float>::quiet_NaN();
        constexpr float HalfTestInfinity = AZStd::numeric_limits<float>::infinity();
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
        static_assert(static_cast<float>(Half::One) == 1.0f);
    }

    //! The sign of zero has to survive, because -0.0f and +0.0f have different bit patterns
    //! and a naive conversion that goes through a comparison against zero would collapse them.
    TEST(MATH_Half, SignedZero)
    {
        static_assert(Half::FromFloat(0.0f) == 0x0000);
        static_assert(Half::FromFloat(-0.0f) == 0x8000);
        static_assert(Half::Zero.IsZero());
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

    //! The subnormal/normal transition is where the exponent encoding changes meaning,
    //! so both sides of it are checked explicitly.
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

    //! The exhaustive sweep below walks the half domain, so it never feeds a *float* NaN or infinity into FromFloat.
    //! Those take a separate early-out branch, so cover them directly.
    TEST(MATH_Half, FloatDomainNaNAndInfinity)
    {
        EXPECT_TRUE(Half(HalfTestNaN).IsNaN());
        EXPECT_TRUE(Half(-HalfTestNaN).IsNaN());
        EXPECT_TRUE(Half(-HalfTestNaN).IsNegative()) << "a NaN's sign bit is carried through";
        EXPECT_FALSE(Half(HalfTestNaN).IsInfinity());

        EXPECT_EQ(Half::FromFloat(HalfTestInfinity), 0x7C00);
        EXPECT_EQ(Half::FromFloat(-HalfTestInfinity), 0xFC00);
        EXPECT_TRUE(Half(HalfTestInfinity).IsInfinity());
        EXPECT_FALSE(Half(HalfTestInfinity).IsNaN());

        // A NaN with a small payload must not be truncated into an infinity.
        const float smallPayloadNaN = AZStd::bit_cast<float>(0x7F800001u);
        EXPECT_TRUE(Half(smallPayloadNaN).IsNaN()) << "mantissa truncation must not turn NaN into Inf";
    }

    TEST(MATH_Half, Classification)
    {
        static_assert(Half::Infinity.IsInfinity() && !Half::Infinity.IsNaN());
        static_assert(Half::NaN.IsNaN() && !Half::NaN.IsInfinity());
        static_assert(Half::Max.IsFinite());
        static_assert(!Half::Infinity.IsFinite() && !Half::NaN.IsFinite());
        EXPECT_TRUE(std::isnan(static_cast<float>(Half::NaN)));
        EXPECT_TRUE(std::isinf(static_cast<float>(Half::Infinity)));
        EXPECT_FLOAT_EQ(static_cast<float>(Half::Max), 65504.0f);
    }

    //! The strongest available statement about the conversion:
    //! it is checked for every one of the 65536 representable values, against an independent reconstruction from the IEEE fields.
    //! This subsumes any hand-picked normal-range case, so those are deliberately not duplicated.
    TEST(MATH_Half, ExhaustiveRoundTripAndValue)
    {
        int roundTripFailures = 0;
        int valueFailures = 0;
        int pathDisagreements = 0;
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

            // The runtime path (hardware where enabled) and the software reference must agree.
            if (AZStd::bit_cast<u32>(Internal::HalfToFloat(bits)) != AZStd::bit_cast<u32>(asFloat)
                || Internal::FloatToHalf(asFloat) != backToBits)
            {
                ++pathDisagreements;
            }

            const u32 exponent = (bits >> 10) & 0x1F;
            const u32 mantissa = bits & 0x3FF;
            float sign = 1.0f;
            if ((bits & 0x8000) != 0)
            {
                sign = -1.0f;
            }
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
                expected = sign * HalfTestInfinity;
            }
            else
            {
                expected = sign * std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, static_cast<int>(exponent) - 15);
            }

            if (asFloat != expected)
            {
                ++valueFailures;
            }
        }

        EXPECT_EQ(roundTripFailures, 0);
        EXPECT_EQ(valueFailures, 0);
        EXPECT_EQ(pathDisagreements, 0);
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

    //! Every comparison and arithmetic operator reaches float through the implicit conversion,
    //! and operator== is defined to match, so the whole surface has one set of semantics.
    TEST(MATH_Half, EqualityMatchesIeeeFloatSemantics)
    {
        static_assert(Half(0.0f) == Half(-0.0f), "+0 equals -0");
        static_assert(Half(0.0f).GetBits() != Half(-0.0f).GetBits(), "...even though their bit patterns differ");
        EXPECT_TRUE(Half::NaN != Half::NaN) << "a NaN does not equal itself";
        EXPECT_TRUE(Half(0.5f) == 0.5f) << "mixed comparison goes through float";
        EXPECT_TRUE(Half(0.25f) < Half(0.5f)) << "relational operators go through float";
        EXPECT_TRUE(Half(1.0f) + Half(1.0f) == 2.0f) << "arithmetic goes through float";

        // +0 and -0 compare equal, so they must hash equal.
        static_assert(AZStd::hash<Half>{}(Half(0.0f)) == AZStd::hash<Half>{}(Half(-0.0f)));
        static_assert(AZStd::hash<Half>{}(Half(0.5f)) != AZStd::hash<Half>{}(Half(0.25f)));
    }

    TEST(MATH_Half, NumericLimits)
    {
        static_assert(AZStd::numeric_limits<Half>::is_specialized);
        static_assert(AZStd::numeric_limits<Half>::is_iec559);
        static_assert(AZStd::numeric_limits<Half>::digits == 11);
        static_assert(AZStd::numeric_limits<Half>::max().GetBits() == Half::Max.GetBits());
        static_assert(AZStd::numeric_limits<Half>::infinity().IsInfinity());
        static_assert(AZStd::numeric_limits<Half>::quiet_NaN().IsNaN());
        static_assert(AZStd::numeric_limits<Half>::lowest() == Half(-65504.0f));

        EXPECT_EQ(static_cast<float>(AZStd::numeric_limits<Half>::max()), 65504.0f);
        EXPECT_EQ(static_cast<float>(AZStd::numeric_limits<Half>::min()), std::ldexp(1.0f, -14));
        EXPECT_EQ(static_cast<float>(AZStd::numeric_limits<Half>::epsilon()), std::ldexp(1.0f, -10));
        EXPECT_EQ(static_cast<float>(AZStd::numeric_limits<Half>::denorm_min()), std::ldexp(1.0f, -24));
    }

    //! The runtime conversions may use hardware instructions.
    //! The constant-evaluated conversions always use the software reference.
    //! Both must produce the same bits.
    //! NaNs only have to agree in class, since hardware NaN payload behavior is not contractual.
    TEST(MATH_Half, RuntimeConversionMatchesSoftwareReference)
    {
        int mismatches = 0;

        // The boundary table: ties, range edges, signed zero and NaN.
        const float boundaryValues[] = {
            0.0f, -0.0f,
            65504.0f, 65519.0f,
            65520.0f, -65520.0f,
            std::ldexp(1.0f, -24),
            std::ldexp(1.0f, -25),
            std::ldexp(1.0f, -14),
            std::ldexp(1023.0f, -24),
            1.0f + 0.00048828125f,
            HalfTestInfinity,
            -HalfTestInfinity,
            HalfTestNaN,
        };
        for (float value : boundaryValues)
        {
            const u16 runtimeBits = Half::FromFloat(value);
            const u16 softwareBits = Internal::FloatToHalf(value);
            if (runtimeBits != softwareBits)
            {
                ++mismatches;
            }
        }

        // A seeded sweep over arbitrary float bit patterns.
        u32 state = 0x12345678u;
        for (int i = 0; i < 1000000; ++i)
        {
            state = state * 1664525u + 1013904223u;
            const float value = AZStd::bit_cast<float>(state);
            const u16 runtimeBits = Half::FromFloat(value);
            const u16 softwareBits = Internal::FloatToHalf(value);
            if (runtimeBits != softwareBits)
            {
                const bool bothNaN = ((runtimeBits & 0x7FFF) > 0x7C00) && ((softwareBits & 0x7FFF) > 0x7C00);
                if (!bothNaN)
                {
                    ++mismatches;
                }
            }
        }
        EXPECT_EQ(mismatches, 0);
    }

    //! The full float domain, for manual runs: all 2^32 float bit patterns through both conversion paths.
    //! Takes a few seconds optimized. Enable with --gtest_also_run_disabled_tests.
    TEST(MATH_Half, DISABLED_ExhaustiveFloatDomainConversion)
    {
        unsigned long long mismatches = 0;
        u32 bits = 0;
        do
        {
            const float value = AZStd::bit_cast<float>(bits);
            const u16 runtimeBits = Half::FromFloat(value);
            const u16 softwareBits = Internal::FloatToHalf(value);
            if (runtimeBits != softwareBits)
            {
                const bool bothNaN = ((runtimeBits & 0x7FFF) > 0x7C00) && ((softwareBits & 0x7FFF) > 0x7C00);
                if (!bothNaN)
                {
                    ++mismatches;
                }
            }
            ++bits;
        } while (bits != 0);
        EXPECT_EQ(mismatches, 0ull);
    }
} // namespace UnitTest
