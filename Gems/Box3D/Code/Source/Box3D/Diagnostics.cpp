/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/DiagnosticsBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Box3D
{
    StatisticsSnapshot StatisticsSnapshot::Create(const WorldStatistics& statistics, bool found)
    {
        StatisticsSnapshot result;
        const StepProfile& profile = statistics.m_lastStep;
        result.m_profile = {
            .m_total = profile.m_total.count(),
            .m_pairGeneration = profile.m_pairGeneration.count(),
            .m_collision = profile.m_collision.count(),
            .m_solver = profile.m_solver.count(),
            .m_solverSetup = profile.m_solverSetup.count(),
            .m_constraints = profile.m_constraints.count(),
            .m_prepareConstraints = profile.m_prepareConstraints.count(),
            .m_integrateVelocities = profile.m_integrateVelocities.count(),
            .m_warmStart = profile.m_warmStart.count(),
            .m_solveImpulses = profile.m_solveImpulses.count(),
            .m_integratePositions = profile.m_integratePositions.count(),
            .m_relaxImpulses = profile.m_relaxImpulses.count(),
            .m_applyRestitution = profile.m_applyRestitution.count(),
            .m_storeImpulses = profile.m_storeImpulses.count(),
            .m_splitIslands = profile.m_splitIslands.count(),
            .m_updateTransforms = profile.m_updateTransforms.count(),
            .m_sensorHits = profile.m_sensorHits.count(),
            .m_jointEvents = profile.m_jointEvents.count(),
            .m_hitEvents = profile.m_hitEvents.count(),
            .m_refitBroadPhase = profile.m_refitBroadPhase.count(),
            .m_continuousCollision = profile.m_continuousCollision.count(),
            .m_sleepIslands = profile.m_sleepIslands.count(),
            .m_sensorOverlap = profile.m_sensorOverlap.count(),
        };
        result.m_counters = statistics.m_counters;
        result.m_capacityHighWaterMarks = statistics.m_capacityHighWaterMarks;
        result.m_worldBounds = statistics.m_worldBounds;
        result.m_simulationTick = statistics.m_simulationTick;
        result.m_found = found;
        return result;
    }

    void ReflectDiagnostics(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EnumProperty<static_cast<AZ::u8>(StatisticsFlags::None)>("StatisticsFlags_None")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(StatisticsFlags::Profile)>("StatisticsFlags_Profile")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(StatisticsFlags::Counters)>("StatisticsFlags_Counters")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(StatisticsFlags::Capacity)>("StatisticsFlags_Capacity")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(StatisticsFlags::Bounds)>("StatisticsFlags_Bounds")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(StatisticsFlags::All)>("StatisticsFlags_All")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->Class<StepProfileSnapshot>("StepProfileSnapshot")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("total", BehaviorValueProperty(&StepProfileSnapshot::m_total))
            ->Property("pairGeneration", BehaviorValueProperty(&StepProfileSnapshot::m_pairGeneration))
            ->Property("collision", BehaviorValueProperty(&StepProfileSnapshot::m_collision))
            ->Property("solver", BehaviorValueProperty(&StepProfileSnapshot::m_solver))
            ->Property("solverSetup", BehaviorValueProperty(&StepProfileSnapshot::m_solverSetup))
            ->Property("constraints", BehaviorValueProperty(&StepProfileSnapshot::m_constraints))
            ->Property("prepareConstraints", BehaviorValueProperty(&StepProfileSnapshot::m_prepareConstraints))
            ->Property("integrateVelocities", BehaviorValueProperty(&StepProfileSnapshot::m_integrateVelocities))
            ->Property("warmStart", BehaviorValueProperty(&StepProfileSnapshot::m_warmStart))
            ->Property("solveImpulses", BehaviorValueProperty(&StepProfileSnapshot::m_solveImpulses))
            ->Property("integratePositions", BehaviorValueProperty(&StepProfileSnapshot::m_integratePositions))
            ->Property("relaxImpulses", BehaviorValueProperty(&StepProfileSnapshot::m_relaxImpulses))
            ->Property("applyRestitution", BehaviorValueProperty(&StepProfileSnapshot::m_applyRestitution))
            ->Property("storeImpulses", BehaviorValueProperty(&StepProfileSnapshot::m_storeImpulses))
            ->Property("splitIslands", BehaviorValueProperty(&StepProfileSnapshot::m_splitIslands))
            ->Property("updateTransforms", BehaviorValueProperty(&StepProfileSnapshot::m_updateTransforms))
            ->Property("sensorHits", BehaviorValueProperty(&StepProfileSnapshot::m_sensorHits))
            ->Property("jointEvents", BehaviorValueProperty(&StepProfileSnapshot::m_jointEvents))
            ->Property("hitEvents", BehaviorValueProperty(&StepProfileSnapshot::m_hitEvents))
            ->Property("refitBroadPhase", BehaviorValueProperty(&StepProfileSnapshot::m_refitBroadPhase))
            ->Property("continuousCollision", BehaviorValueProperty(&StepProfileSnapshot::m_continuousCollision))
            ->Property("sleepIslands", BehaviorValueProperty(&StepProfileSnapshot::m_sleepIslands))
            ->Property("sensorOverlap", BehaviorValueProperty(&StepProfileSnapshot::m_sensorOverlap));
        behaviorContext->Class<SimulationCounters>("SimulationCounters")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("workerCount", BehaviorValueProperty(&SimulationCounters::m_workerCount))
            ->Property("bodyCount", BehaviorValueProperty(&SimulationCounters::m_bodyCount))
            ->Property("awakeBodyCount", BehaviorValueProperty(&SimulationCounters::m_awakeBodyCount))
            ->Property("shapeCount", BehaviorValueProperty(&SimulationCounters::m_shapeCount))
            ->Property("contactCount", BehaviorValueProperty(&SimulationCounters::m_contactCount))
            ->Property("awakeContactCount", BehaviorValueProperty(&SimulationCounters::m_awakeContactCount))
            ->Property("recycledContactCount", BehaviorValueProperty(&SimulationCounters::m_recycledContactCount))
            ->Property("jointCount", BehaviorValueProperty(&SimulationCounters::m_jointCount))
            ->Property("islandCount", BehaviorValueProperty(&SimulationCounters::m_islandCount))
            ->Property("taskCount", BehaviorValueProperty(&SimulationCounters::m_taskCount))
            ->Property("staticTreeHeight", BehaviorValueProperty(&SimulationCounters::m_staticTreeHeight))
            ->Property("dynamicTreeHeight", BehaviorValueProperty(&SimulationCounters::m_dynamicTreeHeight))
            ->Property("separatingAxisCallCount", BehaviorValueProperty(&SimulationCounters::m_separatingAxisCallCount))
            ->Property("separatingAxisCacheHitCount", BehaviorValueProperty(&SimulationCounters::m_separatingAxisCacheHitCount))
            ->Property("maximumToiDistanceIterations", BehaviorValueProperty(&SimulationCounters::m_maxToiDistanceIterations))
            ->Property("maximumToiPushBackIterations", BehaviorValueProperty(&SimulationCounters::m_maxToiPushBackIterations))
            ->Property("maximumToiRootIterations", BehaviorValueProperty(&SimulationCounters::m_maxToiRootIterations))
            ->Property("stackHighWaterBytes", BehaviorValueProperty(&SimulationCounters::m_stackHighWaterBytes))
            ->Property("largestWorkerArenaPeakBytes", BehaviorValueProperty(&SimulationCounters::m_largestWorkerArenaPeakBytes))
            ->Property("globalAllocatedBytes", BehaviorValueProperty(&SimulationCounters::m_globalAllocatedBytes));
        behaviorContext->Class<CapacityHighWaterMarks>("CapacityHighWaterMarks")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("staticShapes", BehaviorValueProperty(&CapacityHighWaterMarks::m_staticShapes))
            ->Property("dynamicShapes", BehaviorValueProperty(&CapacityHighWaterMarks::m_dynamicShapes))
            ->Property("staticBodies", BehaviorValueProperty(&CapacityHighWaterMarks::m_staticBodies))
            ->Property("dynamicBodies", BehaviorValueProperty(&CapacityHighWaterMarks::m_dynamicBodies))
            ->Property("contacts", BehaviorValueProperty(&CapacityHighWaterMarks::m_contacts));
        behaviorContext->Class<StatisticsSnapshot>("StatisticsSnapshot")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("profile", BehaviorValueProperty(&StatisticsSnapshot::m_profile))
            ->Property("counters", BehaviorValueProperty(&StatisticsSnapshot::m_counters))
            ->Property("capacityHighWaterMarks", BehaviorValueProperty(&StatisticsSnapshot::m_capacityHighWaterMarks))
            ->Property("worldBounds", BehaviorValueProperty(&StatisticsSnapshot::m_worldBounds))
            ->Property("simulationTick", BehaviorValueProperty(&StatisticsSnapshot::m_simulationTick))
            ->Property("found", BehaviorValueProperty(&StatisticsSnapshot::m_found))
            ->Method("GetConstraintGraphOccupancy", &StatisticsSnapshot::GetConstraintGraphOccupancy)
            ->Method("GetContactManifoldCount", &StatisticsSnapshot::GetContactManifoldCount);
        behaviorContext->Class<RecordingData>("RecordingData")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Method("GetByteCount", &RecordingData::GetByteCount)
            ->Method("GetByte", &RecordingData::GetByte);
        behaviorContext->Class<RecordingResult>("RecordingResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("data", BehaviorValueProperty(&RecordingResult::m_data))
            ->Property("stopped", BehaviorValueProperty(&RecordingResult::m_stopped));
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::OverlapAabb)>("ReplayQueryType_OverlapAabb")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::OverlapShape)>("ReplayQueryType_OverlapShape")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::Raycast)>("ReplayQueryType_Raycast")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::ShapeCast)>("ReplayQueryType_ShapeCast")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::ClosestRaycast)>("ReplayQueryType_ClosestRaycast")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::MoverCast)>("ReplayQueryType_MoverCast")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->EnumProperty<static_cast<AZ::u8>(ReplayQueryType::MoverCollision)>("ReplayQueryType_MoverCollision")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d");
        behaviorContext->Class<ReplayInfo>("ReplayInfo")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("frameCount", BehaviorValueProperty(&ReplayInfo::m_frameCount))
            ->Property("workerCount", BehaviorValueProperty(&ReplayInfo::m_workerCount))
            ->Property("timeStep", BehaviorValueProperty(&ReplayInfo::m_timeStep))
            ->Property("subStepCount", BehaviorValueProperty(&ReplayInfo::m_subStepCount))
            ->Property("lengthScale", BehaviorValueProperty(&ReplayInfo::m_lengthScale))
            ->Property("bounds", BehaviorValueProperty(&ReplayInfo::m_bounds));
        behaviorContext->Class<ReplayBody>("ReplayBody")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("transform", BehaviorValueProperty(&ReplayBody::m_transform))
            ->Property("linearVelocity", BehaviorValueProperty(&ReplayBody::m_linearVelocity))
            ->Property("angularVelocity", BehaviorValueProperty(&ReplayBody::m_angularVelocity))
            ->Property("exists", BehaviorValueProperty(&ReplayBody::m_exists))
            ->Property("awake", BehaviorValueProperty(&ReplayBody::m_awake));
        behaviorContext->Class<ReplayQuery>("ReplayQuery")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("type", BehaviorValueProperty(&ReplayQuery::m_type))
            ->Property("categoryBits", BehaviorValueProperty(&ReplayQuery::m_categoryBits))
            ->Property("maskBits", BehaviorValueProperty(&ReplayQuery::m_maskBits))
            ->Property("bounds", BehaviorValueProperty(&ReplayQuery::m_bounds))
            ->Property("origin", BehaviorValueProperty(&ReplayQuery::m_origin))
            ->Property("translation", BehaviorValueProperty(&ReplayQuery::m_translation))
            ->Property("hitCount", BehaviorValueProperty(&ReplayQuery::m_hitCount))
            ->Property("key", BehaviorValueProperty(&ReplayQuery::m_key))
            ->Property("id", BehaviorValueProperty(&ReplayQuery::m_id))
            ->Property("name", BehaviorValueProperty(&ReplayQuery::m_name));
        behaviorContext->Class<ReplayShapeId>("ReplayShapeId")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Method("IsValid", &ReplayShapeId::IsValid)
            ->Property("index", BehaviorValueProperty(&ReplayShapeId::m_index))
            ->Property("generation", BehaviorValueProperty(&ReplayShapeId::m_generation));
        behaviorContext->Class<ReplayQueryHit>("ReplayQueryHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("shapeId", BehaviorValueProperty(&ReplayQueryHit::m_shapeId))
            ->Property("position", BehaviorValueProperty(&ReplayQueryHit::m_position))
            ->Property("normal", BehaviorValueProperty(&ReplayQueryHit::m_normal))
            ->Property("fraction", BehaviorValueProperty(&ReplayQueryHit::m_fraction));
        behaviorContext->Class<ReplayInfoResult>("ReplayInfoResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("info", BehaviorValueProperty(&ReplayInfoResult::m_info))
            ->Property("found", BehaviorValueProperty(&ReplayInfoResult::m_found));
        behaviorContext->Class<ReplayBodyResult>("ReplayBodyResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("body", BehaviorValueProperty(&ReplayBodyResult::m_body))
            ->Property("found", BehaviorValueProperty(&ReplayBodyResult::m_found));
        behaviorContext->Class<ReplayQueryResult>("ReplayQueryResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("query", BehaviorValueProperty(&ReplayQueryResult::m_query))
            ->Property("found", BehaviorValueProperty(&ReplayQueryResult::m_found));
        behaviorContext->Class<ReplayQueryHitResult>("ReplayQueryHitResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("hit", BehaviorValueProperty(&ReplayQueryHitResult::m_hit))
            ->Property("found", BehaviorValueProperty(&ReplayQueryHitResult::m_found));
        behaviorContext->EBus<DiagnosticsRequestBus>("Box3DDiagnosticsRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Attribute(AZ::Script::Attributes::Category, "Box3D")
            ->Event("GetWorldStatistics", &DiagnosticsRequests::GetWorldStatistics)
            ->Event("StartRecording", &DiagnosticsRequests::StartRecording)
            ->Event("StopRecording", &DiagnosticsRequests::StopRecording)
            ->Event("ValidateRecording", &DiagnosticsRequests::ValidateRecording)
            ->Event("CreateReplay", &DiagnosticsRequests::CreateReplay)
            ->Event("DestroyReplay", &DiagnosticsRequests::DestroyReplay)
            ->Event("IsReplayValid", &DiagnosticsRequests::IsReplayValid)
            ->Event("GetReplayInfo", &DiagnosticsRequests::GetReplayInfo)
            ->Event("StepReplay", &DiagnosticsRequests::StepReplay)
            ->Event("RestartReplay", &DiagnosticsRequests::RestartReplay)
            ->Event("SeekReplay", &DiagnosticsRequests::SeekReplay)
            ->Event("GetReplayFrame", &DiagnosticsRequests::GetReplayFrame)
            ->Event("IsReplayAtEnd", &DiagnosticsRequests::IsReplayAtEnd)
            ->Event("HasReplayDiverged", &DiagnosticsRequests::HasReplayDiverged)
            ->Event("GetReplayDivergenceFrame", &DiagnosticsRequests::GetReplayDivergenceFrame)
            ->Event("SetReplayWorkerCount", &DiagnosticsRequests::SetReplayWorkerCount)
            ->Event("SetReplayKeyframePolicy", &DiagnosticsRequests::SetReplayKeyframePolicy)
            ->Event("GetReplayKeyframeBudget", &DiagnosticsRequests::GetReplayKeyframeBudget)
            ->Event("GetReplayMinimumKeyframeInterval", &DiagnosticsRequests::GetReplayMinimumKeyframeInterval)
            ->Event("GetReplayKeyframeInterval", &DiagnosticsRequests::GetReplayKeyframeInterval)
            ->Event("GetReplayKeyframeBytes", &DiagnosticsRequests::GetReplayKeyframeBytes)
            ->Event("GetReplayBodyCount", &DiagnosticsRequests::GetReplayBodyCount)
            ->Event("GetReplayBody", &DiagnosticsRequests::GetReplayBody)
            ->Event("GetReplayQueryCount", &DiagnosticsRequests::GetReplayQueryCount)
            ->Event("GetReplayQuery", &DiagnosticsRequests::GetReplayQuery)
            ->Event("GetReplayQueryHit", &DiagnosticsRequests::GetReplayQueryHit)
            ->Event("RebuildStaticTree", &DiagnosticsRequests::RebuildStaticTree);
    }
} // namespace Box3D
