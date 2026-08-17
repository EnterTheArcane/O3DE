/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/NativeRuntime.h>
#include <Jolt/MaterialInternal.h>
#include <Jolt/System.h>

#include <AzCore/Jobs/JobContext.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/parallel/shared_mutex.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/containers/span.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/PathConstraintPath.h>
#include <Jolt/Physics/Hair/HairSettings.h>
#include <Jolt/Physics/Hair/HairShaders.h>
#include <Jolt/Compute/ComputeSystem.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Core/Array.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Skeleton/SkeletalAnimation.h>
#include <Jolt/Skeleton/SkeletonMapper.h>
#include <Jolt/Skeleton/SkeletonPose.h>

namespace Jolt
{
    enum class SystemRegistration : AZ::u8
    {
        Global,
        Isolated,
    };

    class ComponentDependencyManager;
    class DebugRenderer;
    class World;

    class System final
        : public ISystem
        , public ICooking
    {
    public:
        System(
            SystemConfiguration configuration,
            AZ::JobContext* jobContext,
            SystemRegistration registration = SystemRegistration::Global);
        ~System() override;

        AZ_DISABLE_COPY_MOVE(System);

        constexpr explicit operator bool() const noexcept
        {
            return m_initialized;
        }

        [[nodiscard]]
        const SystemConfiguration& GetConfiguration() const override;

        [[nodiscard]]
        RuntimeInfo GetRuntimeInfo() const override;

        [[nodiscard]]
        bool RegisterCustomConstraintProvider(ICustomConstraintProvider* provider) override;

        bool UnregisterCustomConstraintProvider(ICustomConstraintProvider* provider) override;

        [[nodiscard]]
        bool RegisterCustomPathProvider(ICustomPathProvider* provider) override;

        bool UnregisterCustomPathProvider(ICustomPathProvider* provider) override;

        [[nodiscard]]
        bool RegisterCustomConvexShapeProvider(ICustomConvexShapeProvider* provider) override;

        bool UnregisterCustomConvexShapeProvider(ICustomConvexShapeProvider* provider) override;

        [[nodiscard]]
        ProviderRegistrationResult RegisterCustomShapeProvider(ICustomShapeProvider* provider) override;

        [[nodiscard]]
        ProviderRegistrationResult UnregisterCustomShapeProvider(ICustomShapeProvider* provider) override;

        [[nodiscard]]
        MaterialHandle CreateMaterial(const MaterialConfiguration& configuration) override;

        bool DestroyMaterial(MaterialHandle materialHandle) override;

        [[nodiscard]]
        bool IsValid(MaterialHandle materialHandle) const override;

        [[nodiscard]]
        CookedShapeHandle CookShape(const ShapeConfiguration& configuration) override;

        [[nodiscard]]
        CookedShapeHandle CookShape(const CookedCompoundShapeConfiguration& configuration) override;

        [[nodiscard]]
        CookedShapeHandle CookShape(const CookedDecoratedShapeConfiguration& configuration) override;

        [[nodiscard]]
        bool ExportShape(
            CookedShapeHandle cookedShapeHandle,
            CookedShapeArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles,
            AZStd::vector<CookedShapeHandle>& childShapeHandles) const override;

        [[nodiscard]]
        CookedShapeHandle ImportShape(
            const CookedShapeArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles,
            AZStd::span<const CookedShapeHandle> childShapeHandles) override;

        bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle) override;

        [[nodiscard]]
        bool IsValid(CookedShapeHandle cookedShapeHandle) const override;

        [[nodiscard]]
        bool GetStats(
            CookedShapeHandle cookedShapeHandle,
            ShapeStats& stats) const override;

        [[nodiscard]]
        bool GetStatsRecursive(
            CookedShapeHandle cookedShapeHandle,
            ShapeStats& stats) const override;

        [[nodiscard]]
        bool GetProperties(
            CookedShapeHandle cookedShapeHandle,
            ShapeProperties& properties) const override;

        [[nodiscard]]
        bool GetUserData(
            CookedShapeHandle cookedShapeHandle,
            AZ::u64& userData) const override;

        [[nodiscard]]
        bool GetCustomConvexShapeInfo(
            CookedShapeHandle cookedShapeHandle,
            CustomConvexShapeInfo& info) const override;

        [[nodiscard]]
        bool GetCustomShapeInfo(
            CookedShapeHandle cookedShapeHandle,
            CustomShapeInfo& info) const override;

        [[nodiscard]]
        BufferResult GetCustomShapeDependencies(
            CookedShapeHandle cookedShapeHandle,
            AZStd::span<CustomShapeDependency> dependencies) const override;

        [[nodiscard]]
        bool GetSubShapeUserData(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const override;

        [[nodiscard]]
        bool GetDirectChildShape(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            CookedShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const override;

        [[nodiscard]]
        BufferResult GetMeshMaterials(
            CookedShapeHandle cookedShapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const override;

        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const override;

        [[nodiscard]]
        bool GetMeshTriangleUserData(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const override;

        [[nodiscard]]
        bool GetCompoundChildCount(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32& childCount) const override;

        [[nodiscard]]
        bool GetCompoundChild(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32 childIndex,
            CookedCompoundChildConfiguration& child) const override;

        [[nodiscard]]
        bool GetCompoundChildIndex(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const override;

        [[nodiscard]]
        bool Raycast(
            CookedShapeHandle cookedShapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            CookedRaycastHit& hit) const override;

        [[nodiscard]]
        GroupFilterHandle CreateGroupFilter(
            AZ::u32 subGroupCount,
            IGroupFilter* filter) override;

        [[nodiscard]]
        GroupFilterHandle CreateGroupFilterTable(
            const GroupFilterTableConfiguration& configuration) override;

        bool DestroyGroupFilter(GroupFilterHandle filterHandle) override;

        [[nodiscard]]
        bool IsValid(GroupFilterHandle filterHandle) const override;

        bool NotifyGroupFilterChanged(GroupFilterHandle filterHandle) override;

        [[nodiscard]]
        bool GetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool& enabled) const override;

        bool SetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool enabled) override;

        [[nodiscard]]
        PathHandle CreatePath(const HermitePathConfiguration& configuration) override;

        [[nodiscard]]
        PathHandle CreatePath(const CustomPathConfiguration& configuration) override;

        bool DestroyPath(PathHandle pathHandle) override;

        [[nodiscard]]
        bool IsValid(PathHandle pathHandle) const override;

        [[nodiscard]]
        bool GetPathState(
            PathHandle pathHandle,
            PathState& state) const override;

        [[nodiscard]]
        bool GetCustomPathInfo(
            PathHandle pathHandle,
            CustomPathInfo& info) const override;

        bool SamplePath(
            PathHandle pathHandle,
            float fraction,
            PathSample& sample) const override;

        bool FindClosestPathPoint(
            PathHandle pathHandle,
            const AZ::Vector3& position,
            float fractionHint,
            PathSample& sample) const override;

        [[nodiscard]]
        SkeletonDefinitionHandle CreateSkeletonDefinition(
            const SkeletonDefinitionConfiguration& configuration) override;

        [[nodiscard]]
        bool ExportSkeletonDefinition(
            SkeletonDefinitionHandle skeletonHandle,
            SkeletonDefinitionArchive& archive) const override;

        [[nodiscard]]
        SkeletonDefinitionHandle ImportSkeletonDefinition(
            const SkeletonDefinitionArchive& archive) override;

        bool DestroySkeletonDefinition(SkeletonDefinitionHandle skeletonHandle) override;

        [[nodiscard]]
        bool IsValid(SkeletonDefinitionHandle skeletonHandle) const override;

        [[nodiscard]]
        QueryResult GetSkeletonJoints(
            SkeletonDefinitionHandle skeletonHandle,
            AZStd::span<SkeletonJoint> joints) const override;

        [[nodiscard]]
        bool FindSkeletonJoint(
            SkeletonDefinitionHandle skeletonHandle,
            AZ::Name jointName,
            AZ::u32& jointIndex) const override;

        [[nodiscard]]
        SkeletalAnimationHandle CreateSkeletalAnimation(
            const SkeletalAnimationConfiguration& configuration) override;

        [[nodiscard]]
        bool ExportSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationArchive& archive) const override;

        [[nodiscard]]
        SkeletalAnimationHandle ImportSkeletalAnimation(
            const SkeletalAnimationArchive& archive) override;

        bool UpdateSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            const SkeletalAnimationConfiguration& configuration) override;

        bool DestroySkeletalAnimation(SkeletalAnimationHandle animationHandle) override;

        [[nodiscard]]
        bool IsValid(SkeletalAnimationHandle animationHandle) const override;

        [[nodiscard]]
        bool GetSkeletalAnimationState(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationState& state) const override;

        [[nodiscard]]
        bool GetSkeletalAnimatedJointName(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex,
            AZ::Name& jointName) const override;

        [[nodiscard]]
        QueryResult GetSkeletalAnimationKeyframes(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex,
            AZStd::span<SkeletalAnimationKeyframe> keyframes) const override;

        bool SetSkeletalAnimationLooping(
            SkeletalAnimationHandle animationHandle,
            bool isLooping) override;

        bool ScaleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            float scale) override;

