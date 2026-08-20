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
            IContactCallbacks* callbacks);

        bool SetBodyPairCollider(
            WorldHandle worldHandle,
            IBodyPairCollider* collider);

        bool SetSimulationShapeFilter(
            WorldHandle worldHandle,
            ISimulationShapeFilter* filter);

        bool SetSoftBodyContactCallbacks(
            WorldHandle worldHandle,
            ISoftBodyContactCallbacks* callbacks);

        bool AddStepListener(
            WorldHandle worldHandle,
            IStepListener* listener);

        bool RemoveStepListener(
            WorldHandle worldHandle,
            IStepListener* listener);

    private:
        friend class Runtime;

        WorldSimulation() = default;
        ~WorldSimulation() = default;

        static AZStd::atomic<WorldSimulation*> s_instance;
    };
} // namespace Jolt
