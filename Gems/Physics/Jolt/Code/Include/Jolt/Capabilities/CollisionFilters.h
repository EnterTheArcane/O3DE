/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Collision.h>
#include <Jolt/Configuration.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API CollisionFilters
    {
    public:
        [[nodiscard]]
        static CollisionFilters* Get();

        //! The caller owns filter, which must outlive the returned handle and every body that references it.
        [[nodiscard]]
        GroupFilterHandle CreateGroupFilter(
            AZ::u32 subGroupCount,
            IGroupFilter* filter);

        [[nodiscard]]
        GroupFilterHandle CreateGroupFilterTable(
            const GroupFilterTableConfiguration& configuration);

        bool DestroyGroupFilter(GroupFilterHandle filterHandle);

        [[nodiscard]]
        bool IsValid(GroupFilterHandle filterHandle) const;

        //! Call after mutating a custom filter to invalidate contacts and deterministic snapshots.
        bool NotifyGroupFilterChanged(GroupFilterHandle filterHandle);

        [[nodiscard]]
        bool GetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool& enabled) const;

        bool SetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool enabled);

    private:
        friend class Runtime;

        CollisionFilters() = default;
        ~CollisionFilters() = default;

        static AZStd::atomic<CollisionFilters*> s_instance;
    };
} // namespace Jolt
