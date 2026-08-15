/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Path.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class IPathRequests
        : public AZ::ComponentBus
    {
    public:
        [[nodiscard]]
        virtual PathHandle GetPathHandle() const = 0;

        [[nodiscard]]
        virtual const HermitePathConfiguration& GetConfiguration() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<HermitePathPoint> CopyPoints() const = 0;

        [[nodiscard]]
        virtual PathState GetState() const = 0;

        [[nodiscard]]
        virtual PathSample Sample(float fraction) const = 0;

        [[nodiscard]]
        virtual PathSample FindClosestPoint(
            const AZ::Vector3& position,
            float fractionHint) const = 0;
    };

    using PathRequestBus = AZ::EBus<IPathRequests>;

    class IPathNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnPathCreated([[maybe_unused]] PathHandle pathHandle)
        {
        }

        virtual void OnPathDestroying([[maybe_unused]] PathHandle pathHandle)
        {
        }

        virtual void OnPathDestroyed([[maybe_unused]] PathHandle pathHandle)
        {
        }
    };

    using PathNotificationBus = AZ::EBus<IPathNotifications>;
} // namespace Jolt
