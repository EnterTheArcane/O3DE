/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/base.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/array.h>

#include <cstddef>

namespace Box3D
{
    //! Duration unit used by simulation profiling fields.
    using Milliseconds = AZStd::chrono::duration<float, AZStd::milli>;

    inline constexpr size_t ConstraintGraphColorCount = 24;
    inline constexpr size_t ContactManifoldBucketCount = 8;

    //! Selects independently collected world-statistics groups.
    enum class StatisticsFlags : AZ::u8
    {
        None = 0,
        Profile = 1 << 0,
        Counters = 1 << 1,
        Capacity = 1 << 2,
        Bounds = 1 << 3,
        All = 0x0f,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(StatisticsFlags)

    [[nodiscard]]
    constexpr bool HasAnyFlag(
        StatisticsFlags value,
        StatisticsFlags flags)
    {
        return (value & flags) != StatisticsFlags::None;
    }

    //! Timings from the most recently completed simulation step.
    struct StepProfile final
    {
        Milliseconds m_total;
        Milliseconds m_pairGeneration;
        Milliseconds m_collision;

        Milliseconds m_solver;
        Milliseconds m_solverSetup;
        Milliseconds m_constraints;
        Milliseconds m_prepareConstraints;
        Milliseconds m_integrateVelocities;
        Milliseconds m_warmStart;
        Milliseconds m_solveImpulses;
        Milliseconds m_integratePositions;
        Milliseconds m_relaxImpulses;
        Milliseconds m_applyRestitution;
        Milliseconds m_storeImpulses;

        Milliseconds m_splitIslands;
        Milliseconds m_updateTransforms;

        Milliseconds m_sensorHits;
        Milliseconds m_jointEvents;
        Milliseconds m_hitEvents;

        Milliseconds m_refitBroadPhase;
        Milliseconds m_continuousCollision;
        Milliseconds m_sleepIslands;
        Milliseconds m_sensorOverlap;
    };

    //! Counts, memory peaks, and solver workload from the most recently completed step.
    struct SimulationCounters final
    {
        AZ_TYPE_INFO(SimulationCounters, SimulationCountersTypeId);

        AZ::u32 m_workerCount = 0;

        AZ::u32 m_bodyCount = 0;
        AZ::u32 m_awakeBodyCount = 0;

        AZ::u32 m_shapeCount = 0;
        AZ::u32 m_contactCount = 0;
        AZ::u32 m_awakeContactCount = 0;
        AZ::u32 m_recycledContactCount = 0;

        AZ::u32 m_jointCount = 0;
        AZ::u32 m_islandCount = 0;
        AZ::u32 m_taskCount = 0;

        AZ::u32 m_staticTreeHeight = 0;
        AZ::u32 m_dynamicTreeHeight = 0;

        AZ::u32 m_separatingAxisCallCount = 0;
        AZ::u32 m_separatingAxisCacheHitCount = 0;

        AZ::u32 m_maxToiDistanceIterations = 0;
        AZ::u32 m_maxToiPushBackIterations = 0;
        AZ::u32 m_maxToiRootIterations = 0;

        AZ::u64 m_stackHighWaterBytes = 0;
        AZ::u64 m_largestWorkerArenaPeakBytes = 0;
        AZ::u64 m_globalAllocatedBytes = 0;

        AZStd::array<AZ::u32, ConstraintGraphColorCount> m_constraintGraphOccupancy{};
        AZStd::array<AZ::u32, ContactManifoldBucketCount> m_contactManifoldHistogram{};
    };

    //! Greatest native allocation occupancy observed since scene creation.
    struct CapacityHighWaterMarks final
    {
        AZ_TYPE_INFO(CapacityHighWaterMarks, CapacityHighWaterMarksTypeId);

        AZ::u32 m_staticShapes = 0;
        AZ::u32 m_dynamicShapes = 0;

        AZ::u32 m_staticBodies = 0;
        AZ::u32 m_dynamicBodies = 0;

        AZ::u32 m_contacts = 0;
    };

    //! Requested diagnostics for one world at a completed simulation tick.
    struct WorldStatistics final
    {
        AZ::u64 m_simulationTick = 0;

        StepProfile m_lastStep;
        SimulationCounters m_counters;
        CapacityHighWaterMarks m_capacityHighWaterMarks;

        AZ::Aabb m_worldBounds = AZ::Aabb::CreateNull();
    };
} // namespace Box3D
