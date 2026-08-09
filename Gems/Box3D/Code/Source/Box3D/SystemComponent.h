/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/CookingBus.h>
#include <Box3D/DiagnosticsBus.h>
#include <Box3D/MaterialBus.h>
#include <Box3D/SystemConfiguration.h>
#include <Box3D/TypeIds.h>
#include <Box3D/WorldBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Box3D
{
    class System;

    class SystemComponent final
        : public AZ::Component
        , private AZ::TickBus::Handler
        , private CookingRequestBus::Handler
        , private DiagnosticsRequestBus::Handler
        , private MaterialRequestBus::Handler
        , private WorldRequestBus::Handler
    {
    public:
        AZ_COMPONENT(SystemComponent, SystemComponentTypeId);

        SystemComponent();
        ~SystemComponent() override;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void Activate() override;
        void Deactivate() override;

        [[nodiscard]] WorldHandle GetDefaultWorldHandle() const override;
        [[nodiscard]] CookedShapeHandle CookShape(const ShapeConfiguration& configuration) override;
        bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle) override;
        [[nodiscard]] bool IsCookedShapeValid(CookedShapeHandle cookedShapeHandle) const override;
        [[nodiscard]] AZ::Aabb GetCookedShapeAabb(CookedShapeHandle cookedShapeHandle) const override;
        [[nodiscard]] CookedRaycastResult RaycastCookedShape(
            CookedShapeHandle cookedShapeHandle, const AZ::Vector3& start, const AZ::Vector3& direction, float distance) const override;
        [[nodiscard]] StatisticsSnapshot GetWorldStatistics(WorldHandle worldHandle, StatisticsFlags flags) const override;
        [[nodiscard]] bool StartRecording(WorldHandle worldHandle, size_t initialCapacityBytes) override;
        [[nodiscard]] RecordingResult StopRecording(WorldHandle worldHandle) override;
        [[nodiscard]] bool ValidateRecording(const RecordingData& data, AZ::u32 workerCount) const override;
        [[nodiscard]] ReplayHandle CreateReplay(const RecordingData& data, AZ::u32 workerCount) override;
        bool DestroyReplay(ReplayHandle replayHandle) override;
        [[nodiscard]] bool IsReplayValid(ReplayHandle replayHandle) const override;
        [[nodiscard]] ReplayInfoResult GetReplayInfo(ReplayHandle replayHandle) const override;
        [[nodiscard]] bool StepReplay(ReplayHandle replayHandle) override;
        bool RestartReplay(ReplayHandle replayHandle) override;
        bool SeekReplay(ReplayHandle replayHandle, AZ::u32 frame) override;
        [[nodiscard]] AZ::u32 GetReplayFrame(ReplayHandle replayHandle) const override;
        [[nodiscard]] bool IsReplayAtEnd(ReplayHandle replayHandle) const override;
        [[nodiscard]] bool HasReplayDiverged(ReplayHandle replayHandle) const override;
        [[nodiscard]] AZ::s32 GetReplayDivergenceFrame(ReplayHandle replayHandle) const override;
        bool SetReplayWorkerCount(ReplayHandle replayHandle, AZ::u32 workerCount) override;
        bool SetReplayKeyframePolicy(ReplayHandle replayHandle, size_t budgetBytes, AZ::u32 minimumInterval) override;
        [[nodiscard]] size_t GetReplayKeyframeBudget(ReplayHandle replayHandle) const override;
        [[nodiscard]] AZ::u32 GetReplayMinimumKeyframeInterval(ReplayHandle replayHandle) const override;
        [[nodiscard]] AZ::u32 GetReplayKeyframeInterval(ReplayHandle replayHandle) const override;
        [[nodiscard]] size_t GetReplayKeyframeBytes(ReplayHandle replayHandle) const override;
        [[nodiscard]] AZ::u32 GetReplayBodyCount(ReplayHandle replayHandle) const override;
        [[nodiscard]] ReplayBodyResult GetReplayBody(ReplayHandle replayHandle, AZ::u32 index) const override;
        [[nodiscard]] AZ::u32 GetReplayQueryCount(ReplayHandle replayHandle) const override;
        [[nodiscard]] ReplayQueryResult GetReplayQuery(ReplayHandle replayHandle, AZ::u32 index) const override;
        [[nodiscard]] ReplayQueryHitResult GetReplayQueryHit(
            ReplayHandle replayHandle, AZ::u32 queryIndex, AZ::u32 hitIndex) const override;
        [[nodiscard]] bool RebuildStaticTree(WorldHandle worldHandle) override;
        [[nodiscard]] MaterialHandle CreateMaterial(const MaterialConfiguration& configuration) override;
        bool UpdateMaterial(MaterialHandle materialHandle, const MaterialConfiguration& configuration) override;
        [[nodiscard]] MaterialResult GetMaterial(MaterialHandle materialHandle) const override;
        bool DestroyMaterial(MaterialHandle materialHandle) override;
        [[nodiscard]] WorldHandle CreateWorld(const WorldConfiguration& configuration) override;
        bool DestroyWorld(WorldHandle worldHandle) override;
        bool StepWorld(WorldHandle worldHandle, float fixedTimeStep) override;
        [[nodiscard]] ClosestQueryResult RaycastClosest(WorldHandle worldHandle, const RaycastRequest& request) const override;
        [[nodiscard]] ClosestQueryResultCollection RaycastClosestBatch(
            WorldHandle worldHandle, const RaycastRequestCollection& requests) const override;
        [[nodiscard]] QueryHitCollection Raycast(
            WorldHandle worldHandle, const RaycastRequest& request, AZ::u32 maxHitCount) const override;
        [[nodiscard]] QueryHitCollection OverlapAabb(
            WorldHandle worldHandle, const AabbOverlapRequest& request, AZ::u32 maxHitCount) const override;
        [[nodiscard]] size_t GetContactPointCount(WorldHandle worldHandle) const override;
        [[nodiscard]] ContactPoint GetContactPointAt(WorldHandle worldHandle, size_t index) const override;
        [[nodiscard]] QueryHitCollection ShapeCastSphere(
            WorldHandle worldHandle, const SphereShapeConfiguration& geometry, const ConvexCastParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection OverlapSphere(
            WorldHandle worldHandle, const SphereShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection ShapeCastCapsule(
            WorldHandle worldHandle, const CapsuleShapeConfiguration& geometry, const ConvexCastParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection OverlapCapsule(
            WorldHandle worldHandle, const CapsuleShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection ShapeCastBox(
            WorldHandle worldHandle, const BoxShapeConfiguration& geometry, const ConvexCastParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection OverlapBox(
            WorldHandle worldHandle, const BoxShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection ShapeCastCylinder(
            WorldHandle worldHandle, const CylinderShapeConfiguration& geometry, const ConvexCastParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection OverlapCylinder(
            WorldHandle worldHandle, const CylinderShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection ShapeCastConvexHull(
            WorldHandle worldHandle, const ConvexHullShapeConfiguration& geometry, const ConvexCastParameters& parameters) const override;
        [[nodiscard]] QueryHitCollection OverlapConvexHull(
            WorldHandle worldHandle,
            const ConvexHullShapeConfiguration& geometry,
            const ConvexOverlapParameters& parameters) const override;

    private:
        struct ReplaySlot final
        {
            AZStd::unique_ptr<IReplay> m_replay;
            AZ::u32 m_generation = 1;
        };

        [[nodiscard]] IReplay* FindReplay(ReplayHandle replayHandle);
        [[nodiscard]] const IReplay* FindReplay(ReplayHandle replayHandle) const;

        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        int GetTickOrder() override;

        SystemConfiguration m_configuration;
        AZ::JobContext* m_jobContext = nullptr;
        AZStd::unique_ptr<System> m_system;
        mutable AZStd::mutex m_replayMutex;
        AZStd::vector<ReplaySlot> m_replaySlots;
        AZStd::vector<AZ::u32> m_freeReplaySlots;
    };
} // namespace Box3D
