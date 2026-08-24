/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Utils/NoDestructor.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>
#include <AzCore/std/typetraits/is_destructible.h>

namespace UnitTest
{
    namespace
    {
        struct TrivialValue final
        {
            int m_value;
        };

        struct ForwardedValue final
        {
            ForwardedValue(
                int& source,
                int&& movedValue)
                : m_source{&source}
                , m_value{movedValue}
            {
            }

            int* m_source;
            int m_value;
        };

        struct alignas(64) AlignedValue final
        {
            int m_value = 0;
        };

        struct DestructionTracker final
        {
            explicit DestructionTracker(int& destructionCount)
                : m_destructionCount{destructionCount}
            {
            }

            ~DestructionTracker()
            {
                ++m_destructionCount;
            }

            int& m_destructionCount;
        };

        struct InitializationTracker final
        {
            explicit InitializationTracker(AZStd::atomic_int& initializationCount)
            {
                ++initializationCount;
            }
        };

        InitializationTracker& GetInitializedOnce(AZStd::atomic_int& initializationCount)
        {
            static AZ::NoDestructor<InitializationTracker> value{initializationCount};
            return value.Get();
        }
    } // namespace

    TEST(NoDestructorTests, TrivialValueUsesDirectMutableAndConstAccess)
    {
        static_assert(AZStd::is_trivially_destructible_v<AZ::NoDestructor<TrivialValue>>);

        AZ::NoDestructor<TrivialValue> value{TrivialValue{17}};
        value->m_value = 42;
        EXPECT_EQ((*value).m_value, 42);

        const AZ::NoDestructor<TrivialValue>& constValue = value;
        EXPECT_EQ(constValue.Get().m_value, 42);
        EXPECT_EQ(constValue->m_value, 42);
    }

    TEST(NoDestructorTests, ConstructionPerfectForwardsArguments)
    {
        int source = 11;
        AZ::NoDestructor<ForwardedValue> value{source, 29};

        EXPECT_EQ(value->m_source, &source);
        EXPECT_EQ(value->m_value, 29);
    }

    TEST(NoDestructorTests, StoragePreservesContainedAlignment)
    {
        AZ::NoDestructor<AlignedValue> value;
        const uintptr_t address = reinterpret_cast<uintptr_t>(&value.Get());

        EXPECT_EQ(address % alignof(AlignedValue), 0);
    }

    TEST(NoDestructorTests, NonTrivialContainedDestructorIsSuppressed)
    {
        static_assert(AZStd::is_trivially_destructible_v<AZ::NoDestructor<DestructionTracker>>);

        int destructionCount = 0;
        {
            AZ::NoDestructor<DestructionTracker> value{destructionCount};
            EXPECT_EQ(&value->m_destructionCount, &destructionCount);
        }

        EXPECT_EQ(destructionCount, 0);
    }

    TEST(NoDestructorTests, FunctionLocalStaticInitializesExactlyOnceAcrossThreads)
    {
        constexpr size_t ThreadCount = 16;
        AZStd::atomic_int initializationCount{0};
        AZStd::vector<AZStd::thread> threads;
        threads.reserve(ThreadCount);

        for (size_t index = 0; index < ThreadCount; ++index)
        {
            threads.emplace_back([&initializationCount]()
            {
                (void)GetInitializedOnce(initializationCount);
            });
        }
        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        EXPECT_EQ(initializationCount.load(), 1);
    }
} // namespace UnitTest
