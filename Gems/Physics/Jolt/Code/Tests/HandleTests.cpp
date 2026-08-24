/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HandleSlotReservation.h>

#include <AzTest/AzTest.h>

#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>
#include <AzCore/std/sort.h>

namespace Jolt
{
    namespace
    {
        struct TestHandleSlot final
        {
            AZ::u32 m_generation = 0;
            AZ::u32 m_payload = 0;
        };
    } // namespace

    TEST(HandleTests, DefaultHandleIsInvalid)
    {
        const WorldHandle handle;

        EXPECT_FALSE(handle);
        EXPECT_EQ(handle, WorldHandle::Invalid);
    }

    TEST(HandleTests, ValueRoundTripsWithoutExposingNativeIdentifiers)
    {
        constexpr AZ::u64 value = 0x1234'5678'9abc'def0;
        constexpr BodyHandle handle = Internal::HandleAccess::FromValue<BodyHandle>(value);

        static_assert(handle);
        static_assert(Internal::HandleAccess::ToValue(handle) == value);
        EXPECT_TRUE(handle);
    }

    TEST(HandleTests, InvalidHandleKindsDoNotAcquireProductionGenerations)
    {
        EXPECT_EQ(Internal::AcquireHandleGeneration(Internal::HandleKind::None), 0);
        EXPECT_EQ(Internal::AcquireHandleGeneration(Internal::HandleKind::Count), 0);
    }

    TEST(HandleTests, AtomicGenerationSourceReturnsSequentialGenerations)
    {
        Internal::AtomicGenerationSource generationSource;

        EXPECT_EQ(generationSource.Acquire(), 1);
        EXPECT_EQ(generationSource.Acquire(), 2);
        EXPECT_EQ(generationSource.Acquire(), 3);
    }

    TEST(HandleTests, AtomicGenerationSourceExhaustsWithoutWrapping)
    {
        Internal::AtomicGenerationSource generationSource(AZStd::numeric_limits<AZ::u32>::max() - 1);

        EXPECT_EQ(generationSource.Acquire(), AZStd::numeric_limits<AZ::u32>::max() - 1);
        EXPECT_EQ(generationSource.Acquire(), AZStd::numeric_limits<AZ::u32>::max());
        EXPECT_EQ(generationSource.Acquire(), 0);
        EXPECT_EQ(generationSource.Acquire(), 0);
    }

    TEST(HandleTests, AtomicGenerationSourceIsExactlyUniqueUnderContention)
    {
        constexpr size_t ThreadCount = 8;
        constexpr size_t GenerationsPerThread = 2'048;
        constexpr size_t GenerationCount = ThreadCount * GenerationsPerThread;

        Internal::AtomicGenerationSource generationSource;
        AZStd::array<AZStd::array<AZ::u32, GenerationsPerThread>, ThreadCount> threadGenerations;
        AZStd::array<AZStd::thread, ThreadCount> threads;
        AZStd::atomic<size_t> readyThreadCount{0};
        AZStd::atomic_bool start{false};

        for (size_t threadIndex = 0; threadIndex < ThreadCount; ++threadIndex)
        {
            threads[threadIndex] = AZStd::thread(
                [&generationSource, &threadGenerations, &readyThreadCount, &start, threadIndex]()
                {
                    readyThreadCount.fetch_add(1, AZStd::memory_order_release);
                    while (!start.load(AZStd::memory_order_acquire))
                    {
                        AZStd::this_thread::yield();
                    }

                    for (AZ::u32& generation : threadGenerations[threadIndex])
                    {
                        generation = generationSource.Acquire();
                    }
                });
        }

        while (readyThreadCount.load(AZStd::memory_order_acquire) != ThreadCount)
        {
            AZStd::this_thread::yield();
        }
        start.store(true, AZStd::memory_order_release);

        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        AZStd::vector<AZ::u32> generations;
        generations.reserve(GenerationCount);
        for (const auto& generated : threadGenerations)
        {
            generations.insert(generations.end(), generated.begin(), generated.end());
        }
        AZStd::sort(generations.begin(), generations.end());

        ASSERT_EQ(generations.size(), GenerationCount);
        for (size_t generationIndex = 0; generationIndex < GenerationCount; ++generationIndex)
        {
            ASSERT_EQ(generations[generationIndex], static_cast<AZ::u32>(generationIndex + 1));
        }
        EXPECT_EQ(generationSource.Acquire(), static_cast<AZ::u32>(GenerationCount + 1));
    }

