/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HandleEncoding.h>

#include <AzTest/AzTest.h>

#include <AzCore/std/limits.h>

namespace Jolt
{
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

    TEST(HandleTests, WorldMemberGenerationSourcesArePersistentAndIndependentByKind)
    {
        Internal::WorldMemberGenerationSources generationSources;

        const auto acquireHandle = [&generationSources]<typename HandleType>()
        {
            AZ::u32 generation = 0;
            if (!generationSources.Acquire<HandleType>(generation))
            {
                return HandleType::Invalid;
            }
            return Internal::MakeWorldMemberHandle<HandleType>(0, 0, generation);
        };

        const BodyHandle firstBody = acquireHandle.template operator()<BodyHandle>();
        const BodyHandle secondBody = acquireHandle.template operator()<BodyHandle>();
        const ShapeHandle firstShape = acquireHandle.template operator()<ShapeHandle>();
        const ShapeHandle secondShape = acquireHandle.template operator()<ShapeHandle>();

        ASSERT_TRUE(firstBody);
        ASSERT_TRUE(secondBody);
        ASSERT_TRUE(firstShape);
        ASSERT_TRUE(secondShape);
        EXPECT_NE(firstBody, secondBody);
        EXPECT_NE(firstShape, secondShape);

        Internal::WorldMemberHandleParts firstBodyParts;
        Internal::WorldMemberHandleParts firstShapeParts;
        ASSERT_TRUE(Internal::DecodeWorldMemberHandle(firstBody, firstBodyParts));
        ASSERT_TRUE(Internal::DecodeWorldMemberHandle(firstShape, firstShapeParts));
        EXPECT_EQ(firstBodyParts.m_generation, 1);
        EXPECT_EQ(firstShapeParts.m_generation, 1);
    }

    TEST(HandleTests, WorldMemberGenerationSourcesRetireAfterMaximumGeneration)
    {
        Internal::WorldMemberGenerationSources generationSources(AZStd::numeric_limits<AZ::u32>::max());
        AZ::u32 generation = 0;

        EXPECT_TRUE(generationSources.Acquire<BodyHandle>(generation));
        EXPECT_EQ(generation, AZStd::numeric_limits<AZ::u32>::max());
        EXPECT_FALSE(generationSources.Acquire<BodyHandle>(generation));

        EXPECT_TRUE(generationSources.Acquire<ShapeHandle>(generation));
        EXPECT_EQ(generation, AZStd::numeric_limits<AZ::u32>::max());
        EXPECT_FALSE(generationSources.Acquire<ShapeHandle>(generation));
    }
} // namespace Jolt
