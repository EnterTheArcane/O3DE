/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Internal/HandleEncoding.h>

#include <AzTest/AzTest.h>

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
} // namespace Jolt