    TEST(HandleTests, InjectedGenerationSourcesAreIndependentForAllHandleKinds)
    {
        Internal::AtomicGenerationSources generationSources;
        const auto expectIndependentSource = [&generationSources]<typename HandleType>()
        {
            EXPECT_EQ(Internal::AcquireHandleGeneration<HandleType>(generationSources), 1);
            EXPECT_EQ(Internal::AcquireHandleGeneration<HandleType>(generationSources), 2);
        };

        expectIndependentSource.template operator()<BodyHandle>();
        expectIndependentSource.template operator()<CharacterHandle>();
        expectIndependentSource.template operator()<ConstraintHandle>();
        expectIndependentSource.template operator()<CookedShapeHandle>();
        expectIndependentSource.template operator()<ExtensionHandle>();
        expectIndependentSource.template operator()<GroupFilterHandle>();
        expectIndependentSource.template operator()<HairHandle>();
        expectIndependentSource.template operator()<HairDefinitionHandle>();
        expectIndependentSource.template operator()<MaterialHandle>();
        expectIndependentSource.template operator()<PathHandle>();
        expectIndependentSource.template operator()<RagdollHandle>();
        expectIndependentSource.template operator()<RagdollDefinitionHandle>();
        expectIndependentSource.template operator()<SceneDefinitionHandle>();
        expectIndependentSource.template operator()<SceneInstanceHandle>();
        expectIndependentSource.template operator()<ShapeHandle>();
        expectIndependentSource.template operator()<SkeletalAnimationHandle>();
        expectIndependentSource.template operator()<SkeletonDefinitionHandle>();
        expectIndependentSource.template operator()<SkeletonMapperHandle>();
        expectIndependentSource.template operator()<SkeletonPoseHandle>();
        expectIndependentSource.template operator()<StateSnapshotHandle>();
        expectIndependentSource.template operator()<SoftBodyDefinitionHandle>();
        expectIndependentSource.template operator()<VehicleHandle>();
        expectIndependentSource.template operator()<VirtualCharacterHandle>();
        expectIndependentSource.template operator()<WorldHandle>();
    }

    TEST(HandleTests, SlotReservationRollbackRestoresAppendedAndReusedStructureExactly)
    {
        AZStd::vector<TestHandleSlot> slots(3);
        slots[1].m_payload = 17;
        AZStd::vector<AZ::u32> freeSlots = {0, 2};
        Internal::AtomicGenerationSources generationSources;

        const Internal::HandleSlotReservation first = Internal::ReserveHandleSlot<BodyHandle>(
            slots,
            freeSlots,
            5,
            generationSources);
        const Internal::HandleSlotReservation second = Internal::ReserveHandleSlot<BodyHandle>(
            slots,
            freeSlots,
            5,
            generationSources);
        const Internal::HandleSlotReservation third = Internal::ReserveHandleSlot<BodyHandle>(
            slots,
            freeSlots,
            5,
            generationSources);

        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        ASSERT_TRUE(third);
        EXPECT_EQ(first.m_index, 2);
        EXPECT_EQ(second.m_index, 0);
        EXPECT_EQ(third.m_index, 3);
        EXPECT_FALSE(first.m_appended);
        EXPECT_FALSE(second.m_appended);
        EXPECT_TRUE(third.m_appended);

        Internal::RollbackHandleSlot(slots, freeSlots, third);
        Internal::RollbackHandleSlot(slots, freeSlots, second);
        Internal::RollbackHandleSlot(slots, freeSlots, first);

        ASSERT_EQ(slots.size(), 3);
        ASSERT_EQ(freeSlots.size(), 2);
        EXPECT_EQ(freeSlots[0], 0);
        EXPECT_EQ(freeSlots[1], 2);
        EXPECT_EQ(slots[0].m_generation, 0);
        EXPECT_EQ(slots[1].m_payload, 17);
        EXPECT_EQ(slots[2].m_generation, 0);

        const Internal::HandleSlotReservation fourth = Internal::ReserveHandleSlot<BodyHandle>(
            slots,
            freeSlots,
            5,
            generationSources);
        ASSERT_TRUE(fourth);
        EXPECT_EQ(fourth.m_index, 2);
        EXPECT_EQ(slots[2].m_generation, 4);
    }

