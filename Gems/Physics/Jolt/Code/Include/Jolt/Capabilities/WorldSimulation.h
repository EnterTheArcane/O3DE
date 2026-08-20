/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyCollision.h>
#include <Jolt/Configuration.h>
#include <Jolt/Diagnostics.h>
#include <Jolt/Event.h>
#include <Jolt/Extension.h>
#include <Jolt/Simulation.h>
#include <Jolt/SoftBody.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API WorldSimulation
    {
    public:
        [[nodiscard]]
        static WorldSimulation* Get();

        bool StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep);

        [[nodiscard]]
        SimulationResult StepWorldDetailed(
            WorldHandle worldHandle,
            float fixedTimeStep);

        bool StepAutoSimulatedWorlds(float elapsedTime);

        [[nodiscard]]
        SimulationResult StepAutoSimulatedWorldsDetailed(float elapsedTime);

        [[nodiscard]]
        EventView GetEvents(WorldHandle worldHandle) const;

        bool SetContactCallbacks(
            WorldHandle worldHandle,
            ExtensionHandle extensionHandle);

        bool SetBodyPairCollider(
            WorldHandle worldHandle,
            ExtensionHandle extensionHandle);

        bool SetSimulationShapeFilter(
            WorldHandle worldHandle,
            ExtensionHandle extensionHandle);

        bool SetSoftBodyContactCallbacks(
            WorldHandle worldHandle,
            ExtensionHandle extensionHandle);

        bool AddStepListener(
            WorldHandle worldHandle,
            ExtensionHandle extensionHandle);

        bool RemoveStepListener(
            WorldHandle worldHandle,
            ExtensionHandle extensionHandle);

    private:
        friend class Runtime;

        WorldSimulation() = default;
        ~WorldSimulation() = default;

        static AZStd::atomic<WorldSimulation*> s_instance;
    };
} // namespace Jolt
