/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SystemConfiguration.h>
#include <Jolt/WorldTypes.h>

namespace Jolt
{
    class Runtime;
    class IWorldQueries;

    class JOLT_API Worlds
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Worlds* Get();

        [[nodiscard]]
        WorldHandle CreateWorld(const WorldConfiguration& configuration);

        bool DestroyWorld(WorldHandle worldHandle);

        [[nodiscard]]
        WorldHandle GetDefaultWorldHandle() const;

        [[nodiscard]]
        const IWorldQueries* GetWorldQueries(WorldHandle worldHandle) const;

        [[nodiscard]]
        bool IsValid(WorldHandle worldHandle) const;

        [[nodiscard]]
        bool GetWorldGravity(
            WorldHandle worldHandle,
            AZ::Vector3& gravity) const;

        bool SetWorldGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity);

        [[nodiscard]]
        bool GetSimulationConfiguration(
            WorldHandle worldHandle,
            SimulationConfiguration& configuration) const;

        bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration);

        [[nodiscard]]
        bool GetWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            WorldRuntimeConfiguration& configuration) const;

        bool UpdateWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration);

    private:
        friend class Runtime;

        Worlds() = default;
        ~Worlds() = default;
    };
} // namespace Jolt
