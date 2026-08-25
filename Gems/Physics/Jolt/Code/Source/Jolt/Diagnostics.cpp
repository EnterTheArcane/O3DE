/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Diagnostics.h>

#include <Jolt/BehaviorReflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void ReflectDiagnostics(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<SimulationResult>()
                ->Field("UpdateNanoseconds", &SimulationResult::m_updateNanoseconds)
                ->Field("StepCount", &SimulationResult::m_stepCount)
                ->Field("Errors", &SimulationResult::m_errors);

            serializeContext
                ->Class<WorldStateDigest>()
                ->Field("Hash", &WorldStateDigest::m_hash)
                ->Field("StateByteCount", &WorldStateDigest::m_stateByteCount);

            serializeContext
                ->Class<BodySimulationStatistics>()
                ->Field("BroadPhaseTicks", &BodySimulationStatistics::m_broadPhaseTicks)
                ->Field("CcdTicks", &BodySimulationStatistics::m_ccdTicks)
                ->Field("NarrowPhaseTicks", &BodySimulationStatistics::m_narrowPhaseTicks)
                ->Field("PositionConstraintTicks", &BodySimulationStatistics::m_positionConstraintTicks)
                ->Field("UpdateBoundsTicks", &BodySimulationStatistics::m_updateBoundsTicks)
                ->Field("VelocityConstraintTicks", &BodySimulationStatistics::m_velocityConstraintTicks)
                ->Field("ContactConstraintCount", &BodySimulationStatistics::m_contactConstraintCount)
                ->Field("CollisionStepCount", &BodySimulationStatistics::m_collisionStepCount)
                ->Field("PositionStepCount", &BodySimulationStatistics::m_positionStepCount)
                ->Field("VelocityStepCount", &BodySimulationStatistics::m_velocityStepCount)
                ->Field("IsLargeIsland", &BodySimulationStatistics::m_isLargeIsland);

            serializeContext
                ->Class<DiagnosticStatisticsResult>()
                ->Field("Count", &DiagnosticStatisticsResult::m_count)
                ->Field("RequiredCount", &DiagnosticStatisticsResult::m_requiredCount)
                ->Field("Status", &DiagnosticStatisticsResult::m_status);

            serializeContext
                ->Class<BroadPhaseStatistics>()
                ->Field("FilterDescriptionHash", &BroadPhaseStatistics::m_filterDescriptionHash)
                ->Field("TotalTicks", &BroadPhaseStatistics::m_totalTicks)
                ->Field("CollectorTicks", &BroadPhaseStatistics::m_collectorTicks)
                ->Field("QueryCount", &BroadPhaseStatistics::m_queryCount)
                ->Field("NodesVisited", &BroadPhaseStatistics::m_nodesVisited)
                ->Field("BodiesVisited", &BroadPhaseStatistics::m_bodiesVisited)
                ->Field("HitsReported", &BroadPhaseStatistics::m_hitsReported)
                ->Field("BroadPhaseLayer", &BroadPhaseStatistics::m_broadPhaseLayer)
                ->Field("QueryKind", &BroadPhaseStatistics::m_queryKind);

            serializeContext
                ->Class<NarrowPhaseStatistics>()
                ->Field("TotalTicks", &NarrowPhaseStatistics::m_totalTicks)
                ->Field("ChildTicks", &NarrowPhaseStatistics::m_childTicks)
                ->Field("QueryCount", &NarrowPhaseStatistics::m_queryCount)
                ->Field("HitsReported", &NarrowPhaseStatistics::m_hitsReported)
                ->Field("FirstShapeKind", &NarrowPhaseStatistics::m_firstShapeKind)
                ->Field("SecondShapeKind", &NarrowPhaseStatistics::m_secondShapeKind)
                ->Field("QueryKind", &NarrowPhaseStatistics::m_queryKind);

            serializeContext
                ->Class<StateValidationResult>()
                ->Field("FirstMismatchByte", &StateValidationResult::m_firstMismatchByte)
                ->Field("Matches", &StateValidationResult::m_matches);

            serializeContext
                ->Class<StateRestoreResult>()
                ->Field("Status", &StateRestoreResult::m_status);

            serializeContext
                ->Class<StateSnapshotConfiguration>()
                ->Field("Flags", &StateSnapshotConfiguration::m_flags)
                ->Field("RestoreSafety", &StateSnapshotConfiguration::m_restoreSafety)
                ->Field("FilterBodies", &StateSnapshotConfiguration::m_filterBodies);

            serializeContext
                ->Class<StateSnapshotArchive>()
                ->Field("BinaryState", &StateSnapshotArchive::m_binaryState)
                ->Field("BuildFingerprint", &StateSnapshotArchive::m_buildFingerprint)
                ->Field("ContentHash", &StateSnapshotArchive::m_contentHash)
                ->Field("FormatVersion", &StateSnapshotArchive::m_formatVersion)
                ->Field("SnapshotCount", &StateSnapshotArchive::m_snapshotCount);

            serializeContext
                ->Class<WorldStatistics>()
                ->Field("ShapeBytes", &WorldStatistics::m_shapeBytes)
                ->Field("ShapeTriangleCount", &WorldStatistics::m_shapeTriangleCount)
                ->Field("TempAllocatorCapacityBytes", &WorldStatistics::m_tempAllocatorCapacityBytes)
                ->Field("TempAllocatorUsageBytes", &WorldStatistics::m_tempAllocatorUsageBytes)
                ->Field("LastUpdateNanoseconds", &WorldStatistics::m_lastUpdateNanoseconds)
                ->Field("HairShaderWrapperCreationCount", &WorldStatistics::m_hairShaderWrapperCreationCount)
                ->Field("LastUpdateErrors", &WorldStatistics::m_lastUpdateErrors)
                ->Field("LastUpdateJobCount", &WorldStatistics::m_lastUpdateJobCount)
                ->Field("LastUpdateMaximumTaskCount", &WorldStatistics::m_lastUpdateMaximumTaskCount)
                ->Field("LastUpdateTaskCount", &WorldStatistics::m_lastUpdateTaskCount)
                ->Field("JobTaskCapacity", &WorldStatistics::m_jobTaskCapacity)
                ->Field("RequestedWorkerCount", &WorldStatistics::m_requestedWorkerCount)
                ->Field("EffectiveWorkerCount", &WorldStatistics::m_effectiveWorkerCount)
                ->Field("HairShaderWrapperCount", &WorldStatistics::m_hairShaderWrapperCount)
                ->Field("HairWorkerCount", &WorldStatistics::m_hairWorkerCount)
                ->Field("ActiveDynamicBodyCount", &WorldStatistics::m_activeDynamicBodyCount)
                ->Field("ActiveKinematicBodyCount", &WorldStatistics::m_activeKinematicBodyCount)
                ->Field("ActiveSoftBodyCount", &WorldStatistics::m_activeSoftBodyCount)
                ->Field("BodyCapacity", &WorldStatistics::m_bodyCapacity)
                ->Field("BodyCount", &WorldStatistics::m_bodyCount)
                ->Field("DynamicBodyCount", &WorldStatistics::m_dynamicBodyCount)
                ->Field("KinematicBodyCount", &WorldStatistics::m_kinematicBodyCount)
                ->Field("SoftBodyCount", &WorldStatistics::m_softBodyCount)
                ->Field("StaticBodyCount", &WorldStatistics::m_staticBodyCount)
                ->Field("ActiveConstraintCount", &WorldStatistics::m_activeConstraintCount)
                ->Field("ConstraintCount", &WorldStatistics::m_constraintCount)
                ->Field("ShapeCount", &WorldStatistics::m_shapeCount)
                ->Field("CharacterCount", &WorldStatistics::m_characterCount)
                ->Field("HairCount", &WorldStatistics::m_hairCount)
                ->Field("RagdollCount", &WorldStatistics::m_ragdollCount)
                ->Field("SceneInstanceCount", &WorldStatistics::m_sceneInstanceCount)
                ->Field("StateSnapshotCount", &WorldStatistics::m_stateSnapshotCount)
                ->Field("VehicleCount", &WorldStatistics::m_vehicleCount)
                ->Field("VirtualCharacterCount", &WorldStatistics::m_virtualCharacterCount);

            serializeContext
                ->Class<PoolStatistics>()
                ->Field("LiveBytes", &PoolStatistics::m_liveBytes)
                ->Field("CachedBytes", &PoolStatistics::m_cachedBytes)
                ->Field("OutstandingBytes", &PoolStatistics::m_outstandingBytes)
                ->Field("HighWaterBytes", &PoolStatistics::m_highWaterBytes)
                ->Field("LiveCount", &PoolStatistics::m_liveCount)
                ->Field("CachedCount", &PoolStatistics::m_cachedCount)
                ->Field("OutstandingCount", &PoolStatistics::m_outstandingCount)
                ->Field("HighWaterCount", &PoolStatistics::m_highWaterCount);

            serializeContext
                ->Class<ResourceStatistics>()
                ->Field("RetainedBytes", &ResourceStatistics::m_retainedBytes)
                ->Field("Capacity", &ResourceStatistics::m_capacity)
                ->Field("Count", &ResourceStatistics::m_count)
                ->Field("HighWaterCount", &ResourceStatistics::m_highWaterCount);

            serializeContext
                ->Class<WorldPerformanceStatistics>()
                ->Field("AvailableFlags", &WorldPerformanceStatistics::m_availableFlags)
                ->Field("EnabledFlags", &WorldPerformanceStatistics::m_enabledFlags)
                ->Field("IntervalNanoseconds", &WorldPerformanceStatistics::m_intervalNanoseconds)
                ->Field("ProcessNativeAllocatedBytes", &WorldPerformanceStatistics::m_processNativeAllocatedBytes)
                ->Field("ProcessNativePeakAllocatedBytes", &WorldPerformanceStatistics::m_processNativePeakAllocatedBytes)
                ->Field("ProcessNativeAllocationCount", &WorldPerformanceStatistics::m_processNativeAllocationCount)
                ->Field("ProcessNativeFreeCount", &WorldPerformanceStatistics::m_processNativeFreeCount)
                ->Field("ProcessNativeReallocationCount", &WorldPerformanceStatistics::m_processNativeReallocationCount)
                ->Field("TempAllocatorCapacityBytes", &WorldPerformanceStatistics::m_tempAllocatorCapacityBytes)
                ->Field("TempAllocatorCurrentBytes", &WorldPerformanceStatistics::m_tempAllocatorCurrentBytes)
                ->Field("TempAllocatorPeakBytes", &WorldPerformanceStatistics::m_tempAllocatorPeakBytes)
                ->Field("WrapperRetainedBytes", &WorldPerformanceStatistics::m_wrapperRetainedBytes)
                ->Field("Bodies", &WorldPerformanceStatistics::m_bodies)
                ->Field("Characters", &WorldPerformanceStatistics::m_characters)
                ->Field("Constraints", &WorldPerformanceStatistics::m_constraints)
                ->Field("EventBatches", &WorldPerformanceStatistics::m_eventBatches)
                ->Field("Hair", &WorldPerformanceStatistics::m_hair)
                ->Field("Ragdolls", &WorldPerformanceStatistics::m_ragdolls)
                ->Field("Scenes", &WorldPerformanceStatistics::m_scenes)
                ->Field("Shapes", &WorldPerformanceStatistics::m_shapes)
                ->Field("SoftBodies", &WorldPerformanceStatistics::m_softBodies)
                ->Field("StateSnapshots", &WorldPerformanceStatistics::m_stateSnapshots)
                ->Field("Vehicles", &WorldPerformanceStatistics::m_vehicles)
                ->Field("VirtualCharacters", &WorldPerformanceStatistics::m_virtualCharacters)
                ->Field("Operations", &WorldPerformanceStatistics::m_operations)
                ->Field("BroadPhaseOptimizeCount", &WorldPerformanceStatistics::m_broadPhaseOptimizeCount)
                ->Field("BroadPhaseOptimizeNanoseconds", &WorldPerformanceStatistics::m_broadPhaseOptimizeNanoseconds)
                ->Field("OriginShiftCount", &WorldPerformanceStatistics::m_originShiftCount)
                ->Field("ContactEventCount", &WorldPerformanceStatistics::m_contactEventCount)
                ->Field("ContactManifoldCount", &WorldPerformanceStatistics::m_contactManifoldCount)
                ->Field("ContactPointCount", &WorldPerformanceStatistics::m_contactPointCount)
                ->Field("DroppedEventCount", &WorldPerformanceStatistics::m_droppedEventCount)
                ->Field("EventHighWaterCount", &WorldPerformanceStatistics::m_eventHighWaterCount)
                ->Field("PublishedEventCount", &WorldPerformanceStatistics::m_publishedEventCount)
                ->Field("QueryCandidateCount", &WorldPerformanceStatistics::m_queryCandidateCount)
                ->Field("QueryCount", &WorldPerformanceStatistics::m_queryCount)
                ->Field("QueryHitCount", &WorldPerformanceStatistics::m_queryHitCount)
                ->Field("QueryNanoseconds", &WorldPerformanceStatistics::m_queryNanoseconds)
                ->Field("SnapshotBytes", &WorldPerformanceStatistics::m_snapshotBytes)
                ->Field("SnapshotCaptureCount", &WorldPerformanceStatistics::m_snapshotCaptureCount)
                ->Field("SnapshotCaptureNanoseconds", &WorldPerformanceStatistics::m_snapshotCaptureNanoseconds)
                ->Field("SnapshotFailureCount", &WorldPerformanceStatistics::m_snapshotFailureCount)
                ->Field("SnapshotPeakBytes", &WorldPerformanceStatistics::m_snapshotPeakBytes)
                ->Field("SnapshotRestoreCount", &WorldPerformanceStatistics::m_snapshotRestoreCount)
                ->Field("SnapshotRestoreNanoseconds", &WorldPerformanceStatistics::m_snapshotRestoreNanoseconds)
                ->Field("JobCount", &WorldPerformanceStatistics::m_jobCount)
                ->Field("JobExecutionNanoseconds", &WorldPerformanceStatistics::m_jobExecutionNanoseconds)
                ->Field("JobMaximumQueueLatencyNanoseconds", &WorldPerformanceStatistics::m_jobMaximumQueueLatencyNanoseconds)
                ->Field("JobQueueLatencyNanoseconds", &WorldPerformanceStatistics::m_jobQueueLatencyNanoseconds)
                ->Field("JobTaskCount", &WorldPerformanceStatistics::m_jobTaskCount)
                ->Field("JobMaximumActiveTaskCount", &WorldPerformanceStatistics::m_jobMaximumActiveTaskCount)
                ->Field("LockContentionCount", &WorldPerformanceStatistics::m_lockContentionCount)
                ->Field("LockCount", &WorldPerformanceStatistics::m_lockCount)
                ->Field("LockMaximumWaitNanoseconds", &WorldPerformanceStatistics::m_lockMaximumWaitNanoseconds)
                ->Field("LockWaitNanoseconds", &WorldPerformanceStatistics::m_lockWaitNanoseconds)
                ->Field("HairReadbackBytes", &WorldPerformanceStatistics::m_hairReadbackBytes)
                ->Field("HairReadbackCount", &WorldPerformanceStatistics::m_hairReadbackCount)
                ->Field("HairReadbackNanoseconds", &WorldPerformanceStatistics::m_hairReadbackNanoseconds)
                ->Field("HairUpdateCount", &WorldPerformanceStatistics::m_hairUpdateCount)
                ->Field("HairUpdateNanoseconds", &WorldPerformanceStatistics::m_hairUpdateNanoseconds)
                ->Field("SimulationErrorCount", &WorldPerformanceStatistics::m_simulationErrorCount)
                ->Field("SimulationNanoseconds", &WorldPerformanceStatistics::m_simulationNanoseconds)
                ->Field("SimulationStepCount", &WorldPerformanceStatistics::m_simulationStepCount);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EnumProperty<static_cast<AZ::u8>(SimulationError::None)>("SimulationError_None")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(SimulationError::BodyPairCacheFull)>(
                "SimulationError_BodyPairCacheFull")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(SimulationError::ContactConstraintsFull)>(
                "SimulationError_ContactConstraintsFull")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(SimulationError::InvalidRequest)>(
                "SimulationError_InvalidRequest")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(SimulationError::ManifoldCacheFull)>(
                "SimulationError_ManifoldCacheFull")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DiagnosticStatisticsStatus, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DiagnosticStatisticsStatus, Complete);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DiagnosticStatisticsStatus, Overflow);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DiagnosticStatisticsStatus, Unavailable);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, CastAabb);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, CollideAabb);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, CollideOrientedBox);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, CollidePoint);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, CollideSphere);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BroadPhaseQueryKind, Raycast);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, NarrowPhaseQueryKind, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, NarrowPhaseQueryKind, Cast);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, NarrowPhaseQueryKind, Collide);

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::None)>(
                "PerformanceStatisticsFlags_None")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::BroadPhase)>(
                "PerformanceStatisticsFlags_BroadPhase")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Events)>(
                "PerformanceStatisticsFlags_Events")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Hair)>(
                "PerformanceStatisticsFlags_Hair")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Jobs)>(
                "PerformanceStatisticsFlags_Jobs")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Locks)>(
                "PerformanceStatisticsFlags_Locks")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Memory)>(
                "PerformanceStatisticsFlags_Memory")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::NarrowPhase)>(
                "PerformanceStatisticsFlags_NarrowPhase")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Queries)>(
                "PerformanceStatisticsFlags_Queries")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Resources)>(
                "PerformanceStatisticsFlags_Resources")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Simulation)>(
                "PerformanceStatisticsFlags_Simulation")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::Snapshots)>(
                "PerformanceStatisticsFlags_Snapshots")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u16>(PerformanceStatisticsFlags::All)>(
                "PerformanceStatisticsFlags_All")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::None)>("StateSnapshotFlags_None")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::Bodies)>("StateSnapshotFlags_Bodies")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::Constraints)>(
                "StateSnapshotFlags_Constraints")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::Contacts)>(
                "StateSnapshotFlags_Contacts")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::Global)>("StateSnapshotFlags_Global")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::Hair)>("StateSnapshotFlags_Hair")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateSnapshotFlags::All)>("StateSnapshotFlags_All")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(RestoreSafety::None)>("RestoreSafety_None")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(RestoreSafety::Transactional)>(
                "RestoreSafety_Transactional")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(RestoreSafety::Validated)>("RestoreSafety_Validated")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateRestoreStatus::None)>("StateRestoreStatus_None")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateRestoreStatus::Complete)>(
                "StateRestoreStatus_Complete")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateRestoreStatus::Rejected)>(
                "StateRestoreStatus_Rejected")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(StateRestoreStatus::StateIndeterminate)>(
                "StateRestoreStatus_StateIndeterminate")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt");

            behaviorContext->Class<SimulationResult>("SimulationResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "updateNanoseconds",
                    BehaviorValueGetter(&SimulationResult::m_updateNanoseconds),
                    nullptr)
                ->Property("stepCount", BehaviorValueGetter(&SimulationResult::m_stepCount), nullptr)
                ->Property("errors", BehaviorValueGetter(&SimulationResult::m_errors), nullptr);

            behaviorContext->Class<WorldStateDigest>("WorldStateDigest")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("hash", BehaviorValueGetter(&WorldStateDigest::m_hash), nullptr)
                ->Property(
                    "stateByteCount",
                    BehaviorValueGetter(&WorldStateDigest::m_stateByteCount),
                    nullptr);

            behaviorContext->Class<BodySimulationStatistics>("BodySimulationStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "broadPhaseTicks",
                    BehaviorValueGetter(&BodySimulationStatistics::m_broadPhaseTicks),
                    nullptr)
                ->Property("ccdTicks", BehaviorValueGetter(&BodySimulationStatistics::m_ccdTicks), nullptr)
                ->Property(
                    "narrowPhaseTicks",
                    BehaviorValueGetter(&BodySimulationStatistics::m_narrowPhaseTicks),
                    nullptr)
                ->Property(
                    "positionConstraintTicks",
                    BehaviorValueGetter(&BodySimulationStatistics::m_positionConstraintTicks),
                    nullptr)
                ->Property(
                    "updateBoundsTicks",
                    BehaviorValueGetter(&BodySimulationStatistics::m_updateBoundsTicks),
                    nullptr)
                ->Property(
                    "velocityConstraintTicks",
                    BehaviorValueGetter(&BodySimulationStatistics::m_velocityConstraintTicks),
                    nullptr)
                ->Property(
                    "contactConstraintCount",
                    BehaviorValueGetter(&BodySimulationStatistics::m_contactConstraintCount),
                    nullptr)
                ->Property(
                    "collisionStepCount",
                    BehaviorValueGetter(&BodySimulationStatistics::m_collisionStepCount),
                    nullptr)
                ->Property(
                    "positionStepCount",
                    BehaviorValueGetter(&BodySimulationStatistics::m_positionStepCount),
                    nullptr)
                ->Property(
                    "velocityStepCount",
                    BehaviorValueGetter(&BodySimulationStatistics::m_velocityStepCount),
                    nullptr)
                ->Property(
                    "isLargeIsland",
                    BehaviorValueGetter(&BodySimulationStatistics::m_isLargeIsland),
                    nullptr);

            behaviorContext->Class<DiagnosticStatisticsResult>("DiagnosticStatisticsResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("count", BehaviorValueGetter(&DiagnosticStatisticsResult::m_count), nullptr)
                ->Property(
                    "requiredCount",
                    BehaviorValueGetter(&DiagnosticStatisticsResult::m_requiredCount),
                    nullptr)
                ->Property("status", BehaviorValueGetter(&DiagnosticStatisticsResult::m_status), nullptr);

            behaviorContext->Class<BroadPhaseStatistics>("BroadPhaseStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "filterDescriptionHash",
                    BehaviorValueGetter(&BroadPhaseStatistics::m_filterDescriptionHash),
                    nullptr)
                ->Property("totalTicks", BehaviorValueGetter(&BroadPhaseStatistics::m_totalTicks), nullptr)
                ->Property("collectorTicks", BehaviorValueGetter(&BroadPhaseStatistics::m_collectorTicks), nullptr)
                ->Property("queryCount", BehaviorValueGetter(&BroadPhaseStatistics::m_queryCount), nullptr)
                ->Property("nodesVisited", BehaviorValueGetter(&BroadPhaseStatistics::m_nodesVisited), nullptr)
                ->Property("bodiesVisited", BehaviorValueGetter(&BroadPhaseStatistics::m_bodiesVisited), nullptr)
                ->Property("hitsReported", BehaviorValueGetter(&BroadPhaseStatistics::m_hitsReported), nullptr)
                ->Property("broadPhaseLayer", BehaviorValueGetter(&BroadPhaseStatistics::m_broadPhaseLayer), nullptr)
                ->Property("queryKind", BehaviorValueGetter(&BroadPhaseStatistics::m_queryKind), nullptr);

            behaviorContext->Class<NarrowPhaseStatistics>("NarrowPhaseStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("totalTicks", BehaviorValueGetter(&NarrowPhaseStatistics::m_totalTicks), nullptr)
                ->Property("childTicks", BehaviorValueGetter(&NarrowPhaseStatistics::m_childTicks), nullptr)
                ->Property("queryCount", BehaviorValueGetter(&NarrowPhaseStatistics::m_queryCount), nullptr)
                ->Property("hitsReported", BehaviorValueGetter(&NarrowPhaseStatistics::m_hitsReported), nullptr)
                ->Property("firstShapeKind", BehaviorValueGetter(&NarrowPhaseStatistics::m_firstShapeKind), nullptr)
                ->Property("secondShapeKind", BehaviorValueGetter(&NarrowPhaseStatistics::m_secondShapeKind), nullptr)
                ->Property("queryKind", BehaviorValueGetter(&NarrowPhaseStatistics::m_queryKind), nullptr);

            behaviorContext->Class<StateValidationResult>("StateValidationResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "firstMismatchByte",
                    BehaviorValueGetter(&StateValidationResult::m_firstMismatchByte),
                    nullptr)
                ->Property("matches", BehaviorValueGetter(&StateValidationResult::m_matches), nullptr);

            behaviorContext->Class<StateRestoreResult>("StateRestoreResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("status", BehaviorValueGetter(&StateRestoreResult::m_status), nullptr);

            behaviorContext->Class<StateSnapshotConfiguration>("StateSnapshotConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("flags", JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotConfiguration::m_flags))
                ->Property(
                    "restoreSafety",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotConfiguration::m_restoreSafety))
                ->Property(
                    "filterBodies",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotConfiguration::m_filterBodies));

            behaviorContext->Class<StateSnapshotArchive>("StateSnapshotArchive")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("binaryState", JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotArchive::m_binaryState))
                ->Property(
                    "buildFingerprint",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotArchive::m_buildFingerprint))
                ->Property("contentHash", JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotArchive::m_contentHash))
                ->Property("formatVersion", JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotArchive::m_formatVersion))
                ->Property("snapshotCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&StateSnapshotArchive::m_snapshotCount));

            behaviorContext->Class<WorldStatistics>("WorldStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("shapeBytes", BehaviorValueGetter(&WorldStatistics::m_shapeBytes), nullptr)
                ->Property(
                    "shapeTriangleCount",
                    BehaviorValueGetter(&WorldStatistics::m_shapeTriangleCount),
                    nullptr)
                ->Property(
                    "tempAllocatorCapacityBytes",
                    BehaviorValueGetter(&WorldStatistics::m_tempAllocatorCapacityBytes),
                    nullptr)
                ->Property(
                    "tempAllocatorUsageBytes",
                    BehaviorValueGetter(&WorldStatistics::m_tempAllocatorUsageBytes),
                    nullptr)
                ->Property(
                    "lastUpdateNanoseconds",
                    BehaviorValueGetter(&WorldStatistics::m_lastUpdateNanoseconds),
                    nullptr)
                ->Property(
                    "hairShaderWrapperCreationCount",
                    BehaviorValueGetter(&WorldStatistics::m_hairShaderWrapperCreationCount),
                    nullptr)
                ->Property(
                    "lastUpdateErrors",
                    BehaviorValueGetter(&WorldStatistics::m_lastUpdateErrors),
                    nullptr)
                ->Property(
                    "lastUpdateJobCount",
                    BehaviorValueGetter(&WorldStatistics::m_lastUpdateJobCount),
                    nullptr)
                ->Property(
                    "lastUpdateMaximumTaskCount",
                    BehaviorValueGetter(&WorldStatistics::m_lastUpdateMaximumTaskCount),
                    nullptr)
                ->Property(
                    "lastUpdateTaskCount",
                    BehaviorValueGetter(&WorldStatistics::m_lastUpdateTaskCount),
                    nullptr)
                ->Property(
                    "jobTaskCapacity",
                    BehaviorValueGetter(&WorldStatistics::m_jobTaskCapacity),
                    nullptr)
                ->Property(
                    "requestedWorkerCount",
                    BehaviorValueGetter(&WorldStatistics::m_requestedWorkerCount),
                    nullptr)
                ->Property(
                    "effectiveWorkerCount",
                    BehaviorValueGetter(&WorldStatistics::m_effectiveWorkerCount),
                    nullptr)
                ->Property(
                    "hairShaderWrapperCount",
                    BehaviorValueGetter(&WorldStatistics::m_hairShaderWrapperCount),
                    nullptr)
                ->Property(
                    "hairWorkerCount",
                    BehaviorValueGetter(&WorldStatistics::m_hairWorkerCount),
                    nullptr)
                ->Property(
                    "activeDynamicBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_activeDynamicBodyCount),
                    nullptr)
                ->Property(
                    "activeKinematicBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_activeKinematicBodyCount),
                    nullptr)
                ->Property(
                    "activeSoftBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_activeSoftBodyCount),
                    nullptr)
                ->Property("bodyCapacity", BehaviorValueGetter(&WorldStatistics::m_bodyCapacity), nullptr)
                ->Property("bodyCount", BehaviorValueGetter(&WorldStatistics::m_bodyCount), nullptr)
                ->Property(
                    "dynamicBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_dynamicBodyCount),
                    nullptr)
                ->Property(
                    "kinematicBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_kinematicBodyCount),
                    nullptr)
                ->Property(
                    "softBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_softBodyCount),
                    nullptr)
                ->Property(
                    "staticBodyCount",
                    BehaviorValueGetter(&WorldStatistics::m_staticBodyCount),
                    nullptr)
                ->Property(
                    "activeConstraintCount",
                    BehaviorValueGetter(&WorldStatistics::m_activeConstraintCount),
                    nullptr)
                ->Property(
                    "constraintCount",
                    BehaviorValueGetter(&WorldStatistics::m_constraintCount),
                    nullptr)
                ->Property("shapeCount", BehaviorValueGetter(&WorldStatistics::m_shapeCount), nullptr)
                ->Property(
                    "characterCount",
                    BehaviorValueGetter(&WorldStatistics::m_characterCount),
                    nullptr)
                ->Property("hairCount", BehaviorValueGetter(&WorldStatistics::m_hairCount), nullptr)
                ->Property(
                    "ragdollCount",
                    BehaviorValueGetter(&WorldStatistics::m_ragdollCount),
                    nullptr)
                ->Property(
                    "sceneInstanceCount",
                    BehaviorValueGetter(&WorldStatistics::m_sceneInstanceCount),
                    nullptr)
                ->Property(
                    "stateSnapshotCount",
                    BehaviorValueGetter(&WorldStatistics::m_stateSnapshotCount),
                    nullptr)
                ->Property("vehicleCount", BehaviorValueGetter(&WorldStatistics::m_vehicleCount), nullptr)
                ->Property(
                    "virtualCharacterCount",
                    BehaviorValueGetter(&WorldStatistics::m_virtualCharacterCount),
                    nullptr);

            behaviorContext->Class<PoolStatistics>("PoolStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("liveBytes", JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_liveBytes))
                ->Property("cachedBytes", JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_cachedBytes))
                ->Property(
                    "outstandingBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_outstandingBytes))
                ->Property(
                    "highWaterBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_highWaterBytes))
                ->Property("liveCount", JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_liveCount))
                ->Property("cachedCount", JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_cachedCount))
                ->Property(
                    "outstandingCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_outstandingCount))
                ->Property(
                    "highWaterCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&PoolStatistics::m_highWaterCount));

            behaviorContext->Class<ResourceStatistics>("ResourceStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "retainedBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&ResourceStatistics::m_retainedBytes))
                ->Property("capacity", JOLT_BEHAVIOR_READONLY_PROPERTY(&ResourceStatistics::m_capacity))
                ->Property("count", JOLT_BEHAVIOR_READONLY_PROPERTY(&ResourceStatistics::m_count))
                ->Property(
                    "highWaterCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&ResourceStatistics::m_highWaterCount));

            behaviorContext->Class<WorldPerformanceStatistics>("WorldPerformanceStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "availableFlags",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_availableFlags))
                ->Property(
                    "enabledFlags",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_enabledFlags))
                ->Property(
                    "intervalNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_intervalNanoseconds))
                ->Property(
                    "processNativeAllocatedBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_processNativeAllocatedBytes))
                ->Property(
                    "processNativePeakAllocatedBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_processNativePeakAllocatedBytes))
                ->Property(
                    "processNativeAllocationCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_processNativeAllocationCount))
                ->Property(
                    "processNativeFreeCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_processNativeFreeCount))
                ->Property(
                    "processNativeReallocationCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_processNativeReallocationCount))
                ->Property(
                    "tempAllocatorCapacityBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_tempAllocatorCapacityBytes))
                ->Property(
                    "tempAllocatorCurrentBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_tempAllocatorCurrentBytes))
                ->Property(
                    "tempAllocatorPeakBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_tempAllocatorPeakBytes))
                ->Property(
                    "wrapperRetainedBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_wrapperRetainedBytes))
                ->Property("bodies", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_bodies))
                ->Property("characters", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_characters))
                ->Property("constraints", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_constraints))
                ->Property(
                    "eventBatches",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_eventBatches))
                ->Property("hair", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_hair))
                ->Property("ragdolls", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_ragdolls))
                ->Property("scenes", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_scenes))
                ->Property("shapes", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_shapes))
                ->Property("softBodies", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_softBodies))
                ->Property(
                    "stateSnapshots",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_stateSnapshots))
                ->Property("vehicles", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_vehicles))
                ->Property(
                    "virtualCharacters",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_virtualCharacters))
                ->Property(
                    "operations",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_operations))
                ->Property(
                    "broadPhaseOptimizeCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_broadPhaseOptimizeCount))
                ->Property(
                    "broadPhaseOptimizeNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_broadPhaseOptimizeNanoseconds))
                ->Property(
                    "originShiftCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_originShiftCount))
                ->Property(
                    "contactEventCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_contactEventCount))
                ->Property(
                    "contactManifoldCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_contactManifoldCount))
                ->Property(
                    "contactPointCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_contactPointCount))
                ->Property(
                    "droppedEventCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_droppedEventCount))
                ->Property(
                    "eventHighWaterCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_eventHighWaterCount))
                ->Property(
                    "publishedEventCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_publishedEventCount))
                ->Property(
                    "queryCandidateCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_queryCandidateCount))
                ->Property("queryCount", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_queryCount))
                ->Property(
                    "queryHitCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_queryHitCount))
                ->Property(
                    "queryNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_queryNanoseconds))
                ->Property(
                    "snapshotBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotBytes))
                ->Property(
                    "snapshotCaptureCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotCaptureCount))
                ->Property(
                    "snapshotCaptureNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotCaptureNanoseconds))
                ->Property(
                    "snapshotFailureCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotFailureCount))
                ->Property(
                    "snapshotPeakBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotPeakBytes))
                ->Property(
                    "snapshotRestoreCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotRestoreCount))
                ->Property(
                    "snapshotRestoreNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_snapshotRestoreNanoseconds))
                ->Property("jobCount", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_jobCount))
                ->Property(
                    "jobExecutionNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_jobExecutionNanoseconds))
                ->Property(
                    "jobMaximumQueueLatencyNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_jobMaximumQueueLatencyNanoseconds))
                ->Property(
                    "jobQueueLatencyNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_jobQueueLatencyNanoseconds))
                ->Property(
                    "jobTaskCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_jobTaskCount))
                ->Property(
                    "jobMaximumActiveTaskCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_jobMaximumActiveTaskCount))
                ->Property(
                    "lockContentionCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_lockContentionCount))
                ->Property("lockCount", JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_lockCount))
                ->Property(
                    "lockMaximumWaitNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_lockMaximumWaitNanoseconds))
                ->Property(
                    "lockWaitNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_lockWaitNanoseconds))
                ->Property(
                    "hairReadbackBytes",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_hairReadbackBytes))
                ->Property(
                    "hairReadbackCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_hairReadbackCount))
                ->Property(
                    "hairReadbackNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_hairReadbackNanoseconds))
                ->Property(
                    "hairUpdateCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_hairUpdateCount))
                ->Property(
                    "hairUpdateNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_hairUpdateNanoseconds))
                ->Property(
                    "simulationErrorCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_simulationErrorCount))
                ->Property(
                    "simulationNanoseconds",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_simulationNanoseconds))
                ->Property(
                    "simulationStepCount",
                    JOLT_BEHAVIOR_READONLY_PROPERTY(&WorldPerformanceStatistics::m_simulationStepCount));
        }
    }
} // namespace Jolt