    TEST(HandleTests, SlotReservationFailuresDoNotMutateContainers)
    {
        AZStd::vector<TestHandleSlot> slots(2);
        slots[0].m_payload = 3;
        slots[1].m_payload = 5;
        AZStd::vector<AZ::u32> freeSlots;
        AZ::u32 acquisitionCount = 0;

        const Internal::HandleSlotReservation capacityFailure = Internal::ReserveHandleSlot<BodyHandle>(
            slots,
            freeSlots,
            1,
            [&acquisitionCount]
            {
                ++acquisitionCount;
                return 1;
            });
        EXPECT_FALSE(capacityFailure);
        EXPECT_EQ(
            capacityFailure.m_status,
            Internal::HandleSlotReservationStatus::IndexCapacityExhausted);
        EXPECT_EQ(acquisitionCount, 0);
        ASSERT_EQ(slots.size(), 2);
        EXPECT_EQ(slots[0].m_payload, 3);
        EXPECT_EQ(slots[1].m_payload, 5);
        EXPECT_TRUE(freeSlots.empty());

        freeSlots.push_back(1);
        const Internal::HandleSlotReservation generationFailure = Internal::ReserveHandleSlot<BodyHandle>(
            slots,
            freeSlots,
            1,
            []
            {
                return AZ::u32{0};
            });
        EXPECT_FALSE(generationFailure);
        EXPECT_EQ(
            generationFailure.m_status,
            Internal::HandleSlotReservationStatus::GenerationExhausted);
        ASSERT_EQ(freeSlots.size(), 1);
        EXPECT_EQ(freeSlots.back(), 1);
        EXPECT_EQ(slots[1].m_payload, 5);
    }

    TEST(HandleTests, ReleasedAndRolledBackSlotsNeverReturnConsumedGenerations)
    {
        AZStd::vector<TestHandleSlot> slots;
        AZStd::vector<AZ::u32> freeSlots;
        Internal::AtomicGenerationSources generationSources;

        const Internal::HandleSlotReservation first = Internal::ReserveHandleSlot<ShapeHandle>(
            slots,
            freeSlots,
            3,
            generationSources);
        ASSERT_TRUE(first);
        EXPECT_EQ(slots[first.m_index].m_generation, 1);
        Internal::RollbackHandleSlot(slots, freeSlots, first);

        const Internal::HandleSlotReservation second = Internal::ReserveHandleSlot<ShapeHandle>(
            slots,
            freeSlots,
            3,
            generationSources);
        ASSERT_TRUE(second);
        EXPECT_EQ(slots[second.m_index].m_generation, 2);
        Internal::ReleaseHandleSlot(slots, freeSlots, second.m_index);
        EXPECT_EQ(slots[second.m_index].m_generation, 0);

        const Internal::HandleSlotReservation third = Internal::ReserveHandleSlot<ShapeHandle>(
            slots,
            freeSlots,
            3,
            generationSources);
        ASSERT_TRUE(third);
        EXPECT_EQ(third.m_index, second.m_index);
        EXPECT_EQ(slots[third.m_index].m_generation, 3);
    }
} // namespace Jolt
