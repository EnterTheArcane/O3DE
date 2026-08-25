/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SystemConfiguration.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector3.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct RuntimeInfo;

    JOLT_API void ReflectWorlds(AZ::ReflectContext* context);

    class IWorldRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual ~IWorldRequests() = default;

        [[nodiscard]]
        virtual WorldHandle CreateWorld(const WorldConfiguration& configuration) = 0;

        virtual bool DestroyWorld(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual WorldHandle GetDefaultWorldHandle() const = 0;

        [[nodiscard]]
        virtual RuntimeInfo GetRuntimeInfo() const = 0;

        [[nodiscard]]
        virtual bool IsWorldValid(WorldHandle worldHandle) const = 0;

        [[nodiscard]]
        virtual AZ::Vector3 GetGravity(WorldHandle worldHandle) const = 0;

        virtual bool SetGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity) = 0;

        [[nodiscard]]
        virtual SimulationConfiguration GetSimulationConfiguration(WorldHandle worldHandle) const = 0;

        virtual bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual WorldRuntimeConfiguration GetRuntimeConfiguration(WorldHandle worldHandle) const = 0;

        virtual bool UpdateRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration) = 0;
    };

    using WorldRequestBus = AZ::EBus<IWorldRequests>;
} // namespace Jolt
