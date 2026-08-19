/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Utils/NoDestructor.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>
#include <AzCore/std/typetraits/typetraits.h>

#include <cstdint>

namespace UnitTest
{
    namespace
    {
        struct alignas(64) AlignedValue final
        {
            explicit AlignedValue(const int value)
                : m_value{value}
            {
            }

            int m_value;
        };

        struct CountedValue final
        {
            CountedValue(
                int& constructionCount,
                int& destructionCount)
                : m_destructionCount{destructionCount}
            {
                ++constructionCount;
            }

            ~CountedValue()
            {
                ++m_destructionCount;
            }

            int& m_destructionCount;
        };
    } // namespace

    TEST(NoDestructorTests, IsTriviallyDestructibleAndNotCopyable)
    {
        static_assert(AZStd::is_trivially_destructible_v<AZ::NoDestructor<CountedValue>>);
        static_assert(!AZStd::is_copy_constructible_v<AZ::NoDestructor<CountedValue>>);
        static_assert(!AZStd::is_move_constructible_v<AZ::NoDestructor<CountedValue>>);
        SUCCEED();
    }

    TEST(NoDestructorTests, ForwardsConstructionAndPreservesAlignment)
    {
        AZ::NoDestructor<AlignedValue> value{42};

        EXPECT_EQ(value->m_value, 42);
        EXPECT_EQ((*value).m_value, 42);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(&value.Get()) % alignof(AlignedValue), 0);
    }

    TEST(NoDestructorTests, DoesNotInvokeContainedDestructor)
    {
        int constructionCount = 0;
        int destructionCount = 0;
        {
            AZ::NoDestructor<CountedValue> value{constructionCount, destructionCount};
            EXPECT_EQ(constructionCount, 1);
        }

        EXPECT_EQ(destructionCount, 0);
    }
} // namespace UnitTest
