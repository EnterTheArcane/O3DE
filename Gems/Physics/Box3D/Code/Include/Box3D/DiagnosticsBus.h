/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Diagnostics.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    struct StepProfileSnapshot final
    {
        AZ_TYPE_INFO(StepProfileSnapshot, StepProfileSnapshotTypeId);

        float m_total = 0.0f;
        float m_pairGeneration = 0.0f;
        float m_collision = 0.0f;

        float m_solver = 0.0f;
        float m_solverSetup = 0.0f;
        float m_constraints = 0.0f;
        float m_prepareConstraints = 0.0f;
        float m_integrateVelocities = 0.0f;
        float m_warmStart = 0.0f;
        float m_solveImpulses = 0.0f;
        float m_integratePositions = 0.0f;
        float m_relaxImpulses = 0.0f;
        float m_applyRestitution = 0.0f;
        float m_storeImpulses = 0.0f;

        float m_splitIslands = 0.0f;
        float m_updateTransforms = 0.0f;

        float m_sensorHits = 0.0f;
        float m_jointEvents = 0.0f;
        float m_hitEvents = 0.0f;

        float m_refitBroadPhase = 0.0f;
        float m_continuousCollision = 0.0f;
        float m_sleepIslands = 0.0f;
        float m_sensorOverlap = 0.0f;
    };

    struct StatisticsSnapshot final
    {
        AZ_TYPE_INFO(StatisticsSnapshot, StatisticsSnapshotTypeId);

        [[nodiscard]]
        static StatisticsSnapshot Create(
            const WorldStatistics& statistics,
            bool found);

        [[nodiscard]]
        AZ::u32 GetConstraintGraphOccupancy(
            size_t index) const
        {
            if (index < m_counters.m_constraintGraphOccupancy.size())
            {
                return m_counters.m_constraintGraphOccupancy[index];
            }

            return 0;
        }

        [[nodiscard]]
        AZ::u32 GetContactManifoldCount(
            size_t index) const
        {
            if (index < m_counters.m_contactManifoldHistogram.size())
            {
                return m_counters.m_contactManifoldHistogram[index];
            }

            return 0;
        }

        StepProfileSnapshot m_profile;
        SimulationCounters m_counters;
        CapacityHighWaterMarks m_capacityHighWaterMarks;

        AZ::Aabb m_worldBounds = AZ::Aabb::CreateNull();
        AZ::u64 m_simulationTick = 0;

        bool m_found = false;
    };

    struct RecordingData final
    {
        AZ_TYPE_INFO(RecordingData, RecordingDataTypeId);

        RecordingData() = default;

        explicit RecordingData(
            AZStd::vector<AZ::u8>&& data)
            : m_data(AZStd::move(data))
        {
        }

        [[nodiscard]]
        size_t GetByteCount() const
        {
            return m_data.size();
        }

        [[nodiscard]]
        AZ::u8 GetByte(
            size_t index) const
        {
            if (index < m_data.size())
            {
                return m_data[index];
            }

            return 0;
        }

        [[nodiscard]]
        AZStd::span<const AZ::u8> GetData() const
        {
            return m_data;
        }

    private:
        AZStd::vector<AZ::u8> m_data;
    };

    struct RecordingResult final
    {
        AZ_TYPE_INFO(RecordingResult, RecordingResultTypeId);

        RecordingData m_data;
        bool m_stopped = false;
    };

    struct ReplayInfoResult final
    {
        AZ_TYPE_INFO(ReplayInfoResult, ReplayInfoResultTypeId);

        ReplayInfo m_info;
        bool m_found = false;
    };

    struct ReplayBodyResult final
    {
        AZ_TYPE_INFO(ReplayBodyResult, ReplayBodyResultTypeId);

        ReplayBody m_body;
        bool m_found = false;
    };

    struct ReplayQueryResult final
    {
        AZ_TYPE_INFO(ReplayQueryResult, ReplayQueryResultTypeId);

        ReplayQuery m_query;
        bool m_found = false;
    };

    struct ReplayQueryHitResult final
    {
        AZ_TYPE_INFO(ReplayQueryHitResult, ReplayQueryHitResultTypeId);

        ReplayQueryHit m_hit;
        bool m_found = false;
    };

    class DiagnosticsRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        [[nodiscard]]
        virtual StatisticsSnapshot GetWorldStatistics(
            WorldHandle worldHandle,
            StatisticsFlags flags) const = 0;

        [[nodiscard]]
        virtual bool StartRecording(
            WorldHandle worldHandle,
            size_t initialCapacityBytes) = 0;

        [[nodiscard]]
        virtual RecordingResult StopRecording(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual bool ValidateRecording(
            const RecordingData& data,
            AZ::u32 workerCount) const = 0;

        [[nodiscard]]
        virtual ReplayHandle CreateReplay(
            const RecordingData& data,
            AZ::u32 workerCount) = 0;

        virtual bool DestroyReplay(ReplayHandle replayHandle) = 0;

        [[nodiscard]]
        virtual bool IsReplayValid(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual ReplayInfoResult GetReplayInfo(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual bool StepReplay(ReplayHandle replayHandle) = 0;

        virtual bool RestartReplay(ReplayHandle replayHandle) = 0;

        virtual bool SeekReplay(
            ReplayHandle replayHandle,
            AZ::u32 frame) = 0;

        [[nodiscard]]
        virtual AZ::u32 GetReplayFrame(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual bool IsReplayAtEnd(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual bool HasReplayDiverged(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual AZ::s32 GetReplayDivergenceFrame(ReplayHandle replayHandle) const = 0;

        virtual bool SetReplayWorkerCount(
            ReplayHandle replayHandle,
            AZ::u32 workerCount) = 0;

        virtual bool SetReplayKeyframePolicy(
            ReplayHandle replayHandle,
            size_t budgetBytes,
            AZ::u32 minimumInterval) = 0;

        [[nodiscard]]
        virtual size_t GetReplayKeyframeBudget(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetReplayMinimumKeyframeInterval(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetReplayKeyframeInterval(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual size_t GetReplayKeyframeBytes(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetReplayBodyCount(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual ReplayBodyResult GetReplayBody(
            ReplayHandle replayHandle,
            AZ::u32 index) const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetReplayQueryCount(ReplayHandle replayHandle) const = 0;

        [[nodiscard]]
        virtual ReplayQueryResult GetReplayQuery(
            ReplayHandle replayHandle,
            AZ::u32 index) const = 0;

        [[nodiscard]]
        virtual ReplayQueryHitResult GetReplayQueryHit(
            ReplayHandle replayHandle,
            AZ::u32 queryIndex,
            AZ::u32 hitIndex) const = 0;

        [[nodiscard]]
        virtual bool RebuildStaticTree(WorldHandle worldHandle) = 0;
    };

    using DiagnosticsRequestBus = AZ::EBus<DiagnosticsRequests>;

    void ReflectDiagnostics(AZ::ReflectContext* context);
} // namespace Box3D
