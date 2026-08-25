/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Event.h>

#include <AzCore/base.h>
#include <AzCore/Component/ComponentBus.h>

namespace Jolt
{
    class IBodyRequests
        : public AZ::ComponentBus
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual BodyHandle GetBodyHandle() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetUserData() const = 0;

        virtual bool SetUserData(AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual WorldTransform GetCenterOfMassTransform() const = 0;
    };

    using BodyRequestBus = AZ::EBus<IBodyRequests>;

    class IBodyNotifications
        : public AZ::ComponentBus
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

        virtual void OnBodyCreated(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] BodyHandle bodyHandle)
        {
        }

        virtual void OnBodyDestroying(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] BodyHandle bodyHandle)
        {
        }

        virtual void OnBodyDestroyed(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] BodyHandle bodyHandle)
        {
        }

        virtual void OnBodyMoved(
            [[maybe_unused]] const BodyMoveEvent& event)
        {
        }
    };

    using BodyNotificationBus = AZ::EBus<IBodyNotifications>;
} // namespace Jolt
