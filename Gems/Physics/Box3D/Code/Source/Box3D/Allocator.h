/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Memory/ChildAllocatorSchema.h>

namespace Box3D
{
    AZ_CHILD_ALLOCATOR_WITH_NAME(NativeAllocator, "Box3D Native", "{EFFAFC82-2F24-4F56-A6E1-B814D97D0770}", AZ::SystemAllocator);
} // namespace Box3D
