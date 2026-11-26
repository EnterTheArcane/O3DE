/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Half.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/containers/unordered_set.h>
#include <cfenv>
#include <cmath>
#include <type_traits>

#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace UnitTest
{
    namespace
    {
        using AZ::Half;
        enum class EncodedEnum : AZ::u16 {};
        enum UnscopedEncodedEnum { EncodedOne = 1 };
        struct ConvertibleToFloat { operator float() const; };
        struct ConvertibleToDouble { operator double() const; };
        struct ConvertibleToInteger { operator int() const; };

        static_assert(sizeof(Half) == 2 && alignof(Half) == 2);
        static_assert(std::is_standard_layout_v<Half> && std::is_trivially_copyable_v<Half>);
        static_assert(!std::is_arithmetic_v<Half> && !std::is_floating_point_v<Half>);
        static_assert(std::is_convertible_v<Half, float> && !std::is_convertible_v<float, Half>);
        static_assert(!std::is_assignable_v<Half&, float>);
        static_assert(std::is_constructible_v<Half, double> && std::is_constructible_v<Half, AZ::s64>);
        static_assert(!std::is_constructible_v<Half, bool> && !std::is_constructible_v<Half, long double>);
        static_assert(!std::is_constructible_v<Half, EncodedEnum> && !std::is_constructible_v<Half, ConvertibleToFloat>);
        static_assert(!std::is_constructible_v<Half, UnscopedEncodedEnum>);
        static_assert(!std::is_constructible_v<Half, ConvertibleToDouble> && !std::is_constructible_v<Half, ConvertibleToInteger>);
        static_assert(std::is_nothrow_constructible_v<Half, float> && std::is_nothrow_constructible_v<Half, double>);
        static_assert(std::is_same_v<decltype(Half{} + Half{}), float>);
        static_assert(std::is_same_v<decltype(Half{} - Half{}), float>);
        static_assert(std::is_same_v<decltype(Half{} * Half{}), float>);
        static_assert(std::is_same_v<decltype(Half{} / Half{}), float>);
        static_assert(std::is_same_v<decltype(2 + Half{}), float>);
        static_assert(std::is_same_v<decltype(Half{} + 2.0), double>);
        static_assert(std::is_same_v<decltype(2.0L + Half{}), long double>);
        static_assert(std::is_same_v<decltype(+Half{}), Half>);
        static_assert(std::is_same_v<decltype(-Half{}), Half>);
        static_assert(Half::Zero.GetBits() == 0 && Half::One.GetBits() == 0x3c00);
        static_assert(Half(1.00048828125).GetBits() == 0x3c00);
        static_assert(Half(1.0004882812500002).GetBits() == 0x3c01);
        static_assert(Half(-0.0).GetBits() == 0x8000);
        static_assert(Half(AZStd::numeric_limits<AZ::s64>::min()).GetBits() == 0xfc00);
        static_assert(Half(AZStd::numeric_limits<AZ::u64>::max()).GetBits() == 0x7c00);
        static_assert(AZStd::numeric_limits<Half>::digits == 11);
        static_assert(AZStd::numeric_limits<Half>::min().GetBits() == 0x0400);
        static_assert(AZStd::numeric_limits<Half>::denorm_min().GetBits() == 1);
        static_assert(!AZStd::numeric_limits<Half>::is_iec559);

        constexpr Half ConstexprCompound()
        {
            Half value(1);
            value += Half(2);
            value -= 1.0f;
            value *= 3.0;
            value /= 2;
            return value;
        }
        static_assert(ConstexprCompound().GetBits() == Half(3).GetBits());
        static_assert(Half(2048) + Half(1) - Half(2048) == 1.0f);
        static_assert((-Half::FromBits(0x7d01)).GetBits() == 0xfd01);

        // Independent arithmetic reference, without production bit-shifting or normalization algorithms.
        double ReferenceDecode(AZ::u16 bits)
        {
            const int exponent = (bits >> 10) & 31;
            const int fraction = bits & 1023;
            double magnitude;
            if (exponent == 31)
            {
                magnitude = std::numeric_limits<double>::infinity();
                if (fraction != 0)
                {
                    magnitude = std::numeric_limits<double>::quiet_NaN();
                }
            }
            else if (exponent == 0)
            {
                magnitude = std::ldexp(static_cast<double>(fraction), -24);
            }
            else
            {
                magnitude = std::ldexp(static_cast<double>(1024 + fraction), exponent - 25);
            }
            if ((bits & 0x8000) != 0)
            {
                return -magnitude;
            }
            return magnitude;
        }

        AZ::u16 ReferenceEncode(double value)
        {
            AZ::u16 sign = 0;
            if (std::signbit(value))
            {
                sign = 0x8000;
            }
            const double magnitude = std::abs(value);
            if (std::isnan(value))
            {
                return static_cast<AZ::u16>(sign | 0x7e00);
            }
            if (magnitude >= 65520.0)
            {
                return static_cast<AZ::u16>(sign | 0x7c00);
            }
            AZ::u16 lower = 0;
            AZ::u16 upper = 0x7bff;
            while (lower < upper)
            {
                const auto middle = static_cast<AZ::u16>((lower + upper + 1) / 2);
                if (ReferenceDecode(middle) <= magnitude)
                {
                    lower = middle;
                }
                else
                {
                    upper = static_cast<AZ::u16>(middle - 1);
                }
            }
            if (lower < 0x7bff)
            {
                const double midpoint = (ReferenceDecode(lower) + ReferenceDecode(static_cast<AZ::u16>(lower + 1))) * 0.5;
                if (magnitude > midpoint || (magnitude == midpoint && (lower & 1) != 0))
                {
                    ++lower;
                }
            }
            return static_cast<AZ::u16>(sign | lower);
        }

        class ScopedFloatingPointState
        {
        public:
            ScopedFloatingPointState()
            {
                std::fegetenv(&m_environment);
#if defined(__SSE2__) || defined(_M_X64)
                m_mxcsr = _mm_getcsr();
#endif
            }
            ~ScopedFloatingPointState()
            {
                std::fesetenv(&m_environment);
#if defined(__SSE2__) || defined(_M_X64)
                _mm_setcsr(m_mxcsr);
#endif
            }
        private:
            std::fenv_t m_environment{};
#if defined(__SSE2__) || defined(_M_X64)
            unsigned int m_mxcsr{};
#endif
        };
    }

    TEST(MATH_Half, DecodeEveryEncodingAndPreserveRawBits)
    {
        for (AZ::u32 bits = 0; bits < 65536; ++bits)
        {
            const auto raw = static_cast<AZ::u16>(bits);
            const Half value = Half::FromBits(raw);
            const Half copied = value;
            ASSERT_EQ(copied.GetBits(), raw);
            ASSERT_EQ((+value).GetBits(), raw);
            ASSERT_EQ((-value).GetBits(), raw ^ 0x8000);
            const auto expected = static_cast<float>(ReferenceDecode(raw));
            const float actual = value.ToFloat();
            if (std::isnan(expected))
            {
                ASSERT_TRUE(std::isnan(actual)) << bits;
                ASSERT_NE(AZStd::bit_cast<AZ::u32>(actual) & 0x00400000, 0);
            }
            else
            {
                ASSERT_EQ(AZStd::bit_cast<AZ::u32>(actual), AZStd::bit_cast<AZ::u32>(expected)) << bits;
                ASSERT_EQ(Half(actual).GetBits(), raw);
                ASSERT_EQ(Half(static_cast<double>(actual)).GetBits(), raw);
            }
            ASSERT_EQ(value.IsZero(), (bits & 0x7fff) == 0);
            ASSERT_EQ(value.IsNegative(), (bits & 0x8000) != 0);
            ASSERT_EQ(value.IsFinite(), std::isfinite(expected));
            ASSERT_EQ(value.IsInfinity(), std::isinf(expected));
            ASSERT_EQ(value.IsNaN(), std::isnan(expected));
            ASSERT_EQ(value.IsNormal(), ((bits & 0x7c00) != 0) && ((bits & 0x7c00) != 0x7c00));
            ASSERT_EQ(value.IsSubnormal(), ((bits & 0x7c00) == 0) && ((bits & 0x03ff) != 0));
        }
    }

    TEST(MATH_Half, EveryRoundingBoundaryAndFloatAndDoubleNeighbors)
    {
        for (AZ::u16 lower = 0; lower <= 0x7bff; ++lower)
        {
            double upperValue = 65536.0;
            if (lower != 0x7bff)
            {
                upperValue = ReferenceDecode(static_cast<AZ::u16>(lower + 1));
            }
            const double midpoint = (ReferenceDecode(lower) + upperValue) * 0.5;
            const double doubles[] = {std::nextafter(midpoint, 0.0), midpoint, std::nextafter(midpoint, INFINITY)};
            const float middleFloat = static_cast<float>(midpoint);
            const float floats[] = {std::nextafter(middleFloat, 0.0f), middleFloat, std::nextafter(middleFloat, INFINITY)};
            for (const double input : doubles)
            {
                ASSERT_EQ(Half(input).GetBits(), ReferenceEncode(input)) << lower << " " << input;
                ASSERT_EQ(Half(-input).GetBits(), ReferenceEncode(-input)) << lower << " " << input;
            }
            for (const float input : floats)
            {
                ASSERT_EQ(Half(input).GetBits(), ReferenceEncode(input)) << lower << " " << input;
                ASSERT_EQ(Half(-input).GetBits(), ReferenceEncode(-input)) << lower << " " << input;
            }
        }
    }

    TEST(MATH_Half, DoubleNarrowingAvoidsFloatDoubleRounding)
    {
        const double aboveEvenMidpoint = std::nextafter(1.00048828125, 2.0);
        const double belowOddMidpoint = std::nextafter(1.00146484375, 1.0);
        EXPECT_EQ(Half(aboveEvenMidpoint).GetBits(), 0x3c01);
        EXPECT_EQ(Half(belowOddMidpoint).GetBits(), 0x3c01);
        EXPECT_EQ(Half(static_cast<float>(aboveEvenMidpoint)).GetBits(), 0x3c00);
        EXPECT_EQ(Half(static_cast<float>(belowOddMidpoint)).GetBits(), 0x3c02);
    }

    TEST(MATH_Half, IntegersAndExceptionalValues)
    {
        for (AZ::s32 value = -65521; value <= 65521; ++value)
        {
            ASSERT_EQ(Half(value).GetBits(), ReferenceEncode(value)) << value;
            ASSERT_EQ(Half(static_cast<AZ::s64>(value)).GetBits(), ReferenceEncode(value)) << value;
        }
        EXPECT_EQ(Half(std::numeric_limits<AZ::s64>::min()).GetBits(), 0xfc00);
        EXPECT_EQ(Half(std::numeric_limits<AZ::s64>::max()).GetBits(), 0x7c00);
        EXPECT_EQ(Half(std::numeric_limits<AZ::u64>::max()).GetBits(), 0x7c00);
        EXPECT_EQ(Half(static_cast<AZ::u8>(255)).ToFloat(), 255.0f);
        EXPECT_EQ(Half(static_cast<AZ::s8>(-128)).ToFloat(), -128.0f);
        EXPECT_TRUE(Half(std::numeric_limits<float>::infinity()).IsInfinity());
        EXPECT_TRUE(Half(std::numeric_limits<double>::infinity()).IsInfinity());
        const AZ::u64 nanBits[] = {0x7ff0000000000001ULL, 0xfff8000000000000ULL};
        for (const auto bits : nanBits)
        {
            const Half encoded(AZStd::bit_cast<double>(bits));
            EXPECT_TRUE(encoded.IsNaN());
            EXPECT_NE(encoded.GetBits() & 0x0200, 0);
        }
        EXPECT_TRUE(Half(AZStd::bit_cast<float>(AZ::u32{0x7f800001})).IsNaN());
    }

    TEST(MATH_Half, CompoundsRoundOnceAfterPromotedArithmetic)
    {
        Half value(2048);
        value += Half(1);
        EXPECT_EQ(value.ToFloat(), 2048.0f);
        EXPECT_EQ(Half(2048) + Half(1) - Half(2048), 1.0f);
        value = Half(1);
        EXPECT_EQ(&(value += 0.0004882812500002), &value);
        EXPECT_EQ(value.GetBits(), 0x3c01);
        value -= Half(0.5f);
        EXPECT_EQ(value.GetBits(), Half(Half::FromBits(0x3c01).ToFloat() - 0.5f).GetBits());
        value *= 3;
        value /= 2.0;
        EXPECT_EQ(value.GetBits(), Half((Half::FromBits(0x3c01).ToFloat() - 0.5f) * 3.0f / 2.0).GetBits());
        value = Half(1);
        volatile float zero = 0.0f;
        value /= zero;
        EXPECT_TRUE(value.IsInfinity());
        value *= zero;
        EXPECT_TRUE(value.IsNaN());
    }

    TEST(MATH_Half, NumericComparisons)
    {
        const Half positiveZero;
        const Half negativeZero = Half::FromBits(0x8000);
        EXPECT_EQ(positiveZero, negativeZero);
        EXPECT_EQ(AZStd::hash<Half>{}(positiveZero), AZStd::hash<Half>{}(negativeZero));
        const Half nan = Half::FromBits(0x7d01);
        EXPECT_FALSE(nan == nan);
        EXPECT_TRUE(nan != nan);
        EXPECT_FALSE(nan < Half::One);
        EXPECT_FALSE(nan > Half::One);
        EXPECT_FALSE(nan <= Half::One);
        EXPECT_FALSE(nan >= Half::One);
        EXPECT_LT(Half(-2), Half(-1));
    }

    TEST(MATH_Half, ConversionIgnoresRoundingModesAndFlushSettings)
    {
        ScopedFloatingPointState restoreState;
        const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
        for (const int mode : modes)
        {
            ASSERT_EQ(std::fesetround(mode), 0);
            for (unsigned int flush = 0; flush < 2; ++flush)
            {
#if defined(__SSE2__) || defined(_M_X64)
                unsigned int control = _mm_getcsr() & ~0x8040u;
                if (flush != 0)
                {
                    control |= 0x8040u;
                }
                _mm_setcsr(control);
#endif
                volatile double aboveMidpoint = 1.0004882812500002;
                volatile float minimumSubnormal = 0x1p-24f;
                volatile float belowMinimumSubnormalMidpoint = 0x1p-26f;
                EXPECT_EQ(Half(aboveMidpoint).GetBits(), 0x3c01);
                EXPECT_EQ(Half(minimumSubnormal).GetBits(), 1);
                EXPECT_EQ(Half(belowMinimumSubnormalMidpoint).GetBits(), 0);
                EXPECT_EQ(Half::FromBits(1).ToFloat(), minimumSubnormal);
                EXPECT_EQ(Half(AZStd::bit_cast<float>(AZ::u32{1})).GetBits(), 0);
                EXPECT_EQ(Half(AZStd::bit_cast<double>(AZ::u64{1})).GetBits(), 0);
                EXPECT_EQ(std::fegetround(), mode);
#if defined(__SSE2__) || defined(_M_X64)
                EXPECT_EQ(_mm_getcsr() & ~0x3fu, control & ~0x3fu);
#endif
                volatile float input = 0.3333f;
                Half actual(1);
                actual += input;
                volatile float expected = 1.0f + input;
                EXPECT_EQ(actual.GetBits(), Half(expected).GetBits());
            }
        }
    }

    using HalfContainerTests = LeakDetectionFixture;
    TEST_F(HalfContainerTests, HashDeduplicatesSignedZeros)
    {
        AZStd::unordered_set<Half> values;
        values.insert(Half{});
        values.insert(Half::FromBits(0x8000));
        values.insert(Half(1));
        EXPECT_EQ(values.size(), 2);
        EXPECT_NE(values.find(Half(1)), values.end());
    }
}