        [[nodiscard]]
        SkeletonPoseHandle CreateSkeletonPose(SkeletonDefinitionHandle skeletonHandle) override;

        bool DestroySkeletonPose(SkeletonPoseHandle poseHandle) override;

        [[nodiscard]]
        bool IsValid(SkeletonPoseHandle poseHandle) const override;

        [[nodiscard]]
        bool GetSkeletonPoseState(
            SkeletonPoseHandle poseHandle,
            SkeletonPoseState& state) const override;

        bool SetSkeletonPoseRootOffset(
            SkeletonPoseHandle poseHandle,
            const WorldPosition& rootOffset) override;

        bool SetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<const AZ::Transform> localTransforms) override;

        bool SetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<const AZ::Transform> modelTransforms) override;

        [[nodiscard]]
        QueryResult GetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<AZ::Transform> localTransforms) const override;

        [[nodiscard]]
        QueryResult GetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<AZ::Transform> modelTransforms) const override;

        bool SampleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletonPoseHandle poseHandle,
            float time) override;

        [[nodiscard]]
        SkeletonMapperHandle CreateSkeletonMapper(
            const SkeletonMapperConfiguration& configuration) override;

        bool DestroySkeletonMapper(SkeletonMapperHandle mapperHandle) override;

        [[nodiscard]]
        bool IsValid(SkeletonMapperHandle mapperHandle) const override;

        [[nodiscard]]
        bool GetSkeletonMapperState(
            SkeletonMapperHandle mapperHandle,
            SkeletonMapperState& state) const override;

        [[nodiscard]]
        QueryResult GetSkeletonMapperMappings(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperMappingState> mappings) const override;

        [[nodiscard]]
        bool GetSkeletonMapperChainState(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            SkeletonMapperChainState& state) const override;

        [[nodiscard]]
        QueryResult GetSkeletonMapperSourceChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            AZStd::span<AZ::u32> jointIndices) const override;

        [[nodiscard]]
        QueryResult GetSkeletonMapperTargetChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            AZStd::span<AZ::u32> jointIndices) const override;

        [[nodiscard]]
        QueryResult GetSkeletonMapperUnmappedJoints(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperUnmappedJoint> joints) const override;

        [[nodiscard]]
        QueryResult GetSkeletonMapperLockedTranslations(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperLockedTranslation> translations) const override;

        [[nodiscard]]
        bool GetMappedSkeletonJoint(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 sourceJointIndex,
            AZ::u32& targetJointIndex) const override;

        [[nodiscard]]
        bool IsSkeletonJointTranslationLocked(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 targetJointIndex,
            bool& locked) const override;

        [[nodiscard]]
        bool MapSkeletonPose(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<const AZ::Transform> sourceModelTransforms,
            AZStd::span<const AZ::Transform> targetLocalTransforms,
            AZStd::span<AZ::Transform> targetModelTransforms) const override;

        [[nodiscard]]
        bool MapSkeletonPoseReverse(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<const AZ::Transform> targetModelTransforms,
            AZStd::span<AZ::Transform> sourceModelTransforms) const override;

        [[nodiscard]]
        SoftBodyDefinitionHandle CreateSoftBodyDefinition(
            const SoftBodyDefinitionConfiguration& configuration,
            SoftBodyOptimizationRemap* optimizationRemap = nullptr) override;

        [[nodiscard]]
        bool ExportSoftBodyDefinition(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles) const override;

        [[nodiscard]]
        SoftBodyDefinitionHandle ImportSoftBodyDefinition(
            const SoftBodyDefinitionArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles) override;

        bool DestroySoftBodyDefinition(SoftBodyDefinitionHandle definitionHandle) override;

        [[nodiscard]]
        bool IsValid(SoftBodyDefinitionHandle definitionHandle) const override;

        [[nodiscard]]
        bool GetSoftBodyDefinitionState(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionState& state) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionDihedralBendConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyDihedralBendConstraint> constraints) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionEdgeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyEdgeConstraint> constraints) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionFaces(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyFace> faces) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionInverseBinds(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyInverseBind> inverseBinds) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionLongRangeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyLongRangeConstraint> constraints) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionMaterials(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<MaterialHandle> materials) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionRodBendTwistConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionRodStretchShearConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionSkinConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodySkinConstraint> constraints) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionVertices(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVertex> vertices) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionVolumeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVolumeConstraint> constraints) const override;

        [[nodiscard]]
        HairDefinitionHandle CreateHairDefinition(
            const HairDefinitionConfiguration& configuration) override;

        bool DestroyHairDefinition(HairDefinitionHandle definitionHandle) override;

        [[nodiscard]]
        bool IsValid(HairDefinitionHandle definitionHandle) const override;

        [[nodiscard]]
        bool GetHairDefinitionState(
            HairDefinitionHandle definitionHandle,
            HairDefinitionState& state) const override;

        [[nodiscard]]
        QueryResult GetHairNeutralDensity(
            HairDefinitionHandle definitionHandle,
            AZStd::span<float> density) const override;

        bool SkinHairScalpVertices(
            HairDefinitionHandle definitionHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms,
            AZStd::span<AZ::Transform> preparedJointTransforms,
            AZStd::span<AZ::Vector3> scalpVertices) const override;

        [[nodiscard]]
        SceneDefinitionHandle CreateSceneDefinition(const SceneConfiguration& configuration) override;

        [[nodiscard]]
        SceneDefinitionHandle CreateSceneDefinition(const SceneAssetData& assetData) override;

        [[nodiscard]]
        bool BuildSceneAsset(
            const SceneSourceData& sourceData,
            SceneAssetData& assetData) override;

        bool DestroySceneDefinition(SceneDefinitionHandle definitionHandle) override;

        [[nodiscard]]
        bool IsValid(SceneDefinitionHandle definitionHandle) const override;

        [[nodiscard]]
        bool GetSceneDefinitionState(
            SceneDefinitionHandle definitionHandle,
            SceneDefinitionState& state) const override;

        [[nodiscard]]
        SceneInstanceHandle InstantiateScene(
            WorldHandle worldHandle,
            SceneDefinitionHandle definitionHandle) override;

        bool DestroySceneInstance(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle) const override;

        [[nodiscard]]
        bool GetSceneInstanceState(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            SceneInstanceState& state) const override;

        [[nodiscard]]
        QueryResult GetSceneBodies(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<BodyHandle> bodyHandles) const override;

        [[nodiscard]]
        QueryResult GetSceneConstraints(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const override;

        [[nodiscard]]
        WorldHandle CreateWorld(const WorldConfiguration& configuration) override;

        bool DestroyWorld(WorldHandle worldHandle) override;

        [[nodiscard]]
        WorldHandle GetDefaultWorldHandle() const override;

        [[nodiscard]]
        const IWorldQueries* GetWorldQueries(WorldHandle worldHandle) const override;

        [[nodiscard]]
        bool IsValid(WorldHandle worldHandle) const override;

        [[nodiscard]]
        bool GetWorldGravity(
            WorldHandle worldHandle,
            AZ::Vector3& gravity) const override;

        bool SetWorldGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity) override;

        [[nodiscard]]
        bool GetSimulationConfiguration(
            WorldHandle worldHandle,
            SimulationConfiguration& configuration) const override;

        bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration) override;

        [[nodiscard]]
        bool GetWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            WorldRuntimeConfiguration& configuration) const override;

        bool UpdateWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration) override;

        bool StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep) override;

        [[nodiscard]]
        SimulationResult StepWorldDetailed(
            WorldHandle worldHandle,
            float fixedTimeStep) override;

        bool StepAutoSimulatedWorlds(float elapsedTime) override;

        [[nodiscard]]
        SimulationResult StepAutoSimulatedWorldsDetailed(float elapsedTime) override;

        [[nodiscard]]
        EventView GetEvents(WorldHandle worldHandle) const override;

        bool SetContactCallbacks(
            WorldHandle worldHandle,
            IContactCallbacks* callbacks) override;

        bool SetBodyPairCollider(
            WorldHandle worldHandle,
            IBodyPairCollider* collider) override;

        bool SetSimulationShapeFilter(
            WorldHandle worldHandle,
            ISimulationShapeFilter* filter) override;

        bool SetSoftBodyContactCallbacks(
            WorldHandle worldHandle,
            ISoftBodyContactCallbacks* callbacks) override;

        bool AddStepListener(
            WorldHandle worldHandle,
            IStepListener* listener) override;

        bool RemoveStepListener(
            WorldHandle worldHandle,
            IStepListener* listener) override;

        [[nodiscard]]
        HairHandle CreateHair(
            WorldHandle worldHandle,
            const HairConfiguration& configuration) override;

        bool DestroyHair(
            WorldHandle worldHandle,
            HairHandle hairHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            HairHandle hairHandle) const override;

        bool SetHairTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const WorldTransform& worldTransform,
            bool teleport) override;

        bool SetHairScalpToHeadTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& scalpToHeadTransform) override;

        bool UpdateHair(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) override;

        bool EnableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) override;

        bool DisableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle) override;

        [[nodiscard]]
        bool GetHairState(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            HairState& state) const override;

        [[nodiscard]]
        bool GetHairReadback(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const override;

        [[nodiscard]]
        QueryResult GetHairVertexStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairVertexState> states) const override;

        [[nodiscard]]
        QueryResult GetHairRenderPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const override;

        [[nodiscard]]
        QueryResult GetHairScalpPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const override;

        [[nodiscard]]
        QueryResult GetHairGridCellStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairGridCellState> states) const override;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const ShapeConfiguration& configuration) override;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const CompoundShapeConfiguration& configuration) override;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const DecoratedShapeConfiguration& configuration) override;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            CookedShapeHandle cookedShapeHandle) override;

        [[nodiscard]]
        ShapeHandle CloneShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) override;

        [[nodiscard]]
        ShapeHandle ScaleShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) override;

        bool DestroyShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) const override;

        [[nodiscard]]
        bool GetShapeStats(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const override;

        [[nodiscard]]
        bool GetShapeStatsRecursive(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const override;

        [[nodiscard]]
        bool GetShapeProperties(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeProperties& properties) const override;

        [[nodiscard]]
        bool GetShapeSubmergedVolume(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const override;

        [[nodiscard]]
        bool GetPrimitiveShapeState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            PrimitiveShapeState& state) const override;

        [[nodiscard]]
        bool GetConvexHullState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ConvexHullState& state) const override;

        [[nodiscard]]
        BufferResult GetConvexHullPointsRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Vector3> points) const override;

        [[nodiscard]]
        BufferResult GetConvexHullPlanesRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Plane> planes) const override;

        [[nodiscard]]
        BufferResult GetConvexHullFaceVertexIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 faceIndex,
            AZStd::span<AZ::u32> vertexIndices) const override;

        [[nodiscard]]
        bool GetShapeMaterial(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const override;

        [[nodiscard]]
        bool GetShapeSurfaceNormal(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const override;

        [[nodiscard]]
        bool GetShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u64& userData) const override;

        [[nodiscard]]
        bool GetShapeSubShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const override;

        [[nodiscard]]
        bool GetDirectChildShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const override;

        [[nodiscard]]
        bool GetDecoratedShapeConfiguration(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            DecoratedShapeConfiguration& configuration) const override;

        [[nodiscard]]
        BufferResult GetMeshMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const override;

        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const override;

        [[nodiscard]]
        bool GetMeshTriangleUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const override;

        [[nodiscard]]
        bool IsShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) const override;

        [[nodiscard]]
        bool MakeShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const override;

        [[nodiscard]]
        bool GetHeightfieldState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            HeightfieldState& state) const override;

        [[nodiscard]]
        bool GetHeightfieldPosition(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const override;

        [[nodiscard]]
        bool ProjectOntoHeightfield(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::Vector3& surfacePosition,
            SubShapeId& subShapeId) const override;

        [[nodiscard]]
        bool IsHeightfieldNoCollision(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const override;

        [[nodiscard]]
        QueryResult GetHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<float> heights) const override;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterialIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<AZ::u8> materialIndices) const override;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const override;

        [[nodiscard]]
        bool GetHeightfieldSubShapeCoordinates(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const override;

        bool UpdateHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const float> heights,
            const HeightfieldUpdateConfiguration& configuration = {}) override;

        bool UpdateHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const AZ::u8> materialIndices,
            AZStd::span<const MaterialHandle> materialHandles,
            bool activateBodies = true) override;

        bool AddMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            const CompoundChildConfiguration& child,
            AZ::u32 insertionIndex,
            AZ::u32& childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) override;

        bool RemoveMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) override;

        bool UpdateMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const CompoundChildConfiguration& child,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) override;

        bool UpdateMutableCompoundChildTransforms(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 startIndex,
            AZStd::span<const AZ::Vector3> positions,
            AZStd::span<const AZ::Quaternion> rotations,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) override;

        bool AdjustMutableCompoundCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            bool updateMassProperties,
            bool activateBodies) override;

        [[nodiscard]]
        bool GetCompoundChildCount(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32& childCount) const override;

        [[nodiscard]]
        bool GetCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const override;

        [[nodiscard]]
        bool GetCompoundChildIndex(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const override;

        [[nodiscard]]
        BodyHandle CreateBody(
            WorldHandle worldHandle,
            const BodyConfiguration& configuration) override;

        [[nodiscard]]
        BodyHandle CreateBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const BodyConfiguration& configuration) override;

        [[nodiscard]]
        BodyHandle CreateSoftBody(
            WorldHandle worldHandle,
            const SoftBodyConfiguration& configuration) override;

        [[nodiscard]]
        BodyHandle CreateSoftBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const SoftBodyConfiguration& configuration) override;

        bool AddBodyToSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate) override;

        bool AddBodiesToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles,
            bool activate) override;

        bool RemoveBodyFromSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool RemoveBodiesFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) override;

        bool DestroyBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool DestroyBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) override;

        [[nodiscard]]
        bool IsBodyInSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const override;

        bool SetBodyMoveEventsEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        [[nodiscard]]
        RagdollDefinitionHandle CreateRagdollDefinition(
            WorldHandle worldHandle,
            const RagdollDefinitionConfiguration& configuration) override;

        bool DestroyRagdollDefinition(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle) const override;

        [[nodiscard]]
        QueryResult GetRagdollBodyConstraintIndices(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<AZ::s32> constraintIndices) const override;

        [[nodiscard]]
        QueryResult GetRagdollConstraintBodyPairs(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<RagdollConstraintBodyPair> bodyPairs) const override;

        [[nodiscard]]
        RagdollHandle CreateRagdoll(
            WorldHandle worldHandle,
            const RagdollConfiguration& configuration) override;

        bool AddRagdollToSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            bool activate) override;

        bool RemoveRagdollFromSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) override;

        bool DestroyRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const override;

        [[nodiscard]]
        bool IsRagdollInSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const override;

        [[nodiscard]]
        bool GetRagdollState(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            RagdollState& state) const override;

        bool SetRagdollCollisionGroupId(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::u32 collisionGroupId) override;

        [[nodiscard]]
        QueryResult GetRagdollBodies(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<BodyHandle> bodyHandles) const override;

        [[nodiscard]]
        QueryResult GetRagdollConstraints(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const override;

        bool ActivateRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) override;

        bool SetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms) override;

        [[nodiscard]]
        QueryResult GetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const override;

        bool DriveRagdollKinematically(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) override;

        bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> modelTransforms) override;

        bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) override;

        bool ResetRagdollWarmStart(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) override;

        bool SetRagdollVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity,
            AZ::Vector3 angularVelocity) override;

        bool SetRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity) override;

        bool AddRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity) override;

        bool AddRagdollImpulse(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 impulse) override;

        [[nodiscard]]
        ConstraintHandle CreateConstraint(
            WorldHandle worldHandle,
            const ConstraintConfiguration& configuration) override;

        bool AddConstraintToSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        bool AddConstraintsToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles) override;

        bool RemoveConstraintFromSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        bool RemoveConstraintsFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles) override;

        bool DestroyConstraint(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        bool DestroyConstraints(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles) override;

        [[nodiscard]]
        bool IsConstraintInSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const override;

        bool SetConstraintEnabled(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            bool enabled) override;

        [[nodiscard]]
        bool GetConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintState& state) const override;

        [[nodiscard]]
        bool GetConstraintConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64& userData) const override;

        bool SetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64 userData) override;

        [[nodiscard]]
        bool GetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float& debugDrawSize) const override;

        bool SetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float debugDrawSize) override;

        [[nodiscard]]
        bool GetConstraintMeasurements(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintMeasurements& measurements) const override;

        [[nodiscard]]
        bool GetCustomConstraintInfo(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            CustomConstraintInfo& info) const override;

        [[nodiscard]]
        BufferResult GetCustomConstraintImpulses(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<float> impulses) const override;

        [[nodiscard]]
        BufferResult GetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<AZ::u8> state) const override;

        bool SetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const AZ::u8> state) override;

        bool ResetConstraintWarmStart(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        bool UpdateConstraintSolverConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const ConstraintSolverConfiguration& configuration) override;

        bool UpdateConeLimit(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float halfConeAngle) override;

        bool UpdateDistanceLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumDistance,
            float maximumDistance,
            const SpringConfiguration& spring) override;

        bool UpdateHingeLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumAngle,
            float maximumAngle,
            const SpringConfiguration& spring,
            float maximumFrictionTorque) override;

        bool UpdateHingeMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetAngle,
            float targetAngularVelocity) override;

        bool SetHingeTargetOrientation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const AZ::Quaternion& targetOrientation) override;

        bool UpdatePathMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPathFraction,
            float targetVelocity) override;

        bool UpdatePathProperties(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            PathHandle pathHandle,
            float pathFraction,
            float maximumFrictionForce) override;

        bool UpdatePointAnchors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintSpace space,
            const WorldPosition& firstPoint,
            const WorldPosition& secondPoint) override;

        bool UpdatePulleyLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumLength,
            float maximumLength) override;

        bool UpdateSixDofLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const SixDofAxisLimitConfiguration> axes) override;

        bool UpdateSixDofMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const MotorConfiguration> motors,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation,
            const AZ::Vector3& targetPosition,
            const AZ::Vector3& targetVelocity) override;

        bool UpdateSliderMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPosition,
            float targetVelocity) override;

        bool UpdateSliderLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumPosition,
            float maximumPosition,
            const SpringConfiguration& spring,
            float maximumFrictionForce) override;

        bool UpdateSwingTwistMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& swingMotor,
            const MotorConfiguration& twistMotor,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation) override;

        bool UpdateSwingTwistLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float normalHalfConeAngle,
            float planeHalfConeAngle,
            float twistMinimumAngle,
            float twistMaximumAngle,
            float maximumFrictionTorque) override;

        [[nodiscard]]
        bool GetBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyState& state) const override;

        [[nodiscard]]
        bool GetBodyCenterOfMassTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldTransform& transform) const override;

        [[nodiscard]]
        bool GetBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64& userData) const override;

        bool SetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64 userData) override;

        [[nodiscard]]
        bool GetBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyRuntimeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetBodySimulationStatistics(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodySimulationStatistics& statistics) const override;

        bool ApplyBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyConfiguration& configuration) override;

        [[nodiscard]]
        QueryResult GetSoftBodyFaces(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyFace> faces) const override;

        [[nodiscard]]
        bool GetSoftBodyLocalBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Aabb& bounds) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyMaterials(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<MaterialHandle> materials) const override;

        [[nodiscard]]
        QueryResult GetSoftBodyRodStates(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyRodState> rods) const override;

        [[nodiscard]]
        bool GetSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SoftBodyRuntimeConfiguration& configuration) const override;

        bool ApplySoftBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyConfiguration& configuration) override;

        [[nodiscard]]
        QueryResult GetSoftBodyVertices(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyVertex> vertices) const override;

        [[nodiscard]]
        bool GetSoftBodyVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& volume) const override;

        bool RecalculateSoftBodyMassProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate) override;

        bool SkinSoftBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll) override;

        bool UpdateSoftBodyManually(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float deltaTime) override;

        bool UpdateSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyRuntimeConfiguration& configuration) override;

        bool SetSoftBodyVertexInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            float inverseMass) override;

        bool SetSoftBodyVertexInverseMasses(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses) override;

        bool SetSoftBodyVertexVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity) override;

        bool SetSoftBodyVertexVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities) override;

        [[nodiscard]]
        VirtualCharacterHandle CreateVirtualCharacter(
            WorldHandle worldHandle,
            const VirtualCharacterConfiguration& configuration) override;

        bool DestroyVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) const override;

        [[nodiscard]]
        bool GetVirtualCharacterState(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterState& state) const override;

        [[nodiscard]]
        bool GetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64& userData) const override;

        bool SetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64 userData) override;

        [[nodiscard]]
        bool GetVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterRuntimeConfiguration& configuration) const override;

        [[nodiscard]]
        QueryResult CheckVirtualCharacterCollision(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter) const override;

        bool UpdateVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterRuntimeConfiguration& configuration) override;

        bool SetVirtualCharacterShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth) override;

        bool SetVirtualCharacterInnerBodyShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle) override;

        bool SetVirtualCharacterTransform(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const WorldTransform& transform) override;

        bool SetVirtualCharacterVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& velocity) override;

        [[nodiscard]]
        bool CancelVirtualCharacterVelocityTowardsSteepSlopes(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity,
            AZ::Vector3& adjustedVelocity) const override;

        bool BeginVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) override;

        bool EndVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) override;

        bool SetVirtualCharacterContactCallbacks(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            IVirtualCharacterContactCallbacks* callbacks) override;

        [[nodiscard]]
        bool CanVirtualCharacterWalkStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity) const override;

        bool WalkVirtualCharacterStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter = nullptr) override;

        bool StickVirtualCharacterToFloor(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter = nullptr) override;

        bool RefreshVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const IQueryFilter* filter = nullptr) override;

        bool UpdateVirtualCharacterGroundVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) override;

        [[nodiscard]]
        QueryResult GetVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZStd::span<VirtualCharacterContact> contacts) const override;

        [[nodiscard]]
        bool HasVirtualCharacterCollidedWith(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle) const override;

        [[nodiscard]]
        bool HaveVirtualCharactersCollided(
            WorldHandle worldHandle,
            VirtualCharacterHandle firstCharacterHandle,
            VirtualCharacterHandle secondCharacterHandle) const override;

        bool UpdateVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration) override;

        bool EnableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterUpdateConfiguration& configuration) override;

        bool DisableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) override;

        [[nodiscard]]
        CharacterHandle CreateCharacter(
            WorldHandle worldHandle,
            const CharacterConfiguration& configuration) override;

        bool DestroyCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) const override;

        [[nodiscard]]
        bool GetCharacterState(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterState& state) const override;

        [[nodiscard]]
        bool GetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64& userData) const override;

        bool SetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64 userData) override;

        [[nodiscard]]
        bool GetCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterRuntimeConfiguration& configuration) const override;

        QueryResult CheckCharacterCollision(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter) const override;

        bool UpdateCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterRuntimeConfiguration& configuration) override;

        bool SetCharacterShape(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth) override;

        bool SetCharacterTransform(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const WorldTransform& transform,
            bool activate) override;

        bool SetCharacterVelocity(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity) override;

        bool AddCharacterImpulse(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& impulse) override;

        bool ApplyVehicleEngineDamping(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float deltaTime) override;

        bool ApplyVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float torque,
            float deltaTime) override;

        [[nodiscard]]
        bool CalculateVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float acceleration,
            float& torque) const override;

        [[nodiscard]]
        VehicleHandle CreateWheeledVehicle(
            WorldHandle worldHandle,
            const WheeledVehicleConfiguration& configuration) override;

        [[nodiscard]]
        VehicleHandle CreateMotorcycle(
            WorldHandle worldHandle,
            const MotorcycleConfiguration& configuration) override;

        [[nodiscard]]
        VehicleHandle CreateTrackedVehicle(
            WorldHandle worldHandle,
            const TrackedVehicleConfiguration& configuration) override;

        bool DestroyVehicle(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle) const override;

        [[nodiscard]]
        QueryResult GetWheeledVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const override;

        [[nodiscard]]
        QueryResult GetMotorcycleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const override;

        [[nodiscard]]
        QueryResult GetTrackedVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const override;

        [[nodiscard]]
        bool GetVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleCollisionConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float& ratio) const override;

        [[nodiscard]]
        bool GetVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleEngineConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetVehiclePowertrainState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehiclePowertrainState& state) const override;

        [[nodiscard]]
        bool GetVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleRuntimeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleTransmissionConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            VehicleTrackConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetWheelLocalBasis(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            WheelBasis& basis) const override;

        [[nodiscard]]
        bool GetWheelLocalTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            AZ::Transform& transform) const override;

        [[nodiscard]]
        bool GetWheelWorldTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            WorldTransform& transform) const override;

        [[nodiscard]]
        QueryResult QueryVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const override;

        [[nodiscard]]
        QueryResult QueryVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleDifferentialConfiguration> differentials) const override;

        bool SetTrackedVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const TrackedVehicleInput& input) override;

        bool SetVehicleCallbacks(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            IVehicleCallbacks* callbacks) override;

        bool SetVehicleCollisionFilter(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const IVehicleCollisionFilter* filter) override;

        bool SetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float ratio) override;

        bool SetVehiclePowertrainControl(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehiclePowertrainControl& control) override;

        bool SetVehicleTrackAngularVelocity(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            float angularVelocity) override;

        bool SetWheelMotion(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const WheelMotion& motion) override;

        bool SetWheeledVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const WheeledVehicleInput& input) override;

        bool UpdateMotorcycleController(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const MotorcycleControllerUpdateConfiguration& configuration) override;

        bool UpdateVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars) override;

        bool UpdateVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleCollisionConfiguration& configuration) override;

        bool UpdateVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleDifferentialConfiguration> differentials) override;

        bool UpdateVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleEngineConfiguration& configuration) override;

        bool UpdateVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleRuntimeConfiguration& configuration) override;

        bool UpdateVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleTransmissionConfiguration& configuration) override;

        bool UpdateVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration) override;

        [[nodiscard]]
        BodySnapshotHandle CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodySnapshotHandle snapshotHandle) override;

        bool DestroyBodyStateSnapshot(
            WorldHandle worldHandle,
            BodySnapshotHandle snapshotHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            BodySnapshotHandle snapshotHandle) const override;

        bool RestoreBodyState(
            WorldHandle worldHandle,
            BodySnapshotHandle snapshotHandle) override;

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle) override;

        bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) override;

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles) override;

        bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles) override;

        bool CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles,
            AZStd::span<const AZ::u32> partitionBodyCounts,
            AZStd::span<StateSnapshotHandle> snapshotHandles) override;

        bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles,
            StateSnapshotArchive& archive) override;

        bool ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive,
            AZStd::span<StateSnapshotHandle> snapshotHandles) override;

        bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) override;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const override;

        bool RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) override;

        bool RestoreWorldStateParts(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles) override;

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

        [[nodiscard]]
        DiagnosticStatisticsResult GetBroadPhaseStatistics(
            WorldHandle worldHandle,
            AZStd::span<BroadPhaseStatistics> statistics,
            bool reset) override;

        [[nodiscard]]
        DiagnosticStatisticsResult GetNarrowPhaseStatistics(
            AZStd::span<NarrowPhaseStatistics> statistics,
            bool reset) override;

        bool DrawDebug(
            WorldHandle worldHandle,
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer,
            const IDebugFilter* filter = nullptr) override;

        bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration) override;

        [[nodiscard]]
        bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const override;

        QueryResult GetBodies(
            WorldHandle worldHandle,
            BodyKind kind,
            bool activeOnly,
            AZStd::span<BodyHandle> bodies) const override;

        [[nodiscard]]
        bool GetBodyId(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyId& bodyId) const override;

        bool ActivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool ActivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) override;

        bool ActivateBodiesInBounds(
            WorldHandle worldHandle,
            const BroadPhaseAabb& bounds,
            ObjectLayer collisionLayer = ObjectLayer::Invalid) override;

        bool DeactivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool DeactivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) override;

        bool ResetBodySleepTimer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool InvalidateBodyContactCache(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        bool GetBodyPointVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& point,
            AZ::Vector3& velocity) const override;

        [[nodiscard]]
        bool GetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType& motionType) const override;

        [[nodiscard]]
        bool GetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer& objectLayer) const override;

        [[nodiscard]]
        bool GetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            CollisionGroupConfiguration& collisionGroup) const override;

        [[nodiscard]]
        bool GetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle& shapeHandle) const override;

        [[nodiscard]]
        bool GetBodyAccumulatedForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& force,
            AZ::Vector3& torque) const override;

        bool ResetBodyAccumulatedForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool ResetBodyAccumulatedTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool ResetBodyMotion(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        bool GetBodyBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BroadPhaseAabb& bounds) const override;

        [[nodiscard]]
        bool GetBodySubmergedVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const override;

        [[nodiscard]]
        bool GetBodySurfaceNormal(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const override;

        [[nodiscard]]
        bool GetBodyMaterial(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const override;

        [[nodiscard]]
        bool GetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldPosition& position) const override;

        [[nodiscard]]
        bool GetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Quaternion& rotation) const override;

        [[nodiscard]]
        bool GetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const override;

        [[nodiscard]]
        bool GetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity) const override;

        [[nodiscard]]
        bool GetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& angularVelocity) const override;

        bool SetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& position,
            bool activate) override;

        bool SetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Quaternion& rotation,
            bool activate) override;

        bool SetBodyTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate) override;

        bool SetBodyTransformWhenChanged(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate) override;

        bool SetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) override;

        bool SetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity) override;

        bool SetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularVelocity) override;

        bool AddBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) override;

        bool AddBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity) override;

        bool SetBodyTransformAndVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) override;

        bool MoveBodyKinematically(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& target,
            float duration) override;

        bool AddForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool activate = true) override;

        bool AddForceAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate = true) override;

        bool AddTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool activate = true) override;

        bool AddForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate = true) override;

        bool ApplyBuoyancyImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BuoyancyConfiguration& configuration) override;

        [[nodiscard]]
        bool GetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& friction) const override;

        bool SetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float friction) override;

        [[nodiscard]]
        bool GetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& restitution) const override;

        bool SetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float restitution) override;

        [[nodiscard]]
        bool GetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& gravityFactor) const override;

        bool SetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float gravityFactor) override;

        [[nodiscard]]
        bool GetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumLinearVelocity) const override;

        bool SetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumLinearVelocity) override;

        [[nodiscard]]
        bool GetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumAngularVelocity) const override;

        bool SetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumAngularVelocity) override;

        [[nodiscard]]
        bool GetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality& motionQuality) const override;

        bool SetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality motionQuality) override;

        [[nodiscard]]
        bool IsBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const override;

        bool SetBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        [[nodiscard]]
        bool IsBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sensor) const override;

        bool SetBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sensor) override;

        [[nodiscard]]
        bool GetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& linearDamping) const override;

        bool SetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float linearDamping) override;

        [[nodiscard]]
        bool GetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& angularDamping) const override;

        bool SetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float angularDamping) override;

        [[nodiscard]]
        bool IsBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sleepingAllowed) const override;

        bool SetBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sleepingAllowed) override;

        [[nodiscard]]
        bool IsBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const override;

        bool SetBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        [[nodiscard]]
        bool IsBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const override;

        bool SetBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        [[nodiscard]]
        bool IsBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const override;

        bool SetBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        [[nodiscard]]
        bool GetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const override;

        bool SetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount) override;

        bool UpdateBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyRuntimeConfiguration& configuration,
            bool activate) override;

        [[nodiscard]]
        bool GetBodyInverseInertia(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Matrix3x3& inverseInertia) const override;

        [[nodiscard]]
        bool GetBodyInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& inverseMass) const override;

        bool AddImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse) override;

        bool AddImpulseAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const WorldPosition& position) override;

        bool AddAngularImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularImpulse) override;

        bool SetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle shapeHandle,
            bool updateMassProperties,
            bool activate) override;

        bool SetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType motionType,
            bool activate) override;

        bool SetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer objectLayer) override;

        bool SetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const CollisionGroupConfiguration& collisionGroup,
            bool activate) override;

        [[nodiscard]]
        bool RaycastShapeClosest(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            ShapeRaycastHit& hit) const override;

        [[nodiscard]]
        QueryResult RaycastShapeAll(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            AZStd::span<ShapeRaycastHit> hits) const override;

        [[nodiscard]]
        QueryResult CollideShapePoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter,
            AZStd::span<ShapePointHit> hits) const override;

        [[nodiscard]]
        bool CollideShapePointAny(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter = nullptr) const override;

        [[nodiscard]]
        QueryResult CollectShapeTriangles(
            WorldHandle worldHandle,
            const ShapeTriangleCollectionRequest& request,
            AZStd::span<ShapeTriangle> triangles) const override;

        [[nodiscard]]
        bool RaycastTransformedShapeClosest(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            RaycastHit& hit) const override;

        [[nodiscard]]
        QueryResult RaycastTransformedShapeAll(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZStd::span<RaycastHit> hits) const override;

        [[nodiscard]]
        QueryResult CollideTransformedShapePoint(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        bool CollideTransformedShapePointAny(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter = nullptr) const override;

        [[nodiscard]]
        QueryResult CollectTransformedShapeChildren(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            const IQueryFilter* filter,
            AZStd::span<TransformedShape> children) const override;

        [[nodiscard]]
        QueryResult CollectTransformedShapeTriangles(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZStd::span<TransformedTriangle> triangles) const override;

        [[nodiscard]]
        bool GetTransformedShapeSurfaceNormal(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position,
            AZ::Vector3& normal) const override;

        [[nodiscard]]
        QueryResult GetTransformedShapeSupportingFace(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<WorldPosition> vertices) const override;

        [[nodiscard]]
        bool RetainShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const WorldTransform& transform,
            float uniformScale,
            TransformedShape& shape) const override;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const override;

        [[nodiscard]]
        bool CollideTransformedShapes(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            ITransformedShapeCollisionCollector& collector) const override;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            WorldHandle worldHandle,
            const ShapePlacement& firstShape,
            const ShapePlacement& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const override;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const override;

        [[nodiscard]]
        bool CastTransformedShape(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            ITransformedShapeCastCollector& collector) const override;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            WorldHandle worldHandle,
            const ShapePlacement& firstShape,
            const ShapePlacement& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const override;

        [[nodiscard]]
        bool RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            RaycastHit& hit) const override;

        [[nodiscard]]
        BufferResult RaycastClosestBatch(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results) const override;

        [[nodiscard]]
        QueryResult RaycastClosestPerBody(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const override;

        [[nodiscard]]
        bool RaycastAny(
            WorldHandle worldHandle,
            const RaycastRequest& request) const override;

        [[nodiscard]]
        QueryResult RaycastAll(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const override;

        [[nodiscard]]
        QueryResult OverlapPoint(
            WorldHandle worldHandle,
            const PointOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        bool OverlapPointAny(
            WorldHandle worldHandle,
            const PointOverlapRequest& request) const override;

        [[nodiscard]]
        QueryResult CollideShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<ShapeOverlapHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult OverlapShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        bool OverlapShapeAny(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request) const override;

        [[nodiscard]]
        bool CastShapeClosest(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult CastShapeClosestPerBody(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult CastShapeAll(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult OverlapBroadPhase(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request,
            AZStd::span<BroadPhaseHit> hits) const override;

        [[nodiscard]]
        bool OverlapBroadPhaseAny(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request) const override;

        [[nodiscard]]
        bool CastBroadPhaseClosest(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            BroadPhaseCastHit& hit) const override;

        [[nodiscard]]
        QueryResult CastBroadPhaseAll(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            AZStd::span<BroadPhaseCastHit> hits) const override;

        [[nodiscard]]
        QueryResult CollectShapesInBounds(
            WorldHandle worldHandle,
            const ShapeCollectionRequest& request,
            AZStd::span<TransformedShape> shapes) const override;

        [[nodiscard]]
        QueryResult GetSupportingFace(
            WorldHandle worldHandle,
            const SupportingFaceRequest& request,
            AZStd::span<WorldPosition> vertices) const override;

        [[nodiscard]]
        QueryResult CollectTriangles(
            WorldHandle worldHandle,
            const TriangleCollectionRequest& request,
            AZStd::span<TransformedTriangle> triangles) const override;

        [[nodiscard]]
        bool GetBroadPhaseBounds(
            WorldHandle worldHandle,
            BroadPhaseAabb& bounds) const override;

        bool OptimizeBroadPhase(WorldHandle worldHandle) override;

        [[nodiscard]]
        bool WereBodiesInContact(
            WorldHandle worldHandle,
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const override;

    private:
        friend class World;

        struct MaterialSlot final
        {
            JPH::RefConst<NativeMaterial> m_material;
            AZ::u32 m_generation = 1;
            AZ::u32 m_referenceCount = 0;
        };

        struct CustomConstraintProviderEntry final
        {
            ICustomConstraintProvider* m_provider = nullptr;
            AZ::u32 m_referenceCount = 0;
        };

        struct CustomPathProviderEntry final
        {
            ICustomPathProvider* m_provider = nullptr;
            AZ::u32 m_referenceCount = 0;
        };

        struct CustomShapeProviderEntry final
        {
            ICustomShapeProvider* m_provider = nullptr;
            AZ::u32 m_referenceCount = 0;
        };

        struct CookedShapeSlot final
        {
            JPH::RefConst<JPH::Shape> m_shape;
            AZStd::vector<MaterialHandle> m_materialHandles;
            AZStd::vector<CookedShapeHandle> m_childHandles;
            AZStd::vector<CustomShapeDependency> m_customDependencies;
            AZ::TypeId m_customProviderId = AZ::TypeId::CreateNull();
            AZ::u32 m_generation = 1;
            AZ::u32 m_referenceCount = 0;
            AZ::u32 m_parentCount = 0;
        };

        struct GroupFilterSlot final
        {
            JPH::Ref<JPH::GroupFilter> m_filter;
            AZ::u64 m_stateHash = 0;
            AZ::u32 m_generation = 1;
            AZ::u32 m_referenceCount = 0;
            AZ::u32 m_subGroupCount = 0;
            bool m_isCustom = false;
        };

        struct WorldSlot final
        {
            AZStd::unique_ptr<World> m_world;
            AZ::u32 m_generation = 1;
        };

        struct PathSlot final
        {
            JPH::RefConst<JPH::PathConstraintPath> m_path;
            AZ::TypeId m_customProviderId = AZ::TypeId::CreateNull();
            AZ::u64 m_customProviderVersion = 0;
            AZ::u64 m_sourceHash = 0;
            AZ::u32 m_generation = 1;
            AZ::u32 m_constraintCount = 0;
        };

        struct SoftBodyDefinitionSlot final
        {
            JPH::RefConst<JPH::SoftBodySharedSettings> m_settings;
            AZStd::vector<MaterialHandle> m_materialHandles;
            AZ::u32 m_generation = 1;
            AZ::u32 m_bodyCount = 0;
        };

        struct HairDefinitionSlot final
        {
            JPH::RefConst<JPH::HairSettings> m_settings;
            AZ::u32 m_generation = 1;
            AZ::u32 m_instanceCount = 0;
            float m_maximumHairToScalpDistanceSquared = 0.0f;
        };

        struct HairRuntime final
        {
            JPH::Ref<JPH::ComputeSystem> m_computeSystem;
            JPH::Ref<JPH::HairShaders> m_shaders;
        };

        struct SceneDefinitionSlot final
        {
            AZStd::shared_ptr<const SceneConfiguration> m_configuration;
            AZStd::vector<CookedShapeHandle> m_cookedShapeHandles;
            AZStd::vector<GroupFilterHandle> m_groupFilterHandles;
            AZStd::vector<PathHandle> m_pathHandles;
            AZStd::vector<SoftBodyDefinitionHandle> m_softBodyDefinitionHandles;

            AZStd::vector<MaterialHandle> m_ownedMaterialHandles;
            AZStd::vector<CookedShapeHandle> m_ownedCookedShapeHandles;
            AZStd::vector<GroupFilterHandle> m_ownedGroupFilterHandles;
            AZStd::vector<PathHandle> m_ownedPathHandles;
            AZStd::vector<SoftBodyDefinitionHandle> m_ownedSoftBodyDefinitionHandles;

            AZ::u32 m_generation = 1;
            AZ::u32 m_instanceCount = 0;
        };

        struct SkeletonDefinitionSlot final
        {
            JPH::Ref<JPH::Skeleton> m_skeleton;
            AZStd::unordered_map<AZ::Name, AZ::u32> m_jointIndices;
            AZStd::vector<SkeletonJoint> m_joints;
            AZ::u32 m_generation = 1;
            AZ::u32 m_mapperCount = 0;
            AZ::u32 m_poseCount = 0;
            AZ::u32 m_ragdollDefinitionCount = 0;
        };

        struct SkeletalAnimationSlot final
        {
            JPH::Ref<JPH::SkeletalAnimation> m_animation;
            AZStd::vector<AZ::Name> m_jointNames;
            AZ::u64 m_revision = 1;
            AZ::u32 m_generation = 1;
        };

        struct SkeletonPoseScratch final
        {
            AZStd::mutex m_mutex;
            JPH::SkeletonPose m_pose;
            JPH::Array<JPH::Mat44> m_localTransforms;
            AZStd::vector<AZ::u32> m_animationJointIndices;
            SkeletalAnimationHandle m_cachedAnimationHandle;
            AZ::u64 m_cachedAnimationRevision = 0;
        };

        struct SkeletonPoseSlot final
        {
            AZStd::shared_ptr<SkeletonPoseScratch> m_scratch;
            SkeletonDefinitionHandle m_skeletonHandle;
            AZ::u32 m_generation = 1;
        };

        struct SkeletonMapperScratch final
        {
            AZStd::mutex m_mutex;
            JPH::Array<JPH::Mat44> m_sourceTransforms;
            JPH::Array<JPH::Mat44> m_targetLocalTransforms;
            JPH::Array<JPH::Mat44> m_targetModelTransforms;
        };

        struct SkeletonMapperSlot final
        {
            JPH::Ref<JPH::SkeletonMapper> m_mapper;
            AZStd::shared_ptr<SkeletonMapperScratch> m_scratch;
            SkeletonDefinitionHandle m_sourceSkeletonHandle;
            SkeletonDefinitionHandle m_targetSkeletonHandle;
            AZ::u32 m_generation = 1;
            AZ::u32 m_sourceJointCount = 0;
            AZ::u32 m_targetJointCount = 0;
        };

        [[nodiscard]]
        World* FindWorldUnlocked(WorldHandle worldHandle);

        [[nodiscard]]
        const World* FindWorldUnlocked(WorldHandle worldHandle) const;

        [[nodiscard]]
        bool AcquireMaterials(
            AZStd::span<const MaterialHandle> materialHandles,
            JPH::PhysicsMaterialList& materials);

        void ReleaseMaterials(AZStd::span<const MaterialHandle> materialHandles);

        [[nodiscard]]
        ICustomConstraintProvider* AcquireCustomConstraintProvider(
            AZ::TypeId providerId,
            AZStd::span<const AZ::u8> data,
            AZ::u32& maximumRowCount,
            AZ::u32& stateByteCount,
            AZ::u64& providerVersion);

        void ReleaseCustomConstraintProvider(AZ::TypeId providerId);

        [[nodiscard]]
        ICustomPathProvider* AcquireCustomPathProvider(
            AZ::TypeId providerId,
            AZStd::span<const AZ::u8> data,
            float& maximumFraction,
            AZ::u64& providerVersion);

        void ReleaseCustomPathProvider(AZ::TypeId providerId);

        [[nodiscard]]
        ICustomShapeProvider* AcquireCustomShapeProvider(
            AZ::TypeId providerId,
            AZ::u64 requiredVersion = 0);

        void ReleaseCustomShapeProvider(AZ::TypeId providerId);

        [[nodiscard]]
        const MaterialSlot* FindMaterialUnlocked(MaterialHandle materialHandle) const;

        [[nodiscard]]
        MaterialHandle FindMaterialHandle(const JPH::PhysicsMaterial* material) const;

        [[nodiscard]]
        bool AcquireCookedShape(
            CookedShapeHandle cookedShapeHandle,
            JPH::RefConst<JPH::Shape>& shape);

        [[nodiscard]]
        CookedShapeHandle StoreCookedShape(
            const JPH::Shape* shape,
            AZStd::vector<MaterialHandle> materialHandles,
            AZStd::vector<CookedShapeHandle> childHandles,
            AZ::TypeId customProviderId = AZ::TypeId::CreateNull(),
            AZStd::vector<CustomShapeDependency> customDependencies = {});

        void ReleaseCookedShape(CookedShapeHandle cookedShapeHandle);

        [[nodiscard]]
        CookedShapeSlot* FindCookedShapeUnlocked(CookedShapeHandle cookedShapeHandle);

        [[nodiscard]]
        const CookedShapeSlot* FindCookedShapeUnlocked(CookedShapeHandle cookedShapeHandle) const;

        [[nodiscard]]
        bool AcquireCollisionGroup(
            const CollisionGroupConfiguration& configuration,
            JPH::CollisionGroup& collisionGroup);

        void ReleaseGroupFilter(GroupFilterHandle filterHandle);

        [[nodiscard]]
        bool GetGroupFilterStateHash(
            GroupFilterHandle filterHandle,
            AZ::u64& stateHash) const;

        [[nodiscard]]
        bool CaptureGroupFilterParticipantState(
            GroupFilterHandle filterHandle,
            AZStd::vector<AZ::u8>& state,
            AZ::TypeId& typeId,
            AZ::u64& stateHash,
            AZ::u32& version) const;

        [[nodiscard]]
        bool RestoreGroupFilterParticipantState(
            GroupFilterHandle filterHandle,
            AZStd::span<const AZ::u8> state,
            AZ::TypeId typeId,
            AZ::u64 stateHash,
            AZ::u32 version);

        [[nodiscard]]
        GroupFilterHandle StoreGroupFilter(
            JPH::Ref<JPH::GroupFilter> filter,
            AZ::u32 subGroupCount,
            AZ::u64 stateHash,
            bool isCustom);

        void RefreshGroupFilterInWorlds(GroupFilterHandle filterHandle);

        [[nodiscard]]
        GroupFilterSlot* FindGroupFilterUnlocked(GroupFilterHandle filterHandle);

        [[nodiscard]]
        const GroupFilterSlot* FindGroupFilterUnlocked(GroupFilterHandle filterHandle) const;

        [[nodiscard]]
        PathHandle StorePath(
            JPH::RefConst<JPH::PathConstraintPath> path,
            AZ::TypeId customProviderId = AZ::TypeId::CreateNull(),
            AZ::u64 customProviderVersion = 0,
            AZ::u64 sourceHash = 0);

        [[nodiscard]]
        bool AcquirePath(
            PathHandle pathHandle,
            JPH::RefConst<JPH::PathConstraintPath>& path);

        void ReleasePath(PathHandle pathHandle);

        [[nodiscard]]
        const PathSlot* FindPathUnlocked(PathHandle pathHandle) const;

        [[nodiscard]]
        SoftBodyDefinitionHandle StoreSoftBodyDefinition(
            JPH::RefConst<JPH::SoftBodySharedSettings> settings,
            AZStd::vector<MaterialHandle> materialHandles);

        [[nodiscard]]
        bool AcquireSoftBodyDefinition(
            SoftBodyDefinitionHandle definitionHandle,
            JPH::RefConst<JPH::SoftBodySharedSettings>& settings);

        void ReleaseSoftBodyDefinition(SoftBodyDefinitionHandle definitionHandle);

        [[nodiscard]]
        const SoftBodyDefinitionSlot* FindSoftBodyDefinitionUnlocked(
            SoftBodyDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool AcquireHairDefinition(
            HairDefinitionHandle definitionHandle,
            JPH::RefConst<JPH::HairSettings>& settings);

        void ReleaseHairDefinition(HairDefinitionHandle definitionHandle);

        [[nodiscard]]
        const HairDefinitionSlot* FindHairDefinitionUnlocked(
            HairDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool AcquireSceneDefinition(
            SceneDefinitionHandle definitionHandle,
            AZStd::shared_ptr<const SceneConfiguration>& configuration);

        void ReleaseSceneDefinition(SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        SceneDefinitionSlot* FindSceneDefinitionUnlocked(SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        const SceneDefinitionSlot* FindSceneDefinitionUnlocked(SceneDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool EnsureHairRuntime();

        [[nodiscard]]
        SkeletonDefinitionHandle StoreSkeletonDefinition(
            JPH::Ref<JPH::Skeleton> skeleton,
            AZStd::vector<SkeletonJoint> joints);

        [[nodiscard]]
        SkeletonDefinitionSlot* FindSkeletonDefinitionUnlocked(
            SkeletonDefinitionHandle skeletonHandle);

        [[nodiscard]]
        const SkeletonDefinitionSlot* FindSkeletonDefinitionUnlocked(
            SkeletonDefinitionHandle skeletonHandle) const;

        [[nodiscard]]
        bool AcquireSkeletonDefinition(
            SkeletonDefinitionHandle skeletonHandle,
            JPH::Ref<JPH::Skeleton>& skeleton);

        void ReleaseSkeletonDefinition(SkeletonDefinitionHandle skeletonHandle);

        [[nodiscard]]
        SkeletalAnimationHandle StoreSkeletalAnimation(
            JPH::Ref<JPH::SkeletalAnimation> animation,
            AZStd::vector<AZ::Name> jointNames);

        [[nodiscard]]
        const SkeletonMapperSlot* FindSkeletonMapperUnlocked(
            SkeletonMapperHandle mapperHandle) const;

        [[nodiscard]]
        SkeletalAnimationSlot* FindSkeletalAnimationUnlocked(
            SkeletalAnimationHandle animationHandle);

        [[nodiscard]]
        const SkeletalAnimationSlot* FindSkeletalAnimationUnlocked(
            SkeletalAnimationHandle animationHandle) const;

        [[nodiscard]]
        SkeletonPoseSlot* FindSkeletonPoseUnlocked(SkeletonPoseHandle poseHandle);

        [[nodiscard]]
        const SkeletonPoseSlot* FindSkeletonPoseUnlocked(SkeletonPoseHandle poseHandle) const;

        NativeRuntime m_nativeRuntime;
        mutable AZStd::mutex m_debugRendererMutex;
        AZStd::unique_ptr<DebugRenderer> m_debugRenderer;
        AZStd::atomic<AZ::u32> m_debugCaptureWorldCount = 0;
        AZStd::unique_ptr<ComponentDependencyManager> m_dependencyManager;
        SystemConfiguration m_configuration;
        AZ::JobContext* m_jobContext = nullptr;

        mutable AZStd::shared_mutex m_materialMutex;
        AZStd::vector<MaterialSlot> m_materialSlots;
        AZStd::vector<AZ::u32> m_freeMaterialSlots;

        mutable AZStd::mutex m_customConstraintProviderMutex;
        AZStd::unordered_map<AZ::TypeId, CustomConstraintProviderEntry> m_customConstraintProviders;

        mutable AZStd::mutex m_customPathProviderMutex;
        AZStd::unordered_map<AZ::TypeId, CustomPathProviderEntry> m_customPathProviders;

        mutable AZStd::shared_mutex m_customConvexShapeProviderMutex;
        AZStd::unordered_map<AZ::TypeId, ICustomConvexShapeProvider*> m_customConvexShapeProviders;
        mutable AZStd::shared_mutex m_customShapeProviderMutex;
        AZStd::unordered_map<AZ::TypeId, CustomShapeProviderEntry> m_customShapeProviders;

        mutable AZStd::shared_mutex m_cookedShapeMutex;
        AZStd::vector<CookedShapeSlot> m_cookedShapeSlots;
        AZStd::vector<AZ::u32> m_freeCookedShapeSlots;

        mutable AZStd::shared_mutex m_groupFilterMutex;
        AZStd::vector<GroupFilterSlot> m_groupFilterSlots;
        AZStd::vector<AZ::u32> m_freeGroupFilterSlots;

        mutable AZStd::shared_mutex m_pathMutex;
        AZStd::vector<PathSlot> m_pathSlots;
        AZStd::vector<AZ::u32> m_freePathSlots;

        mutable AZStd::shared_mutex m_softBodyDefinitionMutex;
        AZStd::vector<SoftBodyDefinitionSlot> m_softBodyDefinitionSlots;
        AZStd::vector<AZ::u32> m_freeSoftBodyDefinitionSlots;

        mutable AZStd::shared_mutex m_hairDefinitionMutex;
        AZStd::vector<HairDefinitionSlot> m_hairDefinitionSlots;
        AZStd::vector<AZ::u32> m_freeHairDefinitionSlots;
        AZStd::unique_ptr<HairRuntime> m_hairRuntime;

        mutable AZStd::shared_mutex m_sceneDefinitionMutex;
        AZStd::vector<SceneDefinitionSlot> m_sceneDefinitionSlots;
        AZStd::vector<AZ::u32> m_freeSceneDefinitionSlots;

        mutable AZStd::shared_mutex m_skeletonMutex;
        AZStd::vector<SkeletonDefinitionSlot> m_skeletonDefinitionSlots;
        AZStd::vector<AZ::u32> m_freeSkeletonDefinitionSlots;
        AZStd::vector<SkeletalAnimationSlot> m_skeletalAnimationSlots;
        AZStd::vector<AZ::u32> m_freeSkeletalAnimationSlots;
        AZStd::vector<SkeletonMapperSlot> m_skeletonMapperSlots;
        AZStd::vector<AZ::u32> m_freeSkeletonMapperSlots;
        AZStd::vector<SkeletonPoseSlot> m_skeletonPoseSlots;
        AZStd::vector<AZ::u32> m_freeSkeletonPoseSlots;

        mutable AZStd::shared_mutex m_worldMutex;
        AZStd::vector<WorldSlot> m_worldSlots;
        AZStd::vector<AZ::u32> m_freeWorldSlots;
        WorldHandle m_defaultWorldHandle;

        bool m_initialized = false;
        bool m_registered = false;
    };
} // namespace Jolt
