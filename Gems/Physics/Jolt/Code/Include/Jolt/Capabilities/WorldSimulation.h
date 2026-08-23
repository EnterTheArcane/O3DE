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
#include <Jolt/Operation.h>
#include <Jolt/Simulation.h>
#include <Jolt/SoftBody.h>
#include <AzCore/std/containers/array.h>

namespace Jolt
{
    class Runtime;

    class AutoSimulationOperationResult final
    {
    public:
        [[nodiscard]]
        const SimulationResult& GetSimulationResult() const
        {
            return m_simulationResult;
        }

        [[nodiscard]]
        AZStd::span<const WorldEventBatch> GetEventBatches() const &
        {
            return AZStd::span<const WorldEventBatch>(m_eventBatches).first(m_eventBatchCount);
        }

        AZStd::span<const WorldEventBatch> GetEventBatches() const && = delete;

    private:
        friend class RuntimeImplementation;

        SimulationResult m_simulationResult;
        AZStd::array<WorldEventBatch, MaximumWorldCount> m_eventBatches;
        AZ::u32 m_eventBatchCount = 0;
    };

    class JOLT_API WorldSimulation
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static WorldSimulation* Get();

        bool StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep);

        [[nodiscard]]
        SimulationResult StepWorldDetailed(
            WorldHandle worldHandle,
            float fixedTimeStep);

        [[nodiscard]]
        Operation<SimulationResult> StepWorldAsync(
            WorldHandle worldHandle,
            float fixedTimeStep);

        bool StepAutoSimulatedWorlds(float elapsedTime);

        [[nodiscard]]
        SimulationResult StepAutoSimulatedWorldsDetailed(float elapsedTime);

        [[nodiscard]]
        Operation<AutoSimulationOperationResult> StepAutoSimulatedWorldsAsync(float elapsedTime);

        [[nodiscard]]
        SimulationResult StepAutoSimulatedWorldsDetailed(
            float elapsedTime,
            AZStd::span<WorldEventBatch, MaximumWorldCount> eventBatches,
            AZ::u32& eventBatchCount);

        [[nodiscard]]
        EventBatch GetEvents(WorldHandle worldHandle) const;

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
    };
} // namespace Jolt
