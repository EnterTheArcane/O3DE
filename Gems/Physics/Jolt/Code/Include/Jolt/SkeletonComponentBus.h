/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Query.h>
#include <Jolt/Skeleton.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/span.h>

namespace Jolt
{
    class ISkeletonComponentRequests
        : public AZ::ComponentBus
    {
    public:
        [[nodiscard]]
        virtual bool IsReady() const = 0;

        [[nodiscard]]
        virtual SkeletonDefinitionHandle GetSkeletonHandle() const = 0;

        [[nodiscard]]
        virtual SkeletalAnimationHandle FindAnimation(AZ::Name name) const = 0;

        [[nodiscard]]
        virtual BufferResult GetAnimationNames(AZStd::span<AZ::Name> names) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Name> CopyAnimationNames() const = 0;
    };

    using SkeletonComponentRequestBus = AZ::EBus<ISkeletonComponentRequests>;

    class ISkeletonComponentNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnSkeletonReady(SkeletonDefinitionHandle skeletonHandle) = 0;

        virtual void OnSkeletonReloading(SkeletonDefinitionHandle skeletonHandle) = 0;

        virtual void OnSkeletonReleased() = 0;
    };

    using SkeletonComponentNotificationBus = AZ::EBus<ISkeletonComponentNotifications>;
} // namespace Jolt
