/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyConfiguration.h>

#include <AzCore/Component/ComponentBus.h>

namespace Jolt
{
    class IStaticRigidBodyRequests
        : public AZ::ComponentBus
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual BodyHandle GetBodyHandle() const = 0;

        [[nodiscard]]
        virtual WorldTransform GetCenterOfMassTransform() const = 0;

        [[nodiscard]]
        virtual BodyState GetState() const = 0;
    };

    using StaticRigidBodyRequestBus = AZ::EBus<IStaticRigidBodyRequests>;
} // namespace Jolt
