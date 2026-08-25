/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Diagnostics.h>

#include <AzCore/EBus/EBus.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    JOLT_API void ReflectWorldSimulation(AZ::ReflectContext* context);

    class IWorldSimulationRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual ~IWorldSimulationRequests() = default;

        [[nodiscard]]
        virtual SimulationResult StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep) = 0;
    };

    using WorldSimulationRequestBus = AZ::EBus<IWorldSimulationRequests>;
} // namespace Jolt
