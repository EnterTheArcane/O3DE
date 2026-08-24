/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Capabilities.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/MaterialInternal.h>
#include <Jolt/HandleSlotReservation.h>
#include <Jolt/System.h>

#include <AzCore/Jobs/JobContext.h>
#include <AzCore/std/containers/array.h>
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
    namespace Internal
    {
        class OperationPool;
    } // namespace Internal

    enum class SystemRegistration : AZ::u8
    {
        Global,
        Isolated,
    };

    class ComponentDependencyManager;
    class DebugRenderer;
    class World;

    class JOLT_API RuntimeImplementation
    {
    public:
        RuntimeImplementation(
            SystemConfiguration configuration,
            AZ::JobContext* jobContext,
            SystemRegistration registration = SystemRegistration::Global);
        ~RuntimeImplementation();

        AZ_DISABLE_COPY_MOVE(RuntimeImplementation);

        constexpr explicit operator bool() const noexcept
        {
            return m_initialized;
        }

        [[nodiscard]]
        const SystemConfiguration& GetConfiguration() const;

        [[nodiscard]]
        RuntimeInfo GetRuntimeInfo() const;

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IBodyPairCollider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IContactCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomConstraintProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomConvexShapeProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomPathProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomShapeProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IGroupFilter* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ISimulationShapeFilter* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ISoftBodyContactCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IStepListener* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IVehicleCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IVehicleCollisionFilter* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IVirtualCharacterContactCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationStatus UnregisterExtension(ExtensionHandle extensionHandle);

        [[nodiscard]]
        bool GetExtensionInformation(
            ExtensionHandle extensionHandle,
            ExtensionInformation& information) const;

        [[nodiscard]]
        bool RetainExtension(
            ExtensionHandle extensionHandle,
            ExtensionKind kind);

        void ReleaseExtension(ExtensionHandle extensionHandle);

        [[nodiscard]]
        MaterialHandle CreateMaterial(const MaterialConfiguration& configuration);

        bool DestroyMaterial(MaterialHandle materialHandle);

        [[nodiscard]]
        bool IsValid(MaterialHandle materialHandle) const;

        [[nodiscard]]
        CookedShapeHandle CookShape(const ShapeConfiguration& configuration);

        [[nodiscard]]
        Operation<CookedShapeHandle> CookShapeAsync(const ShapeConfiguration& configuration);

        [[nodiscard]]
        CookedShapeHandle CookShape(const CookedCompoundShapeConfiguration& configuration);

        [[nodiscard]]
        Operation<CookedShapeHandle> CookShapeAsync(const CookedCompoundShapeConfiguration& configuration);

        [[nodiscard]]
        CookedShapeHandle CookShape(const CookedDecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        Operation<CookedShapeHandle> CookShapeAsync(const CookedDecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        bool ExportShape(
            CookedShapeHandle cookedShapeHandle,
            CookedShapeArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles,
            AZStd::vector<CookedShapeHandle>& childShapeHandles) const;

        [[nodiscard]]
        CookedShapeHandle ImportShape(
            const CookedShapeArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles,
            AZStd::span<const CookedShapeHandle> childShapeHandles);

        bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle);

        [[nodiscard]]
        bool IsValid(CookedShapeHandle cookedShapeHandle) const;

        [[nodiscard]]
        bool GetStats(
            CookedShapeHandle cookedShapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetStatsRecursive(
            CookedShapeHandle cookedShapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetProperties(
            CookedShapeHandle cookedShapeHandle,
            ShapeProperties& properties) const;

        [[nodiscard]]
        bool GetUserData(
            CookedShapeHandle cookedShapeHandle,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetCustomConvexShapeInfo(
            CookedShapeHandle cookedShapeHandle,
            CustomConvexShapeInfo& info) const;

        [[nodiscard]]
        bool GetCustomShapeInfo(
            CookedShapeHandle cookedShapeHandle,
            CustomShapeInfo& info) const;

        [[nodiscard]]
        BufferResult GetCustomShapeDependencies(
            CookedShapeHandle cookedShapeHandle,
            AZStd::span<CustomShapeDependency> dependencies) const;

        [[nodiscard]]
        bool GetSubShapeUserData(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetDirectChildShape(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            CookedShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const;

        [[nodiscard]]
        BufferResult GetMeshMaterials(
            CookedShapeHandle cookedShapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const;

        [[nodiscard]]
        bool GetMeshTriangleUserData(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const;

        [[nodiscard]]
        bool GetCompoundChildCount(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32& childCount) const;

        [[nodiscard]]
        bool GetCompoundChild(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32 childIndex,
            CookedCompoundChildConfiguration& child) const;

        [[nodiscard]]
        bool GetCompoundChildIndex(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const;

        [[nodiscard]]
        bool Raycast(
            CookedShapeHandle cookedShapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            CookedRaycastHit& hit) const;

        [[nodiscard]]
        GroupFilterHandle CreateGroupFilter(
            AZ::u32 subGroupCount,
            ExtensionHandle extensionHandle);

        [[nodiscard]]
        GroupFilterHandle CreateGroupFilterTable(
            const GroupFilterTableConfiguration& configuration);

        bool DestroyGroupFilter(GroupFilterHandle filterHandle);

        [[nodiscard]]
        bool IsValid(GroupFilterHandle filterHandle) const;

        bool NotifyGroupFilterChanged(GroupFilterHandle filterHandle);

        [[nodiscard]]
        bool GetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool& enabled) const;

        bool SetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool enabled);

        [[nodiscard]]
        PathHandle CreatePath(const HermitePathConfiguration& configuration);

        [[nodiscard]]
        PathHandle CreatePath(const CustomPathConfiguration& configuration);

        bool DestroyPath(PathHandle pathHandle);

        [[nodiscard]]
        bool IsValid(PathHandle pathHandle) const;

        [[nodiscard]]
        bool GetPathState(
            PathHandle pathHandle,
            PathState& state) const;

        [[nodiscard]]
        bool GetCustomPathInfo(
            PathHandle pathHandle,
            CustomPathInfo& info) const;

        bool SamplePath(
            PathHandle pathHandle,
            float fraction,
            PathSample& sample) const;

        bool FindClosestPathPoint(
            PathHandle pathHandle,
            const AZ::Vector3& position,
            float fractionHint,
            PathSample& sample) const;

        [[nodiscard]]
        SkeletonDefinitionHandle CreateSkeletonDefinition(
            const SkeletonDefinitionConfiguration& configuration);

        [[nodiscard]]
        SkeletonDefinitionHandle CreateSkeletonDefinition(
            const SkeletonDefinitionSource& source);

        [[nodiscard]]
        bool ExportSkeletonDefinition(
            SkeletonDefinitionHandle skeletonHandle,
            SkeletonDefinitionArchive& archive) const;

        [[nodiscard]]
        SkeletonDefinitionHandle ImportSkeletonDefinition(
            const SkeletonDefinitionArchive& archive);

        bool DestroySkeletonDefinition(SkeletonDefinitionHandle skeletonHandle);

        [[nodiscard]]
        bool IsValid(SkeletonDefinitionHandle skeletonHandle) const;

        [[nodiscard]]
        QueryResult GetSkeletonJoints(
            SkeletonDefinitionHandle skeletonHandle,
            AZStd::span<SkeletonJoint> joints) const;

        [[nodiscard]]
        bool FindSkeletonJoint(
            SkeletonDefinitionHandle skeletonHandle,
            AZ::Name jointName,
            AZ::u32& jointIndex) const;

        [[nodiscard]]
        SkeletalAnimationHandle CreateSkeletalAnimation(
            const SkeletalAnimationConfiguration& configuration);

        [[nodiscard]]
        SkeletalAnimationHandle CreateSkeletalAnimation(
            const SkeletalAnimationSource& source);

        [[nodiscard]]
        bool ExportSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationArchive& archive) const;

        [[nodiscard]]
        SkeletalAnimationHandle ImportSkeletalAnimation(
            const SkeletalAnimationArchive& archive);

        bool UpdateSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            const SkeletalAnimationConfiguration& configuration);

        bool DestroySkeletalAnimation(SkeletalAnimationHandle animationHandle);

        bool DestroySkeletonResources(
            SkeletonDefinitionHandle skeletonHandle,
            AZStd::span<const SkeletalAnimationHandle> animationHandles);

        [[nodiscard]]
        bool IsValid(SkeletalAnimationHandle animationHandle) const;

        [[nodiscard]]
        bool GetSkeletalAnimationState(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationState& state) const;

        [[nodiscard]]
        bool GetSkeletalAnimatedJointName(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex,
            AZ::Name& jointName) const;

        [[nodiscard]]
        QueryResult GetSkeletalAnimationKeyframes(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex,
            AZStd::span<SkeletalAnimationKeyframe> keyframes) const;

        bool SetSkeletalAnimationLooping(
            SkeletalAnimationHandle animationHandle,
            bool isLooping);

        bool ScaleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            float scale);

        [[nodiscard]]
        SkeletonPoseHandle CreateSkeletonPose(SkeletonDefinitionHandle skeletonHandle);

        bool DestroySkeletonPose(SkeletonPoseHandle poseHandle);

        [[nodiscard]]
        bool IsValid(SkeletonPoseHandle poseHandle) const;

        [[nodiscard]]
        bool GetSkeletonPoseState(
            SkeletonPoseHandle poseHandle,
            SkeletonPoseState& state) const;

        bool SetSkeletonPoseRootOffset(
            SkeletonPoseHandle poseHandle,
            const WorldPosition& rootOffset);

        bool SetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<const AZ::Transform> localTransforms);

        bool SetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<const AZ::Transform> modelTransforms);

        [[nodiscard]]
        QueryResult GetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<AZ::Transform> localTransforms) const;

        [[nodiscard]]
        QueryResult GetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<AZ::Transform> modelTransforms) const;

        bool SampleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletonPoseHandle poseHandle,
            float time);

        [[nodiscard]]
        SkeletonMapperHandle CreateSkeletonMapper(
            const SkeletonMapperConfiguration& configuration);

        bool DestroySkeletonMapper(SkeletonMapperHandle mapperHandle);

        [[nodiscard]]
        bool IsValid(SkeletonMapperHandle mapperHandle) const;

        [[nodiscard]]
        bool GetSkeletonMapperState(
            SkeletonMapperHandle mapperHandle,
            SkeletonMapperState& state) const;

        [[nodiscard]]
        QueryResult GetSkeletonMapperMappings(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperMappingState> mappings) const;

        [[nodiscard]]
        bool GetSkeletonMapperChainState(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            SkeletonMapperChainState& state) const;

        [[nodiscard]]
        QueryResult GetSkeletonMapperSourceChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            AZStd::span<AZ::u32> jointIndices) const;

        [[nodiscard]]
        QueryResult GetSkeletonMapperTargetChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            AZStd::span<AZ::u32> jointIndices) const;

        [[nodiscard]]
        QueryResult GetSkeletonMapperUnmappedJoints(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperUnmappedJoint> joints) const;

        [[nodiscard]]
        QueryResult GetSkeletonMapperLockedTranslations(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperLockedTranslation> translations) const;

        [[nodiscard]]
        bool GetMappedSkeletonJoint(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 sourceJointIndex,
            AZ::u32& targetJointIndex) const;

        [[nodiscard]]
        bool IsSkeletonJointTranslationLocked(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 targetJointIndex,
            bool& locked) const;

        [[nodiscard]]
        bool MapSkeletonPose(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<const AZ::Transform> sourceModelTransforms,
            AZStd::span<const AZ::Transform> targetLocalTransforms,
            AZStd::span<AZ::Transform> targetModelTransforms) const;

        [[nodiscard]]
        bool MapSkeletonPoseReverse(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<const AZ::Transform> targetModelTransforms,
            AZStd::span<AZ::Transform> sourceModelTransforms) const;

        [[nodiscard]]
        SoftBodyDefinitionHandle CreateSoftBodyDefinition(
            const SoftBodyDefinitionConfiguration& configuration,
            SoftBodyOptimizationRemap* optimizationRemap = nullptr);

        [[nodiscard]]
        bool ExportSoftBodyDefinition(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles) const;

        [[nodiscard]]
        SoftBodyDefinitionHandle ImportSoftBodyDefinition(
            const SoftBodyDefinitionArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles);

        bool DestroySoftBodyDefinition(SoftBodyDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(SoftBodyDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool GetSoftBodyDefinitionState(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionState& state) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionDihedralBendConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyDihedralBendConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionEdgeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyEdgeConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionFaces(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyFace> faces) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionInverseBinds(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyInverseBind> inverseBinds) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionLongRangeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyLongRangeConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionMaterials(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<MaterialHandle> materials) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionRodBendTwistConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionRodStretchShearConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionSkinConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodySkinConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionVertices(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVertex> vertices) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionVolumeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVolumeConstraint> constraints) const;

        [[nodiscard]]
        HairDefinitionHandle CreateHairDefinition(
            const HairDefinitionConfiguration& configuration);

        bool DestroyHairDefinition(HairDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(HairDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool GetHairDefinitionState(
            HairDefinitionHandle definitionHandle,
            HairDefinitionState& state) const;

        [[nodiscard]]
        QueryResult GetHairNeutralDensity(
            HairDefinitionHandle definitionHandle,
            AZStd::span<float> density) const;

        bool SkinHairScalpVertices(
            HairDefinitionHandle definitionHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms,
            AZStd::span<AZ::Transform> preparedJointTransforms,
            AZStd::span<AZ::Vector3> scalpVertices) const;

        [[nodiscard]]
        SceneDefinitionHandle CreateSceneDefinition(const SceneConfiguration& configuration);

        [[nodiscard]]
        SceneDefinitionHandle CreateSceneDefinition(const SceneAssetData& assetData);

        [[nodiscard]]
        bool BuildSceneAsset(
            const SceneSourceData& sourceData,
            SceneAssetData& assetData);

        bool DestroySceneDefinition(SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(SceneDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool GetSceneDefinitionState(
            SceneDefinitionHandle definitionHandle,
            SceneDefinitionState& state) const;

        [[nodiscard]]
        SceneInstanceHandle InstantiateScene(
            WorldHandle worldHandle,
            SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        Operation<SceneInstanceHandle> InstantiateSceneAsync(
            WorldHandle worldHandle,
            SceneDefinitionHandle definitionHandle);

        bool DestroySceneInstance(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle);

        bool DestroySceneResources(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle) const;

        [[nodiscard]]
        bool GetSceneInstanceState(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            SceneInstanceState& state) const;

        [[nodiscard]]
        QueryResult GetSceneBodies(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<BodyHandle> bodyHandles) const;

        [[nodiscard]]
        QueryResult GetSceneConstraints(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const;

        [[nodiscard]]
        WorldHandle CreateWorld(const WorldConfiguration& configuration);

        bool DestroyWorld(WorldHandle worldHandle);

        [[nodiscard]]
        WorldHandle GetDefaultWorldHandle() const;

        [[nodiscard]]
        const IWorldQueries* GetWorldQueries(WorldHandle worldHandle) const;

        [[nodiscard]]
        bool IsValid(WorldHandle worldHandle) const;

        [[nodiscard]]
        bool GetWorldGravity(
            WorldHandle worldHandle,
            AZ::Vector3& gravity) const;

        bool SetWorldGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity);

        [[nodiscard]]
        bool GetSimulationConfiguration(
            WorldHandle worldHandle,
            SimulationConfiguration& configuration) const;

        bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration);

        [[nodiscard]]
        bool GetWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            WorldRuntimeConfiguration& configuration) const;

        bool UpdateWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration);

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

        [[nodiscard]]
        HairHandle CreateHair(
            WorldHandle worldHandle,
            const HairConfiguration& configuration);

        bool DestroyHair(
            WorldHandle worldHandle,
            HairHandle hairHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            HairHandle hairHandle) const;

        bool SetHairTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const WorldTransform& worldTransform,
            bool teleport);

        bool SetHairScalpToHeadTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& scalpToHeadTransform);

        bool UpdateHair(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        bool EnableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        bool DisableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle);

        [[nodiscard]]
        bool GetHairState(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            HairState& state) const;

        [[nodiscard]]
        bool GetHairReadback(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const;

        [[nodiscard]]
        QueryResult GetHairVertexStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairVertexState> states) const;

        [[nodiscard]]
        QueryResult GetHairRenderPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const;

        [[nodiscard]]
        QueryResult GetHairScalpPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const;

        [[nodiscard]]
        QueryResult GetHairGridCellStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairGridCellState> states) const;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const ShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const CompoundShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const DecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            CookedShapeHandle cookedShapeHandle);

        [[nodiscard]]
        ShapeHandle CloneShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle);

        [[nodiscard]]
        ShapeHandle ScaleShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale);

        bool DestroyShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) const;

        [[nodiscard]]
        bool GetShapeStats(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetShapeStatsRecursive(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetShapeProperties(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeProperties& properties) const;

        [[nodiscard]]
        bool GetShapeSubmergedVolume(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const;

        [[nodiscard]]
        bool GetPrimitiveShapeState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            PrimitiveShapeState& state) const;

        [[nodiscard]]
        bool GetConvexHullState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ConvexHullState& state) const;

        [[nodiscard]]
        BufferResult GetConvexHullPointsRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Vector3> points) const;

        [[nodiscard]]
        BufferResult GetConvexHullPlanesRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Plane> planes) const;

        [[nodiscard]]
        BufferResult GetConvexHullFaceVertexIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 faceIndex,
            AZStd::span<AZ::u32> vertexIndices) const;

        [[nodiscard]]
        bool GetShapeMaterial(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const;

        [[nodiscard]]
        bool GetShapeSurfaceNormal(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        bool GetShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetShapeSubShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetDirectChildShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const;

        [[nodiscard]]
        bool GetDecoratedShapeConfiguration(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            DecoratedShapeConfiguration& configuration) const;

        [[nodiscard]]
        BufferResult GetMeshMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const;

        [[nodiscard]]
        bool GetMeshTriangleUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const;

        [[nodiscard]]
        bool IsShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) const;

        [[nodiscard]]
        bool MakeShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const;

        [[nodiscard]]
        bool GetHeightfieldState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            HeightfieldState& state) const;

        [[nodiscard]]
        bool GetHeightfieldPosition(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const;

        [[nodiscard]]
        bool ProjectOntoHeightfield(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::Vector3& surfacePosition,
            SubShapeId& subShapeId) const;

        [[nodiscard]]
        bool IsHeightfieldNoCollision(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const;

        [[nodiscard]]
        QueryResult GetHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<float> heights) const;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterialIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<AZ::u8> materialIndices) const;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        [[nodiscard]]
        bool GetHeightfieldSubShapeCoordinates(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const;

        bool UpdateHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const float> heights,
            const HeightfieldUpdateConfiguration& configuration = {});

        bool UpdateHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const AZ::u8> materialIndices,
            AZStd::span<const MaterialHandle> materialHandles,
            bool activateBodies = true);

        bool AddMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            const CompoundChildConfiguration& child,
            AZ::u32 insertionIndex,
            AZ::u32& childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool RemoveMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool UpdateMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const CompoundChildConfiguration& child,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool UpdateMutableCompoundChildTransforms(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 startIndex,
            AZStd::span<const AZ::Vector3> positions,
            AZStd::span<const AZ::Quaternion> rotations,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool AdjustMutableCompoundCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            bool updateMassProperties,
            bool activateBodies);

        [[nodiscard]]
        bool GetCompoundChildCount(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32& childCount) const;

        [[nodiscard]]
        bool GetCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const;

        [[nodiscard]]
        bool GetCompoundChildIndex(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const;

        [[nodiscard]]
        BodyHandle CreateBody(
            WorldHandle worldHandle,
            const BodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle CreateBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const BodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle CreateSoftBody(
            WorldHandle worldHandle,
            const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle CreateSoftBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const SoftBodyConfiguration& configuration);

        bool AddBodyToSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate);

        bool AddBodiesToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles,
            bool activate);

        bool RemoveBodyFromSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool RemoveBodiesFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        bool DestroyBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool DestroyBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        [[nodiscard]]
        bool IsBodyInSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const;

        bool SetBodyMoveEventsEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        RagdollDefinitionHandle CreateRagdollDefinition(
            WorldHandle worldHandle,
            const RagdollDefinitionConfiguration& configuration);

        bool DestroyRagdollDefinition(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        QueryResult GetRagdollBodyConstraintIndices(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<AZ::s32> constraintIndices) const;

        [[nodiscard]]
        QueryResult GetRagdollConstraintBodyPairs(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<RagdollConstraintBodyPair> bodyPairs) const;

        [[nodiscard]]
        RagdollHandle CreateRagdoll(
            WorldHandle worldHandle,
            const RagdollConfiguration& configuration);

        bool AddRagdollToSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            bool activate);

        bool RemoveRagdollFromSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        bool DestroyRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        bool IsRagdollInSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        bool GetRagdollState(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            RagdollState& state) const;

        bool SetRagdollCollisionGroupId(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::u32 collisionGroupId);

        [[nodiscard]]
        QueryResult GetRagdollBodies(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<BodyHandle> bodyHandles) const;

        [[nodiscard]]
        QueryResult GetRagdollConstraints(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const;

        bool ActivateRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        bool SetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms);

        [[nodiscard]]
        QueryResult GetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const;

        bool DriveRagdollKinematically(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime);

        bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> modelTransforms);

        bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime);

        bool ResetRagdollWarmStart(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        bool SetRagdollVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity,
            AZ::Vector3 angularVelocity);

        bool SetRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity);

        bool AddRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity);

        bool AddRagdollImpulse(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 impulse);

        [[nodiscard]]
        ConstraintHandle CreateConstraint(
            WorldHandle worldHandle,
            const ConstraintConfiguration& configuration);

        bool AddConstraintToSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool AddConstraintsToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles);

        bool RemoveConstraintFromSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool RemoveConstraintsFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles);

        bool DestroyConstraint(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool DestroyConstraints(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles);

        [[nodiscard]]
        bool IsConstraintInSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const;

        bool SetConstraintEnabled(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            bool enabled);

        [[nodiscard]]
        bool GetConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintState& state) const;

        [[nodiscard]]
        bool GetConstraintConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintConfiguration& configuration) const;

        [[nodiscard]]
        bool GetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64& userData) const;

        bool SetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float& debugDrawSize) const;

        bool SetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float debugDrawSize);

        [[nodiscard]]
        bool GetConstraintMeasurements(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintMeasurements& measurements) const;

        [[nodiscard]]
        bool GetCustomConstraintInfo(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            CustomConstraintInfo& info) const;

        [[nodiscard]]
        BufferResult GetCustomConstraintImpulses(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<float> impulses) const;

        [[nodiscard]]
        BufferResult GetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<AZ::u8> state) const;

        bool SetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const AZ::u8> state);

        bool ResetConstraintWarmStart(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool UpdateConstraintSolverConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const ConstraintSolverConfiguration& configuration);

        bool UpdateConeLimit(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float halfConeAngle);

        bool UpdateDistanceLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumDistance,
            float maximumDistance,
            const SpringConfiguration& spring);

        bool UpdateHingeLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumAngle,
            float maximumAngle,
            const SpringConfiguration& spring,
            float maximumFrictionTorque);

        bool UpdateHingeMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetAngle,
            float targetAngularVelocity);

        bool SetHingeTargetOrientation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const AZ::Quaternion& targetOrientation);

        bool UpdatePathMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPathFraction,
            float targetVelocity);

        bool UpdatePathProperties(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            PathHandle pathHandle,
            float pathFraction,
            float maximumFrictionForce);

        bool UpdatePointAnchors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintSpace space,
            const WorldPosition& firstPoint,
            const WorldPosition& secondPoint);

        bool UpdatePulleyLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumLength,
            float maximumLength);

        bool UpdateSixDofLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const SixDofAxisLimitConfiguration> axes);

        bool UpdateSixDofMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const MotorConfiguration> motors,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation,
            const AZ::Vector3& targetPosition,
            const AZ::Vector3& targetVelocity);

        bool UpdateSliderMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPosition,
            float targetVelocity);

        bool UpdateSliderLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumPosition,
            float maximumPosition,
            const SpringConfiguration& spring,
            float maximumFrictionForce);

        bool UpdateSwingTwistMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& swingMotor,
            const MotorConfiguration& twistMotor,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation);

        bool UpdateSwingTwistLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float normalHalfConeAngle,
            float planeHalfConeAngle,
            float twistMinimumAngle,
            float twistMaximumAngle,
            float maximumFrictionTorque);

        [[nodiscard]]
        bool GetBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyState& state) const;

        [[nodiscard]]
        bool GetBodyCenterOfMassTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldTransform& transform) const;

        [[nodiscard]]
        bool GetBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyConfiguration& configuration) const;

        [[nodiscard]]
        bool GetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64& userData) const;

        bool SetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        bool GetBodySimulationStatistics(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodySimulationStatistics& statistics) const;

        bool ApplyBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyConfiguration& configuration);

        [[nodiscard]]
        QueryResult GetSoftBodyFaces(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyFace> faces) const;

        [[nodiscard]]
        bool GetSoftBodyLocalBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Aabb& bounds) const;

        [[nodiscard]]
        QueryResult GetSoftBodyMaterials(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<MaterialHandle> materials) const;

        [[nodiscard]]
        QueryResult GetSoftBodyRodStates(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyRodState> rods) const;

        [[nodiscard]]
        bool GetSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SoftBodyRuntimeConfiguration& configuration) const;

        bool ApplySoftBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        QueryResult GetSoftBodyVertices(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyVertex> vertices) const;

        [[nodiscard]]
        bool GetSoftBodyVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& volume) const;

        bool RecalculateSoftBodyMassProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate);

        bool SkinSoftBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll);

        bool UpdateSoftBodyManually(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float deltaTime);

        bool UpdateSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyRuntimeConfiguration& configuration);

        bool SetSoftBodyVertexInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            float inverseMass);

        bool SetSoftBodyVertexInverseMasses(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses);

        bool SetSoftBodyVertexVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity);

        bool SetSoftBodyVertexVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities);

        [[nodiscard]]
        VirtualCharacterHandle CreateVirtualCharacter(
            WorldHandle worldHandle,
            const VirtualCharacterConfiguration& configuration);

        bool DestroyVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) const;

        [[nodiscard]]
        bool GetVirtualCharacterState(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterState& state) const;

        [[nodiscard]]
        bool GetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64& userData) const;

        bool SetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        QueryResult CheckVirtualCharacterCollision(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter = nullptr) const;

        bool UpdateVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterRuntimeConfiguration& configuration);

        bool SetVirtualCharacterShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth);

        bool SetVirtualCharacterInnerBodyShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle);

        bool SetVirtualCharacterTransform(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const WorldTransform& transform);

        bool SetVirtualCharacterVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& velocity);

        [[nodiscard]]
        bool CancelVirtualCharacterVelocityTowardsSteepSlopes(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity,
            AZ::Vector3& adjustedVelocity) const;

        bool BeginVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        bool EndVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        bool SetVirtualCharacterContactCallbacks(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ExtensionHandle extensionHandle);

        [[nodiscard]]
        bool CanVirtualCharacterWalkStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity) const;

        bool WalkVirtualCharacterStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter = nullptr);

        bool StickVirtualCharacterToFloor(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter = nullptr);

        bool RefreshVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const IQueryFilter* filter = nullptr);

        bool UpdateVirtualCharacterGroundVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        QueryResult GetVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZStd::span<VirtualCharacterContact> contacts) const;

        [[nodiscard]]
        bool HasVirtualCharacterCollidedWith(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool HaveVirtualCharactersCollided(
            WorldHandle worldHandle,
            VirtualCharacterHandle firstCharacterHandle,
            VirtualCharacterHandle secondCharacterHandle) const;

        bool UpdateVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration);

        bool EnableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterUpdateConfiguration& configuration);

        bool DisableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        CharacterHandle CreateCharacter(
            WorldHandle worldHandle,
            const CharacterConfiguration& configuration);

        bool DestroyCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) const;

        [[nodiscard]]
        bool GetCharacterState(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterState& state) const;

        [[nodiscard]]
        bool GetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64& userData) const;

        bool SetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterRuntimeConfiguration& configuration) const;

        QueryResult CheckCharacterCollision(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter = nullptr) const;

        bool UpdateCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterRuntimeConfiguration& configuration);

        bool SetCharacterShape(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth);

        bool SetCharacterTransform(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetCharacterVelocity(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity);

        bool AddCharacterImpulse(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& impulse);

        bool ApplyVehicleEngineDamping(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float deltaTime);

        bool ApplyVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float torque,
            float deltaTime);

        [[nodiscard]]
        bool CalculateVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float acceleration,
            float& torque) const;

        [[nodiscard]]
        VehicleHandle CreateWheeledVehicle(
            WorldHandle worldHandle,
            const WheeledVehicleConfiguration& configuration);

        [[nodiscard]]
        VehicleHandle CreateMotorcycle(
            WorldHandle worldHandle,
            const MotorcycleConfiguration& configuration);

        [[nodiscard]]
        VehicleHandle CreateTrackedVehicle(
            WorldHandle worldHandle,
            const TrackedVehicleConfiguration& configuration);

        bool DestroyVehicle(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle) const;

        [[nodiscard]]
        QueryResult GetWheeledVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        QueryResult GetMotorcycleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        QueryResult GetTrackedVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        bool GetVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleCollisionConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float& ratio) const;

        [[nodiscard]]
        bool GetVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleEngineConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehiclePowertrainState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehiclePowertrainState& state) const;

        [[nodiscard]]
        bool GetVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleTransmissionConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            VehicleTrackConfiguration& configuration) const;

        [[nodiscard]]
        bool GetWheelLocalBasis(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            WheelBasis& basis) const;

        [[nodiscard]]
        bool GetWheelLocalTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            AZ::Transform& transform) const;

        [[nodiscard]]
        bool GetWheelWorldTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            WorldTransform& transform) const;

        [[nodiscard]]
        QueryResult QueryVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const;

        [[nodiscard]]
        QueryResult QueryVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleDifferentialConfiguration> differentials) const;

        bool SetTrackedVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const TrackedVehicleInput& input);

        bool SetVehicleCallbacks(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            ExtensionHandle extensionHandle);

        bool SetVehicleCollisionFilter(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            ExtensionHandle extensionHandle);

        bool SetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float ratio);

        bool SetVehiclePowertrainControl(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehiclePowertrainControl& control);

        bool SetVehicleTrackAngularVelocity(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            float angularVelocity);

        bool SetWheelMotion(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const WheelMotion& motion);

        bool SetWheeledVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const WheeledVehicleInput& input);

        bool UpdateMotorcycleController(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const MotorcycleControllerUpdateConfiguration& configuration);

        bool UpdateVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars);

        bool UpdateVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleCollisionConfiguration& configuration);

        bool UpdateVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleDifferentialConfiguration> differentials);

        bool UpdateVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleEngineConfiguration& configuration);

        bool UpdateVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleRuntimeConfiguration& configuration);

        bool UpdateVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleTransmissionConfiguration& configuration);

        bool UpdateVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration);

        [[nodiscard]]
        StateSnapshotHandle CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        [[nodiscard]]
        Operation<StateSnapshotHandle> CaptureBodyStateAsync(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            StateSnapshotHandle snapshotHandle);

        StateRestoreResult RestoreBodyState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        Operation<StateRestoreResult> RestoreBodyStateAsync(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle);

        [[nodiscard]]
        Operation<StateSnapshotHandle> CaptureWorldStateAsync(WorldHandle worldHandle);

        bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        bool CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles,
            AZStd::span<const AZ::u32> partitionBodyCounts,
            AZStd::span<StateSnapshotHandle> snapshotHandles);

        bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles,
            StateSnapshotArchive& archive);

        bool ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive,
            AZStd::span<StateSnapshotHandle> snapshotHandles);

        bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const;

        [[nodiscard]]
        StateRestoreResult RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        Operation<StateRestoreResult> RestoreWorldStateAsync(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateRestoreResult RestoreWorldStateParts(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles);

        bool ValidateWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result);

        [[nodiscard]]
        bool GetWorldStateDigest(
            WorldHandle worldHandle,
            WorldStateDigest& digest) const;

        [[nodiscard]]
        bool GetWorldStatistics(
            WorldHandle worldHandle,
            WorldStatistics& statistics) const;

        bool ConfigurePerformanceStatistics(
            WorldHandle worldHandle,
            PerformanceStatisticsFlags flags);

        [[nodiscard]]
        bool GetPerformanceStatistics(
            WorldHandle worldHandle,
            WorldPerformanceStatistics& statistics,
            bool reset);

        [[nodiscard]]
        DiagnosticStatisticsResult GetBroadPhaseStatistics(
            WorldHandle worldHandle,
            AZStd::span<BroadPhaseStatistics> statistics,
            bool reset);

        [[nodiscard]]
        DiagnosticStatisticsResult GetNarrowPhaseStatistics(
            AZStd::span<NarrowPhaseStatistics> statistics,
            bool reset);

        bool DrawDebug(
            WorldHandle worldHandle,
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer,
            const IDebugFilter* filter = nullptr);

        bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration);

        [[nodiscard]]
        bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const;

        QueryResult GetBodies(
            WorldHandle worldHandle,
            BodyKind kind,
            bool activeOnly,
            AZStd::span<BodyHandle> bodies) const;

        [[nodiscard]]
        bool GetBodyId(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyId& bodyId) const;

        bool ActivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool ActivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        bool ActivateBodiesInBounds(
            WorldHandle worldHandle,
            const BroadPhaseAabb& bounds,
            ObjectLayer collisionLayer = ObjectLayer::Invalid);

        bool DeactivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool DeactivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        bool ResetBodySleepTimer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool InvalidateBodyContactCache(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        [[nodiscard]]
        bool GetBodyPointVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& point,
            AZ::Vector3& velocity) const;

        [[nodiscard]]
        bool GetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType& motionType) const;

        [[nodiscard]]
        bool GetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer& objectLayer) const;

        [[nodiscard]]
        bool GetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            CollisionGroupConfiguration& collisionGroup) const;

        [[nodiscard]]
        bool GetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle& shapeHandle) const;

        [[nodiscard]]
        bool GetBodyAccumulatedForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& force,
            AZ::Vector3& torque) const;

        bool ResetBodyAccumulatedForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool ResetBodyAccumulatedTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool ResetBodyMotion(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        [[nodiscard]]
        bool GetBodyBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BroadPhaseAabb& bounds) const;

        [[nodiscard]]
        bool GetBodySubmergedVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const;

        [[nodiscard]]
        bool GetBodySurfaceNormal(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        bool GetBodyMaterial(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const;

        [[nodiscard]]
        bool GetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldPosition& position) const;

        [[nodiscard]]
        bool GetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Quaternion& rotation) const;

        [[nodiscard]]
        bool GetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const;

        [[nodiscard]]
        bool GetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity) const;

        [[nodiscard]]
        bool GetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& angularVelocity) const;

        bool SetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& position,
            bool activate);

        bool SetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Quaternion& rotation,
            bool activate);

        bool SetBodyTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetBodyTransformWhenChanged(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool SetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity);

        bool SetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularVelocity);

        bool AddBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool AddBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity);

        bool SetBodyTransformAndVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool MoveBodyKinematically(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& target,
            float duration);

        bool AddForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool activate = true);

        bool AddForceAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate = true);

        bool AddTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool activate = true);

        bool AddForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate = true);

        bool ApplyBuoyancyImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BuoyancyConfiguration& configuration);

        [[nodiscard]]
        bool GetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& friction) const;

        bool SetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float friction);

        [[nodiscard]]
        bool GetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& restitution) const;

        bool SetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float restitution);

        [[nodiscard]]
        bool GetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& gravityFactor) const;

        bool SetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float gravityFactor);

        [[nodiscard]]
        bool GetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumLinearVelocity) const;

        bool SetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumLinearVelocity);

        [[nodiscard]]
        bool GetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumAngularVelocity) const;

        bool SetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumAngularVelocity);

        [[nodiscard]]
        bool GetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality& motionQuality) const;

        bool SetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality motionQuality);

        [[nodiscard]]
        bool IsBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sensor) const;

        bool SetBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sensor);

        [[nodiscard]]
        bool GetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& linearDamping) const;

        bool SetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float linearDamping);

        [[nodiscard]]
        bool GetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& angularDamping) const;

        bool SetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float angularDamping);

        [[nodiscard]]
        bool IsBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sleepingAllowed) const;

        bool SetBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sleepingAllowed);

        [[nodiscard]]
        bool IsBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool GetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const;

        bool SetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount);

        bool UpdateBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyRuntimeConfiguration& configuration,
            bool activate);

        [[nodiscard]]
        bool GetBodyInverseInertia(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Matrix3x3& inverseInertia) const;

        [[nodiscard]]
        bool GetBodyInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& inverseMass) const;

        bool AddImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse);

        bool AddImpulseAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const WorldPosition& position);

        bool AddAngularImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularImpulse);

        bool SetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle shapeHandle,
            bool updateMassProperties,
            bool activate);

        bool SetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType motionType,
            bool activate);

        bool SetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer objectLayer);

        bool SetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const CollisionGroupConfiguration& collisionGroup,
            bool activate);

        [[nodiscard]]
        bool RaycastShapeClosest(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            ShapeRaycastHit& hit) const;

        [[nodiscard]]
        QueryResult RaycastShapeAll(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            AZStd::span<ShapeRaycastHit> hits) const;

        [[nodiscard]]
        QueryResult CollideShapePoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter,
            AZStd::span<ShapePointHit> hits) const;

        [[nodiscard]]
        bool CollideShapePointAny(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter = nullptr) const;

        [[nodiscard]]
        QueryResult CollectShapeTriangles(
            WorldHandle worldHandle,
            const ShapeTriangleCollectionRequest& request,
            AZStd::span<ShapeTriangle> triangles) const;

        [[nodiscard]]
        bool RaycastTransformedShapeClosest(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            RaycastHit& hit) const;

        [[nodiscard]]
        QueryResult RaycastTransformedShapeAll(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZStd::span<RaycastHit> hits) const;

        [[nodiscard]]
        QueryResult CollideTransformedShapePoint(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter,
            AZStd::span<OverlapHit> hits) const;

        [[nodiscard]]
        bool CollideTransformedShapePointAny(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter = nullptr) const;

        [[nodiscard]]
        QueryResult CollectTransformedShapeChildren(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            const IQueryFilter* filter,
            AZStd::span<TransformedShape> children) const;

        [[nodiscard]]
        QueryResult CollectTransformedShapeTriangles(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZStd::span<TransformedTriangle> triangles) const;

        [[nodiscard]]
        bool GetTransformedShapeSurfaceNormal(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        QueryResult GetTransformedShapeSupportingFace(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<WorldPosition> vertices) const;

        [[nodiscard]]
        bool RetainShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const WorldTransform& transform,
            float uniformScale,
            TransformedShape& shape) const;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const;

        [[nodiscard]]
        bool CollideTransformedShapes(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            ITransformedShapeCollisionCollector& collector) const;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            WorldHandle worldHandle,
            const ShapePlacement& firstShape,
            const ShapePlacement& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const;

        [[nodiscard]]
        bool CastTransformedShape(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            ITransformedShapeCastCollector& collector) const;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            WorldHandle worldHandle,
            const ShapePlacement& firstShape,
            const ShapePlacement& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const;

        [[nodiscard]]
        bool RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            RaycastHit& hit) const;

        [[nodiscard]]
        BufferResult RaycastClosestBatch(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results) const;

        [[nodiscard]]
        Operation<RaycastBatchOperationResult> RaycastClosestBatchAsync(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests) const;

        [[nodiscard]]
        QueryResult RaycastClosestPerBody(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const;

        [[nodiscard]]
        bool RaycastAny(
            WorldHandle worldHandle,
            const RaycastRequest& request) const;

        [[nodiscard]]
        QueryResult RaycastAll(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const;

        [[nodiscard]]
        QueryResult OverlapPoint(
            WorldHandle worldHandle,
            const PointOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const;

        [[nodiscard]]
        bool OverlapPointAny(
            WorldHandle worldHandle,
            const PointOverlapRequest& request) const;

        [[nodiscard]]
        QueryResult CollideShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<ShapeOverlapHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult OverlapShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const;

        [[nodiscard]]
        bool OverlapShapeAny(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request) const;

        [[nodiscard]]
        bool CastShapeClosest(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult CastShapeClosestPerBody(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult CastShapeAll(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult OverlapBroadPhase(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request,
            AZStd::span<BroadPhaseHit> hits) const;

        [[nodiscard]]
        bool OverlapBroadPhaseAny(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request) const;

        [[nodiscard]]
        bool CastBroadPhaseClosest(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            BroadPhaseCastHit& hit) const;

        [[nodiscard]]
        QueryResult CastBroadPhaseAll(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            AZStd::span<BroadPhaseCastHit> hits) const;

        [[nodiscard]]
        QueryResult CollectShapesInBounds(
            WorldHandle worldHandle,
            const ShapeCollectionRequest& request,
            AZStd::span<TransformedShape> shapes) const;

        [[nodiscard]]
        QueryResult GetSupportingFace(
            WorldHandle worldHandle,
            const SupportingFaceRequest& request,
            AZStd::span<WorldPosition> vertices) const;

        [[nodiscard]]
        QueryResult CollectTriangles(
            WorldHandle worldHandle,
            const TriangleCollectionRequest& request,
            AZStd::span<TransformedTriangle> triangles) const;

        [[nodiscard]]
        bool GetBroadPhaseBounds(
            WorldHandle worldHandle,
            BroadPhaseAabb& bounds) const;

        bool OptimizeBroadPhase(WorldHandle worldHandle);

        [[nodiscard]]
        bool WereBodiesInContact(
            WorldHandle worldHandle,
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const;

    protected:
        void Invalidate()
        {
            m_initialized = false;
        }

    private:
        friend class World;

        struct MaterialSlot final
        {
            JPH::RefConst<NativeMaterial> m_material;
            AZ::u32 m_generation = 0;
            AZ::u32 m_referenceCount = 0;
        };

        struct ExtensionSlot final
        {
            ExtensionHostLease m_hostLease;
            void* m_extension = nullptr;
            AZ::TypeId m_id = AZ::TypeId::CreateNull();
            AZ::u64 m_version = 0;
            AZ::u32 m_dependentCount = 0;
            AZ::u32 m_generation = 0;
            ExtensionKind m_kind = ExtensionKind::None;
        };

        struct CookedShapeSlot final
        {
            JPH::RefConst<JPH::Shape> m_shape;
            AZStd::vector<MaterialHandle> m_materialHandles;
            AZStd::vector<CookedShapeHandle> m_childHandles;
            AZStd::vector<CustomShapeDependency> m_customDependencies;
            AZ::TypeId m_customProviderId = AZ::TypeId::CreateNull();
            ExtensionHandle m_customProviderExtension;
            AZ::u32 m_generation = 0;
            AZ::u32 m_referenceCount = 0;
            AZ::u32 m_parentCount = 0;
        };

        struct GroupFilterSlot final
        {
            JPH::Ref<JPH::GroupFilter> m_filter;
            AZ::u64 m_stateHash = 0;
            ExtensionHandle m_extensionHandle;
            AZ::u32 m_generation = 0;
            AZ::u32 m_referenceCount = 0;
            AZ::u32 m_subGroupCount = 0;
            bool m_isCustom = false;
        };

        struct WorldSlot final
        {
            AZStd::unique_ptr<World> m_world;
            AZ::u32 m_generation = 0;
        };

        struct PathSlot final
        {
            JPH::RefConst<JPH::PathConstraintPath> m_path;
            AZ::TypeId m_customProviderId = AZ::TypeId::CreateNull();
            ExtensionHandle m_customProviderExtension;
            AZ::u64 m_customProviderVersion = 0;
            AZ::u64 m_sourceHash = 0;
            AZ::u32 m_generation = 0;
            AZ::u32 m_constraintCount = 0;
        };

        struct SoftBodyDefinitionSlot final
        {
            JPH::RefConst<JPH::SoftBodySharedSettings> m_settings;
            AZStd::vector<MaterialHandle> m_materialHandles;
            AZ::u32 m_generation = 0;
            AZ::u32 m_bodyCount = 0;
        };

        struct HairDefinitionSlot final
        {
            JPH::RefConst<JPH::HairSettings> m_settings;
            AZ::u32 m_generation = 0;
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

            AZ::u32 m_generation = 0;
            AZ::u32 m_instanceCount = 0;
        };

        struct SkeletonDefinitionSlot final
        {
            JPH::Ref<JPH::Skeleton> m_skeleton;
            AZStd::unordered_map<AZ::Name, AZ::u32> m_jointIndices;
            AZStd::vector<SkeletonJoint> m_joints;
            AZ::u32 m_generation = 0;
            AZ::u32 m_mapperCount = 0;
            AZ::u32 m_poseCount = 0;
            AZ::u32 m_ragdollDefinitionCount = 0;
        };

        struct SkeletalAnimationSlot final
        {
            JPH::Ref<JPH::SkeletalAnimation> m_animation;
            AZStd::vector<AZ::Name> m_jointNames;
            AZ::u64 m_revision = 1;
            AZ::u32 m_generation = 0;
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
            AZ::u32 m_generation = 0;
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
            AZ::u32 m_generation = 0;
            AZ::u32 m_sourceJointCount = 0;
            AZ::u32 m_targetJointCount = 0;
        };

        template<typename HandleType, typename SlotType>
        [[nodiscard]]
        HandleType ReserveResourceSlot(
            AZStd::vector<SlotType>& slots,
            AZStd::vector<AZ::u32>& freeSlots,
            Internal::HandleSlotReservation& reservation)
        {
            reservation = Internal::ReserveHandleSlot<HandleType>(
                slots,
                freeSlots,
                AZStd::numeric_limits<AZ::u32>::max() - 1);
            if (!reservation)
            {
                return {};
            }

            const AZ::u32 generation = slots[reservation.m_index].m_generation;
            return Internal::MakeResourceHandle<HandleType>(reservation.m_index, generation);
        }

        template<typename HandleType, typename SlotType>
        [[nodiscard]]
        HandleType ReserveResourceSlot(
            AZStd::vector<SlotType>& slots,
            AZStd::vector<AZ::u32>& freeSlots)
        {
            Internal::HandleSlotReservation reservation;
            return ReserveResourceSlot<HandleType>(slots, freeSlots, reservation);
        }

        [[nodiscard]]
        World* FindWorldUnlocked(WorldHandle worldHandle);

        [[nodiscard]]
        const World* FindWorldUnlocked(WorldHandle worldHandle) const;

        [[nodiscard]]
        SimulationResult StepAutoSimulatedWorldsDetailedInternal(
            float elapsedTime,
            AZStd::span<WorldEventBatch> eventBatches,
            AZ::u32* eventBatchCount);

        [[nodiscard]]
        bool AcquireMaterials(
            AZStd::span<const MaterialHandle> materialHandles,
            JPH::PhysicsMaterialList& materials);

        void ReleaseMaterials(AZStd::span<const MaterialHandle> materialHandles);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtensionEntry(
            void* extension,
            AZ::TypeId id,
            AZ::u64 version,
            ExtensionKind kind,
            bool uniqueIdentity,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        void* AcquireExtension(
            ExtensionHandle extensionHandle,
            ExtensionKind kind);

        [[nodiscard]]
        void* AcquireExtension(
            ExtensionKind kind,
            AZ::TypeId id,
            AZ::u64 requiredVersion,
            ExtensionHandle& extensionHandle,
            AZ::u64* registeredVersion = nullptr);

        [[nodiscard]]
        ICustomConstraintProvider* AcquireCustomConstraintProvider(
            AZ::TypeId providerId,
            AZStd::span<const AZ::u8> data,
            AZ::u32& maximumRowCount,
            AZ::u32& stateByteCount,
            AZ::u64& providerVersion,
            ExtensionHandle& extensionHandle);

        void ReleaseCustomConstraintProvider(ExtensionHandle extensionHandle);

        [[nodiscard]]
        ICustomPathProvider* AcquireCustomPathProvider(
            AZ::TypeId providerId,
            AZStd::span<const AZ::u8> data,
            float& maximumFraction,
            AZ::u64& providerVersion,
            ExtensionHandle& extensionHandle);

        void ReleaseCustomPathProvider(ExtensionHandle extensionHandle);

        [[nodiscard]]
        ICustomShapeProvider* AcquireCustomShapeProvider(
            AZ::TypeId providerId,
            AZ::u64 requiredVersion,
            ExtensionHandle& extensionHandle,
            AZ::u64* registeredVersion = nullptr);

        void ReleaseCustomShapeProvider(ExtensionHandle extensionHandle);

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
            ExtensionHandle customProviderExtension = ExtensionHandle::Invalid,
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
        bool PrepareGroupFilterParticipantRestore(
            GroupFilterHandle filterHandle,
            AZStd::span<const AZ::u8> state,
            AZ::TypeId typeId,
            AZ::u64 stateHash,
            AZ::u32 version) const;

        [[nodiscard]]
        bool CommitGroupFilterParticipantRestore(
            GroupFilterHandle filterHandle,
            AZStd::span<const AZ::u8> state,
            AZ::u64 stateHash);

        [[nodiscard]]
        GroupFilterHandle StoreGroupFilter(
            JPH::Ref<JPH::GroupFilter> filter,
            AZ::u32 subGroupCount,
            AZ::u64 stateHash,
            bool isCustom,
            ExtensionHandle extensionHandle = ExtensionHandle::Invalid);

        void RefreshGroupFilterInWorlds(GroupFilterHandle filterHandle);

        [[nodiscard]]
        GroupFilterSlot* FindGroupFilterUnlocked(GroupFilterHandle filterHandle);

        [[nodiscard]]
        const GroupFilterSlot* FindGroupFilterUnlocked(GroupFilterHandle filterHandle) const;

        [[nodiscard]]
        PathHandle StorePath(
            JPH::RefConst<JPH::PathConstraintPath> path,
            AZ::TypeId customProviderId = AZ::TypeId::CreateNull(),
            ExtensionHandle customProviderExtension = ExtensionHandle::Invalid,
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
        DebugRenderer* m_debugRenderer = nullptr;
        AZStd::atomic<AZ::u32> m_debugCaptureWorldCount = 0;
        AZStd::unique_ptr<ComponentDependencyManager> m_dependencyManager;
        SystemConfiguration m_configuration;
        AZ::JobContext* m_jobContext = nullptr;
        Internal::OperationPool* m_operationPool = nullptr;

        mutable AZStd::shared_mutex m_materialMutex;
        AZStd::vector<MaterialSlot> m_materialSlots;
        AZStd::vector<AZ::u32> m_freeMaterialSlots;

        mutable AZStd::mutex m_extensionMutex;
        AZStd::vector<ExtensionSlot> m_extensionSlots;
        AZStd::vector<AZ::u32> m_freeExtensionSlots;

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

        mutable AZStd::recursive_mutex m_sceneResourceMutex;
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
    };

#if defined(AZ_COMPILER_MSVC) || (defined(AZ_COMPILER_CLANG) && defined(AZ_PLATFORM_WINDOWS))
#define JOLT_RUNTIME_EMPTY_BASES __declspec(empty_bases)
#else
#define JOLT_RUNTIME_EMPTY_BASES
#endif

    class JOLT_API JOLT_RUNTIME_EMPTY_BASES Runtime final
        : public RuntimeImplementation
        , public RuntimeConfiguration
        , public Extensions
        , public Materials
        , public CollisionFilters
        , public Cooking
        , public Paths
        , public Skeletons
        , public Scenes
        , public Worlds
        , public WorldSimulation
        , public WorldQueries
        , public Shapes
        , public Bodies
        , public Constraints
        , public Characters
        , public Vehicles
        , public Ragdolls
        , public SoftBodies
        , public Hair
        , public Rollback
        , public Diagnostics
    {
    public:
        Runtime(
            SystemConfiguration configuration,
            AZ::JobContext* jobContext,
            SystemRegistration registration = SystemRegistration::Global);
        ~Runtime();

        AZ_DISABLE_COPY_MOVE(Runtime);

        using RuntimeImplementation::GetConfiguration;
        using RuntimeImplementation::GetRuntimeInfo;

        using RuntimeImplementation::RegisterExtension;
        using RuntimeImplementation::UnregisterExtension;
        using RuntimeImplementation::GetExtensionInformation;

        using RuntimeImplementation::CreateMaterial;
        using RuntimeImplementation::DestroyMaterial;
        using RuntimeImplementation::IsValid;

        using RuntimeImplementation::CreateGroupFilter;
        using RuntimeImplementation::CreateGroupFilterTable;
        using RuntimeImplementation::DestroyGroupFilter;
        using RuntimeImplementation::NotifyGroupFilterChanged;
        using RuntimeImplementation::GetSubGroupCollisionEnabled;
        using RuntimeImplementation::SetSubGroupCollisionEnabled;

        using RuntimeImplementation::CookShape;
        using RuntimeImplementation::CookShapeAsync;
        using RuntimeImplementation::ExportShape;
        using RuntimeImplementation::ImportShape;
        using RuntimeImplementation::DestroyCookedShape;
        using RuntimeImplementation::GetStats;
        using RuntimeImplementation::GetStatsRecursive;
        using RuntimeImplementation::GetProperties;
        using RuntimeImplementation::GetUserData;
        using RuntimeImplementation::GetCustomConvexShapeInfo;
        using RuntimeImplementation::GetCustomShapeInfo;
        using RuntimeImplementation::GetCustomShapeDependencies;
        using RuntimeImplementation::GetSubShapeUserData;
        using RuntimeImplementation::GetDirectChildShape;
        using RuntimeImplementation::GetMeshMaterials;
        using RuntimeImplementation::GetMeshTriangleMaterialIndex;
        using RuntimeImplementation::GetMeshTriangleUserData;
        using RuntimeImplementation::GetCompoundChildCount;
        using RuntimeImplementation::GetCompoundChild;
        using RuntimeImplementation::GetCompoundChildIndex;
        using RuntimeImplementation::Raycast;

        using RuntimeImplementation::CreatePath;
        using RuntimeImplementation::DestroyPath;
        using RuntimeImplementation::GetPathState;
        using RuntimeImplementation::GetCustomPathInfo;
        using RuntimeImplementation::SamplePath;
        using RuntimeImplementation::FindClosestPathPoint;

        using RuntimeImplementation::CreateSkeletonDefinition;
        using RuntimeImplementation::ExportSkeletonDefinition;
        using RuntimeImplementation::ImportSkeletonDefinition;
        using RuntimeImplementation::DestroySkeletonDefinition;
        using RuntimeImplementation::GetSkeletonJoints;
        using RuntimeImplementation::FindSkeletonJoint;
        using RuntimeImplementation::CreateSkeletalAnimation;
        using RuntimeImplementation::ExportSkeletalAnimation;
        using RuntimeImplementation::ImportSkeletalAnimation;
        using RuntimeImplementation::UpdateSkeletalAnimation;
        using RuntimeImplementation::DestroySkeletalAnimation;
        using RuntimeImplementation::GetSkeletalAnimationState;
        using RuntimeImplementation::GetSkeletalAnimatedJointName;
        using RuntimeImplementation::GetSkeletalAnimationKeyframes;
        using RuntimeImplementation::SetSkeletalAnimationLooping;
        using RuntimeImplementation::ScaleSkeletalAnimation;
        using RuntimeImplementation::CreateSkeletonPose;
        using RuntimeImplementation::DestroySkeletonPose;
        using RuntimeImplementation::GetSkeletonPoseState;
        using RuntimeImplementation::SetSkeletonPoseRootOffset;
        using RuntimeImplementation::SetSkeletonPoseLocalTransforms;
        using RuntimeImplementation::SetSkeletonPoseModelTransforms;
        using RuntimeImplementation::GetSkeletonPoseLocalTransforms;
        using RuntimeImplementation::GetSkeletonPoseModelTransforms;
        using RuntimeImplementation::SampleSkeletalAnimation;
        using RuntimeImplementation::CreateSkeletonMapper;
        using RuntimeImplementation::DestroySkeletonMapper;
        using RuntimeImplementation::GetSkeletonMapperState;
        using RuntimeImplementation::GetSkeletonMapperMappings;
        using RuntimeImplementation::GetSkeletonMapperChainState;
        using RuntimeImplementation::GetSkeletonMapperSourceChain;
        using RuntimeImplementation::GetSkeletonMapperTargetChain;
        using RuntimeImplementation::GetSkeletonMapperUnmappedJoints;
        using RuntimeImplementation::GetSkeletonMapperLockedTranslations;
        using RuntimeImplementation::GetMappedSkeletonJoint;
        using RuntimeImplementation::IsSkeletonJointTranslationLocked;
        using RuntimeImplementation::MapSkeletonPose;
        using RuntimeImplementation::MapSkeletonPoseReverse;

        using RuntimeImplementation::CreateSceneDefinition;
        using RuntimeImplementation::BuildSceneAsset;
        using RuntimeImplementation::DestroySceneDefinition;
        using RuntimeImplementation::GetSceneDefinitionState;
        using RuntimeImplementation::InstantiateScene;
        using RuntimeImplementation::InstantiateSceneAsync;
        using RuntimeImplementation::DestroySceneInstance;
        using RuntimeImplementation::GetSceneInstanceState;
        using RuntimeImplementation::GetSceneBodies;
        using RuntimeImplementation::GetSceneConstraints;

        using RuntimeImplementation::CreateWorld;
        using RuntimeImplementation::DestroyWorld;
        using RuntimeImplementation::GetDefaultWorldHandle;
        using RuntimeImplementation::GetWorldQueries;
        using RuntimeImplementation::GetWorldGravity;
        using RuntimeImplementation::SetWorldGravity;
        using RuntimeImplementation::GetSimulationConfiguration;
        using RuntimeImplementation::UpdateSimulationConfiguration;
        using RuntimeImplementation::GetWorldRuntimeConfiguration;
        using RuntimeImplementation::UpdateWorldRuntimeConfiguration;

        using RuntimeImplementation::StepWorld;
        using RuntimeImplementation::StepWorldDetailed;
        using RuntimeImplementation::StepWorldAsync;
        using RuntimeImplementation::StepAutoSimulatedWorlds;
        using RuntimeImplementation::StepAutoSimulatedWorldsDetailed;
        using RuntimeImplementation::StepAutoSimulatedWorldsAsync;
        using RuntimeImplementation::GetEvents;
        using RuntimeImplementation::SetContactCallbacks;
        using RuntimeImplementation::SetBodyPairCollider;
        using RuntimeImplementation::SetSimulationShapeFilter;
        using RuntimeImplementation::SetSoftBodyContactCallbacks;
        using RuntimeImplementation::AddStepListener;
        using RuntimeImplementation::RemoveStepListener;

        using RuntimeImplementation::RaycastShapeClosest;
        using RuntimeImplementation::RaycastShapeAll;
        using RuntimeImplementation::CollideShapePoint;
        using RuntimeImplementation::CollideShapePointAny;
        using RuntimeImplementation::CollectShapeTriangles;
        using RuntimeImplementation::RaycastTransformedShapeClosest;
        using RuntimeImplementation::RaycastTransformedShapeAll;
        using RuntimeImplementation::CollideTransformedShapePoint;
        using RuntimeImplementation::CollideTransformedShapePointAny;
        using RuntimeImplementation::CollectTransformedShapeChildren;
        using RuntimeImplementation::CollectTransformedShapeTriangles;
        using RuntimeImplementation::GetTransformedShapeSurfaceNormal;
        using RuntimeImplementation::GetTransformedShapeSupportingFace;
        using RuntimeImplementation::RetainShape;
        using RuntimeImplementation::CollideTransformedShapes;
        using RuntimeImplementation::CastTransformedShape;
        using RuntimeImplementation::RaycastClosest;
        using RuntimeImplementation::RaycastClosestBatch;
        using RuntimeImplementation::RaycastClosestBatchAsync;
        using RuntimeImplementation::RaycastClosestPerBody;
        using RuntimeImplementation::RaycastAny;
        using RuntimeImplementation::RaycastAll;
        using RuntimeImplementation::OverlapPoint;
        using RuntimeImplementation::OverlapPointAny;
        using RuntimeImplementation::CollideShape;
        using RuntimeImplementation::OverlapShape;
        using RuntimeImplementation::OverlapShapeAny;
        using RuntimeImplementation::CastShapeClosest;
        using RuntimeImplementation::CastShapeClosestPerBody;
        using RuntimeImplementation::CastShapeAll;
        using RuntimeImplementation::OverlapBroadPhase;
        using RuntimeImplementation::OverlapBroadPhaseAny;
        using RuntimeImplementation::CastBroadPhaseClosest;
        using RuntimeImplementation::CastBroadPhaseAll;
        using RuntimeImplementation::CollectShapesInBounds;
        using RuntimeImplementation::GetSupportingFace;
        using RuntimeImplementation::CollectTriangles;
        using RuntimeImplementation::GetBroadPhaseBounds;
        using RuntimeImplementation::OptimizeBroadPhase;
        using RuntimeImplementation::WereBodiesInContact;

        using RuntimeImplementation::CreateShape;
        using RuntimeImplementation::CloneShape;
        using RuntimeImplementation::ScaleShape;
        using RuntimeImplementation::DestroyShape;
        using RuntimeImplementation::GetShapeStats;
        using RuntimeImplementation::GetShapeStatsRecursive;
        using RuntimeImplementation::GetShapeProperties;
        using RuntimeImplementation::GetShapeSubmergedVolume;
        using RuntimeImplementation::GetPrimitiveShapeState;
        using RuntimeImplementation::GetConvexHullState;
        using RuntimeImplementation::GetConvexHullPointsRelativeToCenterOfMass;
        using RuntimeImplementation::GetConvexHullPlanesRelativeToCenterOfMass;
        using RuntimeImplementation::GetConvexHullFaceVertexIndices;
        using RuntimeImplementation::GetShapeMaterial;
        using RuntimeImplementation::GetShapeSurfaceNormal;
        using RuntimeImplementation::GetShapeUserData;
        using RuntimeImplementation::GetShapeSubShapeUserData;
        using RuntimeImplementation::GetDecoratedShapeConfiguration;
        using RuntimeImplementation::IsShapeScaleValid;
        using RuntimeImplementation::MakeShapeScaleValid;
        using RuntimeImplementation::GetHeightfieldState;
        using RuntimeImplementation::GetHeightfieldPosition;
        using RuntimeImplementation::ProjectOntoHeightfield;
        using RuntimeImplementation::IsHeightfieldNoCollision;
        using RuntimeImplementation::GetHeightfieldHeights;
        using RuntimeImplementation::GetHeightfieldMaterialIndices;
        using RuntimeImplementation::GetHeightfieldMaterials;
        using RuntimeImplementation::GetHeightfieldSubShapeCoordinates;
        using RuntimeImplementation::UpdateHeightfieldHeights;
        using RuntimeImplementation::UpdateHeightfieldMaterials;
        using RuntimeImplementation::AddMutableCompoundChild;
        using RuntimeImplementation::RemoveMutableCompoundChild;
        using RuntimeImplementation::UpdateMutableCompoundChild;
        using RuntimeImplementation::UpdateMutableCompoundChildTransforms;
        using RuntimeImplementation::AdjustMutableCompoundCenterOfMass;
        using RuntimeImplementation::CreateBody;

        using RuntimeImplementation::CreateBodyWithId;
        using RuntimeImplementation::CreateSoftBody;
        using RuntimeImplementation::AddBodiesToSimulation;
        using RuntimeImplementation::RemoveBodyFromSimulation;
        using RuntimeImplementation::RemoveBodiesFromSimulation;
        using RuntimeImplementation::DestroyBody;
        using RuntimeImplementation::DestroyBodies;
        using RuntimeImplementation::IsBodyInSimulation;
        using RuntimeImplementation::SetBodyMoveEventsEnabled;
        using RuntimeImplementation::CreateRagdollDefinition;
        using RuntimeImplementation::DestroyRagdollDefinition;
        using RuntimeImplementation::GetRagdollBodyConstraintIndices;
        using RuntimeImplementation::GetRagdollConstraintBodyPairs;
        using RuntimeImplementation::UpdateDistanceLimits;
        using RuntimeImplementation::UpdateHingeLimits;
        using RuntimeImplementation::UpdateHingeMotor;
        using RuntimeImplementation::SetHingeTargetOrientation;
        using RuntimeImplementation::UpdatePathMotor;
        using RuntimeImplementation::UpdatePathProperties;
        using RuntimeImplementation::UpdatePointAnchors;
        using RuntimeImplementation::UpdatePulleyLimits;
        using RuntimeImplementation::UpdateSixDofLimits;
        using RuntimeImplementation::UpdateSixDofMotors;
        using RuntimeImplementation::UpdateSliderMotor;
        using RuntimeImplementation::UpdateSliderLimits;
        using RuntimeImplementation::UpdateSwingTwistMotors;
        using RuntimeImplementation::UpdateSwingTwistLimits;
        using RuntimeImplementation::GetBodyState;
        using RuntimeImplementation::GetBodyCenterOfMassTransform;
        using RuntimeImplementation::GetBodyConfiguration;
        using RuntimeImplementation::GetBodies;
        using RuntimeImplementation::GetBodyId;
        using RuntimeImplementation::ActivateBody;
        using RuntimeImplementation::ActivateBodies;
        using RuntimeImplementation::ActivateBodiesInBounds;
        using RuntimeImplementation::DeactivateBody;
        using RuntimeImplementation::DeactivateBodies;
        using RuntimeImplementation::ResetBodySleepTimer;
        using RuntimeImplementation::InvalidateBodyContactCache;
        using RuntimeImplementation::GetBodyPointVelocity;
        using RuntimeImplementation::GetBodyMotionType;
        using RuntimeImplementation::GetBodyObjectLayer;
        using RuntimeImplementation::GetBodyCollisionGroup;
        using RuntimeImplementation::GetBodyShape;
        using RuntimeImplementation::GetBodyAccumulatedForceAndTorque;
        using RuntimeImplementation::ResetBodyAccumulatedForce;
        using RuntimeImplementation::ResetBodyAccumulatedTorque;
        using RuntimeImplementation::ResetBodyMotion;
        using RuntimeImplementation::GetBodyBounds;
        using RuntimeImplementation::GetBodySubmergedVolume;
        using RuntimeImplementation::GetBodySurfaceNormal;
        using RuntimeImplementation::GetBodyMaterial;
        using RuntimeImplementation::GetBodyPosition;
        using RuntimeImplementation::GetBodyRotation;
        using RuntimeImplementation::GetBodyVelocities;
        using RuntimeImplementation::GetBodyLinearVelocity;
        using RuntimeImplementation::GetBodyAngularVelocity;
        using RuntimeImplementation::SetBodyPosition;
        using RuntimeImplementation::SetBodyRotation;
        using RuntimeImplementation::SetBodyTransform;
        using RuntimeImplementation::SetBodyTransformWhenChanged;
        using RuntimeImplementation::SetBodyVelocities;
        using RuntimeImplementation::SetBodyLinearVelocity;
        using RuntimeImplementation::SetBodyAngularVelocity;
        using RuntimeImplementation::AddBodyVelocities;
        using RuntimeImplementation::AddBodyLinearVelocity;
        using RuntimeImplementation::SetBodyTransformAndVelocities;
        using RuntimeImplementation::MoveBodyKinematically;
        using RuntimeImplementation::AddForce;
        using RuntimeImplementation::AddForceAtPosition;
        using RuntimeImplementation::AddTorque;
        using RuntimeImplementation::AddForceAndTorque;
        using RuntimeImplementation::ApplyBuoyancyImpulse;
        using RuntimeImplementation::GetBodyFriction;
        using RuntimeImplementation::SetBodyFriction;
        using RuntimeImplementation::GetBodyRestitution;
        using RuntimeImplementation::SetBodyRestitution;
        using RuntimeImplementation::GetBodyGravityFactor;
        using RuntimeImplementation::SetBodyGravityFactor;
        using RuntimeImplementation::GetBodyMaximumLinearVelocity;
        using RuntimeImplementation::SetBodyMaximumLinearVelocity;
        using RuntimeImplementation::GetBodyMaximumAngularVelocity;
        using RuntimeImplementation::SetBodyMaximumAngularVelocity;
        using RuntimeImplementation::GetBodyMotionQuality;
        using RuntimeImplementation::SetBodyMotionQuality;
        using RuntimeImplementation::IsBodyManifoldReductionEnabled;
        using RuntimeImplementation::SetBodyManifoldReductionEnabled;
        using RuntimeImplementation::IsBodySensor;
        using RuntimeImplementation::SetBodySensor;
        using RuntimeImplementation::GetBodyLinearDamping;
        using RuntimeImplementation::SetBodyLinearDamping;
        using RuntimeImplementation::GetBodyAngularDamping;
        using RuntimeImplementation::SetBodyAngularDamping;
        using RuntimeImplementation::IsBodySleepingAllowed;
        using RuntimeImplementation::SetBodySleepingAllowed;
        using RuntimeImplementation::IsBodyGyroscopicForceEnabled;
        using RuntimeImplementation::SetBodyGyroscopicForceEnabled;
        using RuntimeImplementation::IsBodyKinematicVsNonDynamicCollisionEnabled;
        using RuntimeImplementation::SetBodyKinematicVsNonDynamicCollisionEnabled;
        using RuntimeImplementation::IsBodyEnhancedInternalEdgeRemovalEnabled;
        using RuntimeImplementation::SetBodyEnhancedInternalEdgeRemovalEnabled;
        using RuntimeImplementation::GetBodySolverStepCounts;
        using RuntimeImplementation::SetBodySolverStepCounts;
        using RuntimeImplementation::UpdateBodyRuntimeConfiguration;
        using RuntimeImplementation::GetBodyInverseInertia;
        using RuntimeImplementation::GetBodyInverseMass;
        using RuntimeImplementation::AddImpulse;
        using RuntimeImplementation::AddImpulseAtPosition;
        using RuntimeImplementation::AddAngularImpulse;
        using RuntimeImplementation::SetBodyShape;
        using RuntimeImplementation::SetBodyMotionType;
        using RuntimeImplementation::SetBodyObjectLayer;
        using RuntimeImplementation::SetBodyCollisionGroup;

        using RuntimeImplementation::CreateConstraint;
        using RuntimeImplementation::AddConstraintToSimulation;
        using RuntimeImplementation::AddConstraintsToSimulation;
        using RuntimeImplementation::RemoveConstraintFromSimulation;
        using RuntimeImplementation::RemoveConstraintsFromSimulation;
        using RuntimeImplementation::DestroyConstraint;
        using RuntimeImplementation::DestroyConstraints;
        using RuntimeImplementation::IsConstraintInSimulation;
        using RuntimeImplementation::SetConstraintEnabled;
        using RuntimeImplementation::GetConstraintState;
        using RuntimeImplementation::GetConstraintConfiguration;
        using RuntimeImplementation::GetConstraintUserData;
        using RuntimeImplementation::SetConstraintUserData;
        using RuntimeImplementation::GetConstraintDebugDrawSize;
        using RuntimeImplementation::SetConstraintDebugDrawSize;
        using RuntimeImplementation::GetConstraintMeasurements;
        using RuntimeImplementation::GetCustomConstraintInfo;
        using RuntimeImplementation::GetCustomConstraintImpulses;
        using RuntimeImplementation::GetCustomConstraintState;
        using RuntimeImplementation::SetCustomConstraintState;
        using RuntimeImplementation::ResetConstraintWarmStart;
        using RuntimeImplementation::UpdateConstraintSolverConfiguration;
        using RuntimeImplementation::UpdateConeLimit;

        using RuntimeImplementation::CreateVirtualCharacter;
        using RuntimeImplementation::DestroyVirtualCharacter;
        using RuntimeImplementation::GetVirtualCharacterState;
        using RuntimeImplementation::GetVirtualCharacterUserData;
        using RuntimeImplementation::SetVirtualCharacterUserData;
        using RuntimeImplementation::GetVirtualCharacterRuntimeConfiguration;
        using RuntimeImplementation::CheckVirtualCharacterCollision;
        using RuntimeImplementation::UpdateVirtualCharacterRuntimeConfiguration;
        using RuntimeImplementation::SetVirtualCharacterShape;
        using RuntimeImplementation::SetVirtualCharacterInnerBodyShape;
        using RuntimeImplementation::SetVirtualCharacterTransform;
        using RuntimeImplementation::SetVirtualCharacterVelocity;
        using RuntimeImplementation::CancelVirtualCharacterVelocityTowardsSteepSlopes;
        using RuntimeImplementation::BeginVirtualCharacterContactTracking;
        using RuntimeImplementation::EndVirtualCharacterContactTracking;
        using RuntimeImplementation::SetVirtualCharacterContactCallbacks;
        using RuntimeImplementation::CanVirtualCharacterWalkStairs;
        using RuntimeImplementation::WalkVirtualCharacterStairs;
        using RuntimeImplementation::StickVirtualCharacterToFloor;
        using RuntimeImplementation::RefreshVirtualCharacterContacts;
        using RuntimeImplementation::UpdateVirtualCharacterGroundVelocity;
        using RuntimeImplementation::GetVirtualCharacterContacts;
        using RuntimeImplementation::HasVirtualCharacterCollidedWith;
        using RuntimeImplementation::HaveVirtualCharactersCollided;
        using RuntimeImplementation::UpdateVirtualCharacter;
        using RuntimeImplementation::EnableVirtualCharacterAutoUpdate;
        using RuntimeImplementation::DisableVirtualCharacterAutoUpdate;
        using RuntimeImplementation::CreateCharacter;
        using RuntimeImplementation::DestroyCharacter;
        using RuntimeImplementation::GetCharacterState;
        using RuntimeImplementation::GetCharacterUserData;
        using RuntimeImplementation::SetCharacterUserData;
        using RuntimeImplementation::GetCharacterRuntimeConfiguration;
        using RuntimeImplementation::CheckCharacterCollision;
        using RuntimeImplementation::UpdateCharacterRuntimeConfiguration;
        using RuntimeImplementation::SetCharacterShape;
        using RuntimeImplementation::SetCharacterTransform;
        using RuntimeImplementation::SetCharacterVelocity;
        using RuntimeImplementation::AddCharacterImpulse;

        using RuntimeImplementation::ApplyVehicleEngineDamping;
        using RuntimeImplementation::ApplyVehicleEngineTorque;
        using RuntimeImplementation::CalculateVehicleEngineTorque;
        using RuntimeImplementation::CreateWheeledVehicle;
        using RuntimeImplementation::CreateMotorcycle;
        using RuntimeImplementation::CreateTrackedVehicle;
        using RuntimeImplementation::DestroyVehicle;
        using RuntimeImplementation::GetWheeledVehicleState;
        using RuntimeImplementation::GetMotorcycleState;
        using RuntimeImplementation::GetTrackedVehicleState;
        using RuntimeImplementation::GetVehicleCollisionConfiguration;
        using RuntimeImplementation::GetVehicleDifferentialLimitedSlipRatio;
        using RuntimeImplementation::GetVehicleEngineConfiguration;
        using RuntimeImplementation::GetVehiclePowertrainState;
        using RuntimeImplementation::GetVehicleRuntimeConfiguration;
        using RuntimeImplementation::GetVehicleTransmissionConfiguration;
        using RuntimeImplementation::GetVehicleTrackConfiguration;
        using RuntimeImplementation::GetWheelLocalBasis;
        using RuntimeImplementation::GetWheelLocalTransform;
        using RuntimeImplementation::GetWheelWorldTransform;
        using RuntimeImplementation::QueryVehicleAntiRollBars;
        using RuntimeImplementation::QueryVehicleDifferentials;
        using RuntimeImplementation::SetTrackedVehicleInput;
        using RuntimeImplementation::SetVehicleCallbacks;
        using RuntimeImplementation::SetVehicleCollisionFilter;
        using RuntimeImplementation::SetVehicleDifferentialLimitedSlipRatio;
        using RuntimeImplementation::SetVehiclePowertrainControl;
        using RuntimeImplementation::SetVehicleTrackAngularVelocity;
        using RuntimeImplementation::SetWheelMotion;
        using RuntimeImplementation::SetWheeledVehicleInput;
        using RuntimeImplementation::UpdateMotorcycleController;
        using RuntimeImplementation::UpdateVehicleAntiRollBars;
        using RuntimeImplementation::UpdateVehicleCollisionConfiguration;
        using RuntimeImplementation::UpdateVehicleDifferentials;
        using RuntimeImplementation::UpdateVehicleEngineConfiguration;
        using RuntimeImplementation::UpdateVehicleRuntimeConfiguration;
        using RuntimeImplementation::UpdateVehicleTransmissionConfiguration;
        using RuntimeImplementation::UpdateVehicleTrackConfiguration;

        using RuntimeImplementation::CreateRagdoll;
        using RuntimeImplementation::AddRagdollToSimulation;
        using RuntimeImplementation::RemoveRagdollFromSimulation;
        using RuntimeImplementation::DestroyRagdoll;
        using RuntimeImplementation::IsRagdollInSimulation;
        using RuntimeImplementation::GetRagdollState;
        using RuntimeImplementation::SetRagdollCollisionGroupId;
        using RuntimeImplementation::GetRagdollBodies;
        using RuntimeImplementation::GetRagdollConstraints;
        using RuntimeImplementation::ActivateRagdoll;
        using RuntimeImplementation::SetRagdollPose;
        using RuntimeImplementation::GetRagdollPose;
        using RuntimeImplementation::DriveRagdollKinematically;
        using RuntimeImplementation::DriveRagdollMotors;
        using RuntimeImplementation::ResetRagdollWarmStart;
        using RuntimeImplementation::SetRagdollVelocity;
        using RuntimeImplementation::SetRagdollLinearVelocity;
        using RuntimeImplementation::AddRagdollLinearVelocity;
        using RuntimeImplementation::AddRagdollImpulse;

        using RuntimeImplementation::CreateSoftBodyDefinition;
        using RuntimeImplementation::ExportSoftBodyDefinition;
        using RuntimeImplementation::ImportSoftBodyDefinition;
        using RuntimeImplementation::DestroySoftBodyDefinition;
        using RuntimeImplementation::GetSoftBodyDefinitionState;
        using RuntimeImplementation::GetSoftBodyDefinitionDihedralBendConstraints;
        using RuntimeImplementation::GetSoftBodyDefinitionEdgeConstraints;
        using RuntimeImplementation::GetSoftBodyDefinitionFaces;
        using RuntimeImplementation::GetSoftBodyDefinitionInverseBinds;
        using RuntimeImplementation::GetSoftBodyDefinitionLongRangeConstraints;
        using RuntimeImplementation::GetSoftBodyDefinitionMaterials;
        using RuntimeImplementation::GetSoftBodyDefinitionRodBendTwistConstraints;
        using RuntimeImplementation::GetSoftBodyDefinitionRodStretchShearConstraints;
        using RuntimeImplementation::GetSoftBodyDefinitionSkinConstraints;
        using RuntimeImplementation::GetSoftBodyDefinitionVertices;
        using RuntimeImplementation::GetSoftBodyDefinitionVolumeConstraints;
        using RuntimeImplementation::CreateSoftBodyWithId;
        using RuntimeImplementation::AddBodyToSimulation;
        using RuntimeImplementation::GetBodyUserData;
        using RuntimeImplementation::SetBodyUserData;
        using RuntimeImplementation::GetBodyRuntimeConfiguration;
        using RuntimeImplementation::GetBodySimulationStatistics;
        using RuntimeImplementation::ApplyBodyConfiguration;
        using RuntimeImplementation::GetSoftBodyFaces;
        using RuntimeImplementation::GetSoftBodyLocalBounds;
        using RuntimeImplementation::GetSoftBodyMaterials;
        using RuntimeImplementation::GetSoftBodyRodStates;
        using RuntimeImplementation::GetSoftBodyRuntimeConfiguration;
        using RuntimeImplementation::ApplySoftBodyConfiguration;
        using RuntimeImplementation::GetSoftBodyVertices;
        using RuntimeImplementation::GetSoftBodyVolume;
        using RuntimeImplementation::RecalculateSoftBodyMassProperties;
        using RuntimeImplementation::SkinSoftBody;
        using RuntimeImplementation::UpdateSoftBodyManually;
        using RuntimeImplementation::UpdateSoftBodyRuntimeConfiguration;
        using RuntimeImplementation::SetSoftBodyVertexInverseMass;
        using RuntimeImplementation::SetSoftBodyVertexInverseMasses;
        using RuntimeImplementation::SetSoftBodyVertexVelocity;
        using RuntimeImplementation::SetSoftBodyVertexVelocities;

        using RuntimeImplementation::CreateHairDefinition;
        using RuntimeImplementation::DestroyHairDefinition;
        using RuntimeImplementation::GetHairDefinitionState;
        using RuntimeImplementation::GetHairNeutralDensity;
        using RuntimeImplementation::SkinHairScalpVertices;
        using RuntimeImplementation::CreateHair;
        using RuntimeImplementation::DestroyHair;
        using RuntimeImplementation::SetHairTransform;
        using RuntimeImplementation::SetHairScalpToHeadTransform;
        using RuntimeImplementation::UpdateHair;
        using RuntimeImplementation::EnableHairAutoUpdate;
        using RuntimeImplementation::DisableHairAutoUpdate;
        using RuntimeImplementation::GetHairState;
        using RuntimeImplementation::GetHairReadback;
        using RuntimeImplementation::GetHairVertexStates;
        using RuntimeImplementation::GetHairRenderPositions;
        using RuntimeImplementation::GetHairScalpPositions;
        using RuntimeImplementation::GetHairGridCellStates;

        using RuntimeImplementation::CaptureBodyState;
        using RuntimeImplementation::CaptureBodyStateAsync;
        using RuntimeImplementation::RestoreBodyState;
        using RuntimeImplementation::RestoreBodyStateAsync;
        using RuntimeImplementation::CaptureWorldState;
        using RuntimeImplementation::CaptureWorldStateAsync;
        using RuntimeImplementation::CaptureWorldStateParts;
        using RuntimeImplementation::ExportWorldStateArchive;
        using RuntimeImplementation::ImportWorldStateArchive;
        using RuntimeImplementation::DestroyStateSnapshot;
        using RuntimeImplementation::RestoreWorldState;
        using RuntimeImplementation::RestoreWorldStateAsync;
        using RuntimeImplementation::RestoreWorldStateParts;
        using RuntimeImplementation::ValidateWorldState;
        using RuntimeImplementation::GetWorldStateDigest;

        using RuntimeImplementation::GetWorldStatistics;
        using RuntimeImplementation::ConfigurePerformanceStatistics;
        using RuntimeImplementation::GetPerformanceStatistics;
        using RuntimeImplementation::GetBroadPhaseStatistics;
        using RuntimeImplementation::GetNarrowPhaseStatistics;
        using RuntimeImplementation::DrawDebug;
        using RuntimeImplementation::ConfigureDebugCapture;
        using RuntimeImplementation::GetDebugCaptureStatistics;

    private:
        [[nodiscard]]
        bool PublishCapabilities();

        void UnpublishCapabilities();

        bool m_registered = false;
    };

#undef JOLT_RUNTIME_EMPTY_BASES

    [[nodiscard]]
    Runtime* GetRuntime();
} // namespace Jolt
