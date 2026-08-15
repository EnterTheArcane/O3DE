/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/TypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
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

    struct StateValidationResult final
    {
        AZ_TYPE_INFO(StateValidationResult, StateValidationResultTypeId);

        AZ::u64 m_firstMismatchByte = 0;
        bool m_matches = false;
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

        AZ::u32 m_bodySnapshotCount = 0;
        AZ::u32 m_characterCount = 0;
        AZ::u32 m_hairCount = 0;
        AZ::u32 m_ragdollCount = 0;
        AZ::u32 m_sceneInstanceCount = 0;
        AZ::u32 m_stateSnapshotCount = 0;
        AZ::u32 m_vehicleCount = 0;
        AZ::u32 m_virtualCharacterCount = 0;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::StateSnapshotFlags, "{66C982E6-7530-492B-8E45-3EC39234391E}");

AZ_TYPE_INFO_SPECIALIZE(Jolt::SimulationError, "{826890EC-7B26-4C15-8596-7DF75FCD04E1}");
