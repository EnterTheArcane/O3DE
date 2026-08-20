/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SystemConfiguration.h>
#include <Jolt/SkeletonBus.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldQueryBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class EventBatch;
    class System;
    class Runtime;
    class SceneAssetHandler;
    class SkeletonAssetHandler;

    class JOLT_API SystemComponent final
        : public AZ::Component
        , private AZ::TickBus::Handler
        , private SkeletonRequestBus::Handler
        , private WorldQueryRequestBus::Handler
    {
    public:
        AZ_COMPONENT(SystemComponent, SystemComponentTypeId);

        SystemComponent();
        ~SystemComponent() override;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;

        void Deactivate() override;

    private:
        void OnTick(
            float deltaTime,
            AZ::ScriptTimePoint time) override;

        void DispatchWorldEvents(
            WorldHandle worldHandle,
            const EventBatch& events);

        SkeletonDefinitionHandle CreateSkeletonDefinition(
            const SkeletonDefinitionConfiguration& configuration) override;

        bool DestroySkeletonDefinition(SkeletonDefinitionHandle skeletonHandle) override;

        [[nodiscard]]
        bool IsSkeletonDefinitionValid(SkeletonDefinitionHandle skeletonHandle) const override;

        [[nodiscard]]
        AZStd::vector<SkeletonJoint> CopySkeletonJoints(
            SkeletonDefinitionHandle skeletonHandle) const override;

        [[nodiscard]]
        AZ::s32 FindSkeletonJoint(
            SkeletonDefinitionHandle skeletonHandle,
            AZ::Name jointName) const override;

        SkeletalAnimationHandle CreateSkeletalAnimation(
            const SkeletalAnimationConfiguration& configuration) override;

        bool UpdateSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            const SkeletalAnimationConfiguration& configuration) override;

        bool DestroySkeletalAnimation(SkeletalAnimationHandle animationHandle) override;

        [[nodiscard]]
        bool IsSkeletalAnimationValid(SkeletalAnimationHandle animationHandle) const override;

        bool GetSkeletalAnimationState(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationState& state) const override;

        [[nodiscard]]
        AZ::Name GetSkeletalAnimatedJointName(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex) const override;

        [[nodiscard]]
        AZStd::vector<SkeletalAnimationKeyframe> CopySkeletalAnimationKeyframes(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex) const override;

        bool SetSkeletalAnimationLooping(
            SkeletalAnimationHandle animationHandle,
            bool isLooping) override;

        bool ScaleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            float scale) override;

        SkeletonPoseHandle CreateSkeletonPose(SkeletonDefinitionHandle skeletonHandle) override;

        bool DestroySkeletonPose(SkeletonPoseHandle poseHandle) override;

        [[nodiscard]]
        bool IsSkeletonPoseValid(SkeletonPoseHandle poseHandle) const override;

        bool GetSkeletonPoseState(
            SkeletonPoseHandle poseHandle,
            SkeletonPoseState& state) const override;

        bool SetSkeletonPoseRootOffset(
            SkeletonPoseHandle poseHandle,
            const WorldPosition& rootOffset) override;

        bool SetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            const AZStd::vector<AZ::Transform>& localTransforms) override;

        bool SetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            const AZStd::vector<AZ::Transform>& modelTransforms) override;

        [[nodiscard]]
        AZStd::vector<AZ::Transform> CopySkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle) const override;

        [[nodiscard]]
        AZStd::vector<AZ::Transform> CopySkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle) const override;

        bool SampleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletonPoseHandle poseHandle,
            float time) override;

        SkeletonMapperHandle CreateSkeletonMapper(
            const SkeletonMapperConfiguration& configuration) override;

        bool DestroySkeletonMapper(SkeletonMapperHandle mapperHandle) override;

        [[nodiscard]]
        bool IsSkeletonMapperValid(SkeletonMapperHandle mapperHandle) const override;

        bool GetSkeletonMapperState(
            SkeletonMapperHandle mapperHandle,
            SkeletonMapperState& state) const override;

        [[nodiscard]]
        AZStd::vector<SkeletonMapperMappingState> CopySkeletonMapperMappings(
            SkeletonMapperHandle mapperHandle) const override;

        bool GetSkeletonMapperChainState(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            SkeletonMapperChainState& state) const override;

        [[nodiscard]]
        AZStd::vector<AZ::u32> CopySkeletonMapperSourceChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex) const override;

        [[nodiscard]]
        AZStd::vector<AZ::u32> CopySkeletonMapperTargetChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex) const override;

        [[nodiscard]]
        AZStd::vector<SkeletonMapperUnmappedJoint> CopySkeletonMapperUnmappedJoints(
            SkeletonMapperHandle mapperHandle) const override;

        [[nodiscard]]
        AZStd::vector<SkeletonMapperLockedTranslation> CopySkeletonMapperLockedTranslations(
            SkeletonMapperHandle mapperHandle) const override;

        [[nodiscard]]
        AZ::s32 FindMappedSkeletonJoint(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 sourceJointIndex) const override;

        [[nodiscard]]
        bool IsSkeletonJointTranslationLocked(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 targetJointIndex) const override;

        [[nodiscard]]
        AZStd::vector<AZ::Transform> MapSkeletonPose(
            SkeletonMapperHandle mapperHandle,
            const AZStd::vector<AZ::Transform>& sourceModelTransforms,
            const AZStd::vector<AZ::Transform>& targetLocalTransforms) const override;

        [[nodiscard]]
        AZStd::vector<AZ::Transform> MapSkeletonPoseReverse(
            SkeletonMapperHandle mapperHandle,
            const AZStd::vector<AZ::Transform>& targetModelTransforms) const override;

        [[nodiscard]]
        WorldHandle CreateWorld(const WorldConfiguration& configuration) override;

        bool DestroyWorld(WorldHandle worldHandle) override;

        [[nodiscard]]
        WorldHandle GetDefaultWorldHandle() const override;

        [[nodiscard]]
        RuntimeInfo GetRuntimeInfo() const override;

        [[nodiscard]]
        bool IsWorldValid(WorldHandle worldHandle) const override;

        [[nodiscard]]
        SimulationResult StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep) override;

        [[nodiscard]]
        AZ::Vector3 GetGravity(WorldHandle worldHandle) const override;

        bool SetGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity) override;

        [[nodiscard]]
        SimulationConfiguration GetSimulationConfiguration(WorldHandle worldHandle) const override;

        bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration) override;

        [[nodiscard]]
        WorldRuntimeConfiguration GetRuntimeConfiguration(WorldHandle worldHandle) const override;

        bool UpdateRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration) override;

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle) override;

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldStateConfigured(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles) override;

        [[nodiscard]]
        AZStd::vector<StateSnapshotHandle> CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles,
            const AZStd::vector<AZ::u32>& partitionBodyCounts) override;

        bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            const AZStd::vector<StateSnapshotHandle>& snapshotHandles,
            StateSnapshotArchive& archive) override;

        [[nodiscard]]
        AZStd::vector<StateSnapshotHandle> ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive) override;

        bool RecaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) override;

        bool RecaptureWorldStateConfigured(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles) override;

        bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) override;

        [[nodiscard]]
        bool IsStateSnapshotValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const override;

        StateRestoreResult RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) override;

        StateRestoreResult RestoreWorldStateParts(
            WorldHandle worldHandle,
            const AZStd::vector<StateSnapshotHandle>& snapshotHandles) override;

        bool ValidateWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result) override;

        [[nodiscard]]
        bool GetWorldStateDigest(
            WorldHandle worldHandle,
            WorldStateDigest& digest) const override;

        [[nodiscard]]
        bool GetWorldStatistics(
            WorldHandle worldHandle,
            WorldStatistics& statistics) const override;

        bool ConfigurePerformanceStatistics(
            WorldHandle worldHandle,
            PerformanceStatisticsFlags flags) override;

        [[nodiscard]]
        bool GetPerformanceStatistics(
            WorldHandle worldHandle,
            WorldPerformanceStatistics& statistics,
            bool reset) override;

        bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration) override;

        [[nodiscard]]
        bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const override;

        [[nodiscard]]
        BodyCollection GetBodies(
            WorldHandle worldHandle,
            BodyKind kind,
            bool activeOnly,
            AZ::u32 maximumBodyCount) const override;

        [[nodiscard]]
        bool GetBodyId(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyId& bodyId) const override;

        [[nodiscard]]
        ClosestShapeRaycastResult RaycastShapeClosest(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request) const override;

        [[nodiscard]]
        ShapeRaycastHitCollection RaycastShapeAll(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        ShapePointHitCollection CollideShapePoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        bool CollideShapePointAny(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition) const override;

        [[nodiscard]]
        ShapeTriangleCollection CollectShapeTriangles(
            WorldHandle worldHandle,
            const ShapeTriangleCollectionRequest& request,
            AZ::u32 maximumTriangleCount) const override;

        [[nodiscard]]
        ClosestRaycastResult RaycastTransformedShapeClosest(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request) const override;

        [[nodiscard]]
        RaycastHitCollection RaycastTransformedShapeAll(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        OverlapHitCollection CollideTransformedShapePoint(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        bool CollideTransformedShapePointAny(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position) const override;

        [[nodiscard]]
        TransformedShapeCollection CollectTransformedShapeChildren(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZ::u32 maximumShapeCount) const override;

        [[nodiscard]]
        TransformedTriangleCollection CollectTransformedShapeTriangles(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZ::u32 maximumTriangleCount) const override;

        [[nodiscard]]
        SurfaceNormalResult GetTransformedShapeSurfaceNormal(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position) const override;

        [[nodiscard]]
        SupportingFaceVertexCollection GetTransformedShapeSupportingFace(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZ::u32 maximumVertexCount) const override;

        [[nodiscard]]
        ClosestRaycastResult RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request) const override;

        [[nodiscard]]
        ClosestRaycastResultCollection RaycastClosestBatch(
            WorldHandle worldHandle,
            const RaycastRequestCollection& requests) const override;

        [[nodiscard]]
        RaycastHitCollection RaycastClosestPerBody(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        bool RaycastAny(
            WorldHandle worldHandle,
            const RaycastRequest& request) const override;

        [[nodiscard]]
        RaycastHitCollection RaycastAll(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        OverlapHitCollection OverlapPoint(
            WorldHandle worldHandle,
            const PointOverlapRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        bool OverlapPointAny(
            WorldHandle worldHandle,
            const PointOverlapRequest& request) const override;

        [[nodiscard]]
        ShapeOverlapHitCollection CollideShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        OverlapHitCollection OverlapShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        bool OverlapShapeAny(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request) const override;

        [[nodiscard]]
        ClosestShapeCastResult CastShapeClosest(
            WorldHandle worldHandle,
            const ShapeCastRequest& request) const override;

        [[nodiscard]]
        ShapeCastHitCollection CastShapeClosestPerBody(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        ShapeCastHitCollection CastShapeAll(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        BroadPhaseHitCollection OverlapBroadPhase(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        bool OverlapBroadPhaseAny(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request) const override;

        [[nodiscard]]
        ClosestBroadPhaseCastResult CastBroadPhaseClosest(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request) const override;

        [[nodiscard]]
        BroadPhaseCastHitCollection CastBroadPhaseAll(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            AZ::u32 maximumHitCount) const override;

        [[nodiscard]]
        TransformedShapeCollection CollectShapesInBounds(
            WorldHandle worldHandle,
            const ShapeCollectionRequest& request,
            AZ::u32 maximumShapeCount) const override;

        [[nodiscard]]
        SupportingFaceVertexCollection GetSupportingFace(
            WorldHandle worldHandle,
            const SupportingFaceRequest& request,
            AZ::u32 maximumVertexCount) const override;

        [[nodiscard]]
        TransformedTriangleCollection CollectTriangles(
            WorldHandle worldHandle,
            const TriangleCollectionRequest& request,
            AZ::u32 maximumTriangleCount) const override;

        [[nodiscard]]
        BroadPhaseBoundsResult GetBroadPhaseBounds(WorldHandle worldHandle) const override;

        bool OptimizeBroadPhase(WorldHandle worldHandle) override;

        [[nodiscard]]
        bool WereBodiesInContact(
            WorldHandle worldHandle,
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const override;

        SystemConfiguration m_configuration;
        AZStd::unique_ptr<SceneAssetHandler> m_sceneAssetHandler;
        AZStd::unique_ptr<SkeletonAssetHandler> m_skeletonAssetHandler;
        AZStd::unique_ptr<System> m_system;
        Runtime* m_runtime = nullptr;
    };
} // namespace Jolt
