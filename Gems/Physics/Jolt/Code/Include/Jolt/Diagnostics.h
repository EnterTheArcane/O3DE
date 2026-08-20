/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    void ReflectDiagnostics(AZ::ReflectContext* context);

    enum class SimulationError : AZ::u8
    {
        None = 0,
        BodyPairCacheFull = 1 << 0,
        ContactConstraintsFull = 1 << 1,
        InvalidRequest = 1 << 2,
        ManifoldCacheFull = 1 << 3,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(SimulationError)

    struct SimulationResult final
    {
        AZ_TYPE_INFO(SimulationResult, SimulationResultTypeId);

        AZ::u64 m_updateNanoseconds = 0;
        AZ::u32 m_stepCount = 0;
        SimulationError m_errors = SimulationError::None;

        constexpr explicit operator bool() const noexcept
        {
            return m_errors == SimulationError::None;
        }
    };

    struct WorldStateDigest final
    {
        AZ_TYPE_INFO(WorldStateDigest, WorldStateDigestTypeId);

        AZ::u64 m_hash = 0;
        AZ::u64 m_stateByteCount = 0;

        friend constexpr bool operator==(const WorldStateDigest&, const WorldStateDigest&) noexcept = default;
    };

    //! Per-body native diagnostic counters from the most recent update.
    //! Tick fields are processor ticks and are comparable only within the current process.
    struct BodySimulationStatistics final
    {
        AZ_TYPE_INFO(BodySimulationStatistics, BodySimulationStatisticsTypeId);

        AZ::u64 m_broadPhaseTicks = 0;
        AZ::u64 m_ccdTicks = 0;
        AZ::u64 m_narrowPhaseTicks = 0;
        AZ::u64 m_positionConstraintTicks = 0;
        AZ::u64 m_updateBoundsTicks = 0;
        AZ::u64 m_velocityConstraintTicks = 0;

        AZ::u32 m_contactConstraintCount = 0;
        AZ::u8 m_collisionStepCount = 0;
        AZ::u8 m_positionStepCount = 0;
        AZ::u8 m_velocityStepCount = 0;
        bool m_isLargeIsland = false;
    };

    enum class DiagnosticStatisticsStatus : AZ::u8
    {
        None = 0,
        Complete,
        Overflow,
        Unavailable,
    };

    enum class BroadPhaseQueryKind : AZ::u8
    {
        None = 0,
        CastAabb,
        CollideAabb,
        CollideOrientedBox,
        CollidePoint,
        CollideSphere,
        Raycast,
    };

    enum class NarrowPhaseQueryKind : AZ::u8
    {
        None = 0,
        Cast,
        Collide,
    };

    struct DiagnosticStatisticsResult final
    {
        AZ_TYPE_INFO(DiagnosticStatisticsResult, DiagnosticStatisticsResultTypeId);

        AZ::u32 m_count = 0;
        AZ::u32 m_requiredCount = 0;
        DiagnosticStatisticsStatus m_status = DiagnosticStatisticsStatus::None;

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept
        {
            return m_status == DiagnosticStatisticsStatus::Complete;
        }
    };

    //! Broadphase counters are per world and accumulate until explicitly reset.
    struct BroadPhaseStatistics final
    {
        AZ_TYPE_INFO(BroadPhaseStatistics, BroadPhaseStatisticsTypeId);

        AZ::u64 m_filterDescriptionHash = 0;
        AZ::u64 m_totalTicks = 0;
        AZ::u64 m_collectorTicks = 0;
        AZ::u64 m_queryCount = 0;
        AZ::u64 m_nodesVisited = 0;
        AZ::u64 m_bodiesVisited = 0;
        AZ::u64 m_hitsReported = 0;
        AZ::u32 m_broadPhaseLayer = 0;
        BroadPhaseQueryKind m_queryKind = BroadPhaseQueryKind::None;
    };

    //! Narrowphase counters are process-wide and accumulate until explicitly reset.
    struct NarrowPhaseStatistics final
    {
        AZ_TYPE_INFO(NarrowPhaseStatistics, NarrowPhaseStatisticsTypeId);

        AZ::u64 m_totalTicks = 0;
        AZ::u64 m_childTicks = 0;
        AZ::u64 m_queryCount = 0;
        AZ::u64 m_hitsReported = 0;
        ShapeKind m_firstShapeKind = ShapeKind::None;
        ShapeKind m_secondShapeKind = ShapeKind::None;
        NarrowPhaseQueryKind m_queryKind = NarrowPhaseQueryKind::None;
    };

    struct StateValidationResult final
    {
        AZ_TYPE_INFO(StateValidationResult, StateValidationResultTypeId);

        AZ::u64 m_firstMismatchByte = 0;
        bool m_matches = false;
    };

    enum class StateRestoreStatus : AZ::u8
    {
        None = 0,
        //! The requested state is active and the world remains usable.
        Complete,
        //! Validation or preparation failed before mutation, or the previous state was recovered.
        Rejected,
        //! Recovery failed after mutation. Only world destruction remains valid.
        StateIndeterminate,
    };

    struct StateRestoreResult final
    {
        AZ_TYPE_INFO(StateRestoreResult, StateRestoreResultTypeId);

        constexpr explicit operator bool() const noexcept
        {
            return m_status == StateRestoreStatus::Complete;
        }

        StateRestoreStatus m_status = StateRestoreStatus::None;
    };

    enum class StateSnapshotFlags : AZ::u8
    {
        None = 0,
        Bodies = 1 << 0,
        Constraints = 1 << 1,
        Contacts = 1 << 2,
        Global = 1 << 3,
        Hair = 1 << 4,
        All = Bodies | Constraints | Contacts | Global | Hair,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(StateSnapshotFlags)

    enum class RestoreSafety : AZ::u8
    {
        None = 0,
        Transactional,
        Validated,
    };

    struct StateSnapshotConfiguration final
    {
        AZ_TYPE_INFO(StateSnapshotConfiguration, StateSnapshotConfigurationTypeId);

        //! Callback objects remain caller-owned. Restore requires their behavior state hashes to match capture.
        StateSnapshotFlags m_flags = StateSnapshotFlags::All;

        //! Transactional restores always recover after failure. Validated restores omit recovery only when all state is provider-owned.
        RestoreSafety m_restoreSafety = RestoreSafety::Transactional;

        //! Filtered snapshots contain only the requested bodies and their provider-owned relationship partition.
        bool m_filterBodies = false;
    };

    //! Same-build portable storage for one snapshot or a complete multipart snapshot batch.
    struct StateSnapshotArchive final
    {
        AZ_TYPE_INFO(StateSnapshotArchive, StateSnapshotArchiveTypeId);

        AZStd::vector<AZ::u8> m_binaryState;
        AZ::u64 m_buildFingerprint = 0;
        AZ::u64 m_contentHash = 0;
        AZ::u32 m_formatVersion = 0;
        AZ::u32 m_snapshotCount = 0;
    };

    struct WorldStatistics final
    {
        AZ_TYPE_INFO(WorldStatistics, WorldStatisticsTypeId);

        AZ::u64 m_shapeBytes = 0;
        AZ::u64 m_shapeTriangleCount = 0;
        AZ::u64 m_tempAllocatorCapacityBytes = 0;
        AZ::u64 m_tempAllocatorUsageBytes = 0;
        AZ::u64 m_lastUpdateNanoseconds = 0;
        SimulationError m_lastUpdateErrors = SimulationError::None;
        AZ::u32 m_lastUpdateJobCount = 0;
        AZ::u32 m_lastUpdateMaximumTaskCount = 0;
        AZ::u32 m_lastUpdateTaskCount = 0;
        AZ::u32 m_requestedWorkerCount = 0;
        AZ::u32 m_effectiveWorkerCount = 0;
        AZ::u32 m_hairWorkerCount = 0;

        AZ::u32 m_activeDynamicBodyCount = 0;
        AZ::u32 m_activeKinematicBodyCount = 0;
        AZ::u32 m_activeSoftBodyCount = 0;
        AZ::u32 m_bodyCapacity = 0;
        AZ::u32 m_bodyCount = 0;
        AZ::u32 m_dynamicBodyCount = 0;
        AZ::u32 m_kinematicBodyCount = 0;
        AZ::u32 m_softBodyCount = 0;
        AZ::u32 m_staticBodyCount = 0;

        AZ::u32 m_activeConstraintCount = 0;
        AZ::u32 m_constraintCount = 0;
        AZ::u32 m_shapeCount = 0;

        AZ::u32 m_characterCount = 0;
        AZ::u32 m_hairCount = 0;
        AZ::u32 m_ragdollCount = 0;
        AZ::u32 m_sceneInstanceCount = 0;
        AZ::u32 m_stateSnapshotCount = 0;
        AZ::u32 m_vehicleCount = 0;
        AZ::u32 m_virtualCharacterCount = 0;
    };

    enum class PerformanceStatisticsFlags : AZ::u16
    {
        None = 0,
        BroadPhase = 1 << 0,
        Events = 1 << 1,
        Hair = 1 << 2,
        Jobs = 1 << 3,
        Locks = 1 << 4,
        Memory = 1 << 5,
        NarrowPhase = 1 << 6,
        Queries = 1 << 7,
        Resources = 1 << 8,
        Simulation = 1 << 9,
        Snapshots = 1 << 10,
        All = (1 << 11) - 1,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(PerformanceStatisticsFlags)

    //! Current and high-water information for one world-owned resource category.
    //! Retained bytes describe provider-owned capacity and exclude memory owned by the native allocator.
    struct ResourceStatistics final
    {
        AZ_TYPE_INFO(ResourceStatistics, ResourceStatisticsTypeId);

        AZ::u64 m_retainedBytes = 0;
        AZ::u32 m_capacity = 0;
        AZ::u32 m_count = 0;
        AZ::u32 m_highWaterCount = 0;
    };

    //! Allocation-free snapshot of opt-in counters accumulated by a world since the last reset.
    //! Process-native memory counters cover all live Jolt systems because the native allocator is process-wide.
    //! Resetting them from one world starts a new process-wide interval for every world that has memory statistics enabled.
    struct WorldPerformanceStatistics final
    {
        AZ_TYPE_INFO(WorldPerformanceStatistics, WorldPerformanceStatisticsTypeId);

        PerformanceStatisticsFlags m_availableFlags = PerformanceStatisticsFlags::None;
        PerformanceStatisticsFlags m_enabledFlags = PerformanceStatisticsFlags::None;
        AZ::u64 m_intervalNanoseconds = 0;

        AZ::u64 m_processNativeAllocatedBytes = 0;
        AZ::u64 m_processNativePeakAllocatedBytes = 0;
        AZ::u64 m_processNativeAllocationCount = 0;
        AZ::u64 m_processNativeFreeCount = 0;
        AZ::u64 m_processNativeReallocationCount = 0;
        AZ::u64 m_tempAllocatorCapacityBytes = 0;
        AZ::u64 m_tempAllocatorCurrentBytes = 0;
        AZ::u64 m_tempAllocatorPeakBytes = 0;
        AZ::u64 m_wrapperRetainedBytes = 0;

        ResourceStatistics m_bodies;
        ResourceStatistics m_characters;
        ResourceStatistics m_constraints;
        ResourceStatistics m_hair;
        ResourceStatistics m_ragdolls;
        ResourceStatistics m_scenes;
        ResourceStatistics m_shapes;
        ResourceStatistics m_softBodies;
        ResourceStatistics m_stateSnapshots;
        ResourceStatistics m_vehicles;
        ResourceStatistics m_virtualCharacters;

        AZ::u64 m_broadPhaseOptimizeCount = 0;
        AZ::u64 m_broadPhaseOptimizeNanoseconds = 0;
        AZ::u64 m_originShiftCount = 0;

        AZ::u64 m_contactEventCount = 0;
        AZ::u64 m_contactManifoldCount = 0;
        AZ::u64 m_contactPointCount = 0;

        AZ::u64 m_droppedEventCount = 0;
        AZ::u64 m_eventHighWaterCount = 0;
        AZ::u64 m_publishedEventCount = 0;

        AZ::u64 m_queryCandidateCount = 0;
        AZ::u64 m_queryCount = 0;
        AZ::u64 m_queryHitCount = 0;
        AZ::u64 m_queryNanoseconds = 0;

        AZ::u64 m_snapshotBytes = 0;
        AZ::u64 m_snapshotCaptureCount = 0;
        AZ::u64 m_snapshotCaptureNanoseconds = 0;
        AZ::u64 m_snapshotFailureCount = 0;
        AZ::u64 m_snapshotPeakBytes = 0;
        AZ::u64 m_snapshotRestoreCount = 0;
        AZ::u64 m_snapshotRestoreNanoseconds = 0;

        AZ::u64 m_jobCount = 0;
        AZ::u64 m_jobExecutionNanoseconds = 0;
        AZ::u64 m_jobMaximumQueueLatencyNanoseconds = 0;
        AZ::u64 m_jobQueueLatencyNanoseconds = 0;
        AZ::u64 m_jobTaskCount = 0;
        AZ::u32 m_jobMaximumActiveTaskCount = 0;

        AZ::u64 m_lockContentionCount = 0;
        AZ::u64 m_lockCount = 0;
        AZ::u64 m_lockMaximumWaitNanoseconds = 0;
        AZ::u64 m_lockWaitNanoseconds = 0;

        AZ::u64 m_hairReadbackBytes = 0;
        AZ::u64 m_hairReadbackCount = 0;
        AZ::u64 m_hairReadbackNanoseconds = 0;
        AZ::u64 m_hairUpdateCount = 0;
        AZ::u64 m_hairUpdateNanoseconds = 0;

        AZ::u64 m_simulationErrorCount = 0;
        AZ::u64 m_simulationNanoseconds = 0;
        AZ::u64 m_simulationStepCount = 0;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::StateSnapshotFlags, "{66C982E6-7530-492B-8E45-3EC39234391E}");

AZ_TYPE_INFO_SPECIALIZE(Jolt::SimulationError, "{826890EC-7B26-4C15-8596-7DF75FCD04E1}");

AZ_TYPE_INFO_SPECIALIZE(Jolt::DiagnosticStatisticsStatus, "{BF697872-10BD-498B-B2CE-F12614322A92}");

AZ_TYPE_INFO_SPECIALIZE(Jolt::BroadPhaseQueryKind, "{6A540946-6DB1-48D3-B76C-A4025295EF30}");

AZ_TYPE_INFO_SPECIALIZE(Jolt::NarrowPhaseQueryKind, "{4D2D7359-D430-4897-9681-37F0729E70B3}");

AZ_TYPE_INFO_SPECIALIZE(Jolt::PerformanceStatisticsFlags, "{08EBFB01-ECBC-419C-AF22-B8240FF7494D}");
