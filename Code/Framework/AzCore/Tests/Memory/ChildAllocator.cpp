/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzCore/Memory/AllocatorInstance.h>
#include <AzCore/Memory/ChildAllocatorSchema.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzTest/AzTest.h>

namespace UnitTest
{
    AZ_CHILD_ALLOCATOR_WITH_NAME(
        ChildAllocatorAccountingTestAllocator,
        "Child Allocator Accounting Test",
        "{3F82C531-5F5B-42DC-95FB-61C72673E278}",
        AZ::SystemAllocator);

    TEST(ChildAllocatorTests, UnknownSizeDeallocationReturnsTheReportedAllocationSize)
    {
        AZ::IAllocator& allocator = AZ::AllocatorInstance<ChildAllocatorAccountingTestAllocator>::Get();
        const size_t allocatedBytesBeforeTest = allocator.NumAllocatedBytes();

        constexpr size_t RequestedSize = 47;
        constexpr size_t Alignment = 16;
        const AllocateAddress allocation = allocator.allocate(RequestedSize, Alignment);
        ASSERT_TRUE(allocation);

        const size_t reportedSize = allocation.GetAllocatedBytes();
        EXPECT_GE(reportedSize, RequestedSize);
        EXPECT_EQ(allocator.get_allocated_size(allocation.GetAddress(), Alignment), reportedSize);
        EXPECT_EQ(allocator.NumAllocatedBytes(), allocatedBytesBeforeTest + reportedSize);

        const size_t deallocatedSize = allocator.deallocate(allocation.GetAddress(), 0, Alignment);
        EXPECT_EQ(deallocatedSize, reportedSize);
        EXPECT_EQ(allocator.NumAllocatedBytes(), allocatedBytesBeforeTest);
    }
} // namespace UnitTest
