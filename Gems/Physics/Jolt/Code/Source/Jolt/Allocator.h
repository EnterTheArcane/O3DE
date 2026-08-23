/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Memory/ChildAllocatorSchema.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace Jolt
{
    AZ_CHILD_ALLOCATOR_WITH_NAME(NativeAllocator, "Jolt Native", "{C1E5E56B-B7C6-4C2D-B1AE-9FBB4702C352}", AZ::SystemAllocator);
} // namespace Jolt
