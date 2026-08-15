/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyConfiguration.h>
#include <Jolt/BodyCollision.h>
#include <Jolt/Character.h>
#include <Jolt/Constraint.h>
#include <Jolt/Cooking.h>
#include <Jolt/Diagnostics.h>
#include <Jolt/DebugDraw.h>
#include <Jolt/Event.h>
#include <Jolt/Hair.h>
#include <Jolt/Handle.h>
#include <Jolt/Material.h>
#include <Jolt/Path.h>
#include <Jolt/Query.h>
#include <Jolt/Ragdoll.h>
#include <Jolt/Scene.h>
#include <Jolt/Simulation.h>
#include <Jolt/Shape.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/Skeleton.h>
#include <Jolt/SoftBody.h>
#include <Jolt/SystemConfiguration.h>
#include <Jolt/Vehicle.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Plane.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/std/containers/span.h>

namespace Jolt
{
    struct SceneAssetData;
    struct SceneSourceData;

    enum class Precision : AZ::u8
    {
        None = 0,
        Double,
        Single,
    };

    enum class SimdLevel : AZ::u8
    {
        None = 0,
        Avx,
        Avx2,
        Avx512,
        Neon,
        Rvv,
        Scalar,
        Sse2,
        Sse41,
        Sse42,
        WasmSimd,
    };

    enum class DeterminismCertification : AZ::u8
    {
        None = 0,
        //! Repeatable only with the same executable and deterministic application inputs.
        SameBinary,
        //! Repeatable across supported platforms when source, defines, inputs, and mutation order match.
        CrossPlatform,
    };

    struct Version final
    {
        AZ_TYPE_INFO(Version, VersionTypeId);

        AZ::u32 m_major = 0;
        AZ::u32 m_minor = 0;
        AZ::u32 m_patch = 0;

        friend constexpr bool operator==(const Version&, const Version&) = default;
    };

    struct RuntimeInfo final
    {
        AZ_TYPE_INFO(RuntimeInfo, RuntimeInfoTypeId);

        Version m_version;
        //! Compare this before exchanging snapshots or deterministic state.
        AZ::u64 m_buildFingerprint = 0;

        AZStd::string_view m_configuration;
        AZStd::string_view m_patchHash;
        AZStd::string_view m_patchRevision;
        AZStd::string_view m_sourceRevision;

        DeterminismCertification m_hairDeterminism = DeterminismCertification::None;
        DeterminismCertification m_physicsDeterminism = DeterminismCertification::None;
        Precision m_precision = Precision::None;
        SimdLevel m_simdLevel = SimdLevel::None;

        bool m_detailedProfiling = false;
        bool m_simulationStatistics = false;
    };

    class ISystem
    {
    public:
        AZ_RTTI(ISystem, ISystemTypeId);

        virtual ~ISystem() = default;

        [[nodiscard]]
        virtual const SystemConfiguration& GetConfiguration() const = 0;

        [[nodiscard]]
        virtual RuntimeInfo GetRuntimeInfo() const = 0;

        //! The caller retains ownership and must keep provider alive until unregistration succeeds.
        [[nodiscard]]
        virtual bool RegisterCustomConstraintProvider(ICustomConstraintProvider* provider) = 0;

        virtual bool UnregisterCustomConstraintProvider(ICustomConstraintProvider* provider) = 0;

        //! The caller retains ownership and must keep provider alive until unregistration succeeds.
        [[nodiscard]]
        virtual bool RegisterCustomPathProvider(ICustomPathProvider* provider) = 0;

        virtual bool UnregisterCustomPathProvider(ICustomPathProvider* provider) = 0;

        [[nodiscard]]
        virtual MaterialHandle CreateMaterial(const MaterialConfiguration& configuration) = 0;

        virtual bool DestroyMaterial(MaterialHandle materialHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(MaterialHandle materialHandle) const = 0;

        //! The caller owns filter, which must outlive the returned handle and every body that references it.
        [[nodiscard]]
        virtual GroupFilterHandle CreateGroupFilter(
            AZ::u32 subGroupCount,
            IGroupFilter* filter) = 0;

        [[nodiscard]]
        virtual GroupFilterHandle CreateGroupFilterTable(
            const GroupFilterTableConfiguration& configuration) = 0;

        virtual bool DestroyGroupFilter(GroupFilterHandle filterHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(GroupFilterHandle filterHandle) const = 0;

        //! Call after mutating a custom filter to invalidate contacts and deterministic snapshots.
        virtual bool NotifyGroupFilterChanged(GroupFilterHandle filterHandle) = 0;

        [[nodiscard]]
        virtual bool GetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool& enabled) const = 0;

        virtual bool SetSubGroupCollisionEnabled(
            GroupFilterHandle filterHandle,
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup,
            bool enabled) = 0;

        [[nodiscard]]
        virtual PathHandle CreatePath(const HermitePathConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual PathHandle CreatePath(const CustomPathConfiguration& configuration) = 0;

        virtual bool DestroyPath(PathHandle pathHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(PathHandle pathHandle) const = 0;

        [[nodiscard]]
        virtual bool GetPathState(
            PathHandle pathHandle,
            PathState& state) const = 0;

        [[nodiscard]]
        virtual bool GetCustomPathInfo(
            PathHandle pathHandle,
            CustomPathInfo& info) const = 0;

        virtual bool SamplePath(
            PathHandle pathHandle,
            float fraction,
            PathSample& sample) const = 0;

        virtual bool FindClosestPathPoint(
            PathHandle pathHandle,
            const AZ::Vector3& position,
            float fractionHint,
            PathSample& sample) const = 0;

        [[nodiscard]]
        virtual SkeletonDefinitionHandle CreateSkeletonDefinition(
            const SkeletonDefinitionConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool ExportSkeletonDefinition(
            SkeletonDefinitionHandle skeletonHandle,
            SkeletonDefinitionArchive& archive) const = 0;

        [[nodiscard]]
        virtual SkeletonDefinitionHandle ImportSkeletonDefinition(
            const SkeletonDefinitionArchive& archive) = 0;

        virtual bool DestroySkeletonDefinition(SkeletonDefinitionHandle skeletonHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(SkeletonDefinitionHandle skeletonHandle) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonJoints(
            SkeletonDefinitionHandle skeletonHandle,
            AZStd::span<SkeletonJoint> joints) const = 0;

        [[nodiscard]]
        virtual bool FindSkeletonJoint(
            SkeletonDefinitionHandle skeletonHandle,
            AZ::Name jointName,
            AZ::u32& jointIndex) const = 0;

        [[nodiscard]]
        virtual SkeletalAnimationHandle CreateSkeletalAnimation(
            const SkeletalAnimationConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool ExportSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationArchive& archive) const = 0;

        [[nodiscard]]
        virtual SkeletalAnimationHandle ImportSkeletalAnimation(
            const SkeletalAnimationArchive& archive) = 0;

        virtual bool UpdateSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            const SkeletalAnimationConfiguration& configuration) = 0;

        virtual bool DestroySkeletalAnimation(SkeletalAnimationHandle animationHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(SkeletalAnimationHandle animationHandle) const = 0;

        [[nodiscard]]
        virtual bool GetSkeletalAnimationState(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationState& state) const = 0;

        [[nodiscard]]
        virtual bool GetSkeletalAnimatedJointName(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex,
            AZ::Name& jointName) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletalAnimationKeyframes(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex,
            AZStd::span<SkeletalAnimationKeyframe> keyframes) const = 0;

        virtual bool SetSkeletalAnimationLooping(
            SkeletalAnimationHandle animationHandle,
            bool isLooping) = 0;

        virtual bool ScaleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            float scale) = 0;

        [[nodiscard]]
        virtual SkeletonPoseHandle CreateSkeletonPose(SkeletonDefinitionHandle skeletonHandle) = 0;

        virtual bool DestroySkeletonPose(SkeletonPoseHandle poseHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(SkeletonPoseHandle poseHandle) const = 0;

        [[nodiscard]]
        virtual bool GetSkeletonPoseState(
            SkeletonPoseHandle poseHandle,
            SkeletonPoseState& state) const = 0;

        virtual bool SetSkeletonPoseRootOffset(
            SkeletonPoseHandle poseHandle,
            const WorldPosition& rootOffset) = 0;

        virtual bool SetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<const AZ::Transform> localTransforms) = 0;

        virtual bool SetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<const AZ::Transform> modelTransforms) = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<AZ::Transform> localTransforms) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            AZStd::span<AZ::Transform> modelTransforms) const = 0;

        virtual bool SampleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletonPoseHandle poseHandle,
            float time) = 0;

        [[nodiscard]]
        virtual SkeletonMapperHandle CreateSkeletonMapper(
            const SkeletonMapperConfiguration& configuration) = 0;

        virtual bool DestroySkeletonMapper(SkeletonMapperHandle mapperHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(SkeletonMapperHandle mapperHandle) const = 0;

        [[nodiscard]]
        virtual bool GetSkeletonMapperState(
            SkeletonMapperHandle mapperHandle,
            SkeletonMapperState& state) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonMapperMappings(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperMappingState> mappings) const = 0;

        [[nodiscard]]
        virtual bool GetSkeletonMapperChainState(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            SkeletonMapperChainState& state) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonMapperSourceChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            AZStd::span<AZ::u32> jointIndices) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonMapperTargetChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            AZStd::span<AZ::u32> jointIndices) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonMapperUnmappedJoints(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperUnmappedJoint> joints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSkeletonMapperLockedTranslations(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<SkeletonMapperLockedTranslation> translations) const = 0;

        [[nodiscard]]
        virtual bool GetMappedSkeletonJoint(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 sourceJointIndex,
            AZ::u32& targetJointIndex) const = 0;

        [[nodiscard]]
        virtual bool IsSkeletonJointTranslationLocked(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 targetJointIndex,
            bool& locked) const = 0;

        [[nodiscard]]
        virtual bool MapSkeletonPose(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<const AZ::Transform> sourceModelTransforms,
            AZStd::span<const AZ::Transform> targetLocalTransforms,
            AZStd::span<AZ::Transform> targetModelTransforms) const = 0;

        [[nodiscard]]
        virtual bool MapSkeletonPoseReverse(
            SkeletonMapperHandle mapperHandle,
            AZStd::span<const AZ::Transform> targetModelTransforms,
            AZStd::span<AZ::Transform> sourceModelTransforms) const = 0;

        [[nodiscard]]
        virtual SoftBodyDefinitionHandle CreateSoftBodyDefinition(
            const SoftBodyDefinitionConfiguration& configuration,
            SoftBodyOptimizationRemap* optimizationRemap = nullptr) = 0;

        [[nodiscard]]
        virtual bool ExportSoftBodyDefinition(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles) const = 0;

        [[nodiscard]]
        virtual SoftBodyDefinitionHandle ImportSoftBodyDefinition(
            const SoftBodyDefinitionArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles) = 0;

        virtual bool DestroySoftBodyDefinition(SoftBodyDefinitionHandle definitionHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(SoftBodyDefinitionHandle definitionHandle) const = 0;

        [[nodiscard]]
        virtual bool GetSoftBodyDefinitionState(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionState& state) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionDihedralBendConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyDihedralBendConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionEdgeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyEdgeConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionFaces(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyFace> faces) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionInverseBinds(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyInverseBind> inverseBinds) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionLongRangeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyLongRangeConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionMaterials(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<MaterialHandle> materials) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionRodBendTwistConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionRodStretchShearConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionSkinConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodySkinConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionVertices(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVertex> vertices) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyDefinitionVolumeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVolumeConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual HairDefinitionHandle CreateHairDefinition(
            const HairDefinitionConfiguration& configuration) = 0;

        virtual bool DestroyHairDefinition(HairDefinitionHandle definitionHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(HairDefinitionHandle definitionHandle) const = 0;

        [[nodiscard]]
        virtual bool GetHairDefinitionState(
            HairDefinitionHandle definitionHandle,
            HairDefinitionState& state) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHairNeutralDensity(
            HairDefinitionHandle definitionHandle,
            AZStd::span<float> density) const = 0;

        virtual bool SkinHairScalpVertices(
            HairDefinitionHandle definitionHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms,
            AZStd::span<AZ::Transform> preparedJointTransforms,
            AZStd::span<AZ::Vector3> scalpVertices) const = 0;

        [[nodiscard]]
        virtual SceneDefinitionHandle CreateSceneDefinition(const SceneConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual SceneDefinitionHandle CreateSceneDefinition(const SceneAssetData& assetData) = 0;

        [[nodiscard]]
        virtual bool BuildSceneAsset(
            const SceneSourceData& sourceData,
            SceneAssetData& assetData) = 0;

        virtual bool DestroySceneDefinition(SceneDefinitionHandle definitionHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(SceneDefinitionHandle definitionHandle) const = 0;

        [[nodiscard]]
        virtual bool GetSceneDefinitionState(
            SceneDefinitionHandle definitionHandle,
            SceneDefinitionState& state) const = 0;

        [[nodiscard]]
        virtual SceneInstanceHandle InstantiateScene(
            WorldHandle worldHandle,
            SceneDefinitionHandle definitionHandle) = 0;

        virtual bool DestroySceneInstance(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle) const = 0;

        [[nodiscard]]
        virtual bool GetSceneInstanceState(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            SceneInstanceState& state) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSceneBodies(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<BodyHandle> bodyHandles) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSceneConstraints(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const = 0;

        [[nodiscard]]
        virtual WorldHandle CreateWorld(const WorldConfiguration& configuration) = 0;

        virtual bool DestroyWorld(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual WorldHandle GetDefaultWorldHandle() const = 0;

        [[nodiscard]]
        virtual const IWorldQueries* GetWorldQueries(WorldHandle worldHandle) const = 0;

        [[nodiscard]]
        virtual bool IsValid(WorldHandle worldHandle) const = 0;

        [[nodiscard]]
        virtual bool GetWorldGravity(
            WorldHandle worldHandle,
            AZ::Vector3& gravity) const = 0;

        virtual bool SetWorldGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity) = 0;

        [[nodiscard]]
        virtual bool GetSimulationConfiguration(
            WorldHandle worldHandle,
            SimulationConfiguration& configuration) const = 0;

        virtual bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool GetWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            WorldRuntimeConfiguration& configuration) const = 0;

        virtual bool UpdateWorldRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration) = 0;

        virtual bool StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep) = 0;

        [[nodiscard]]
        virtual SimulationResult StepWorldDetailed(
            WorldHandle worldHandle,
            float fixedTimeStep) = 0;

        virtual bool StepAutoSimulatedWorlds(float elapsedTime) = 0;

        [[nodiscard]]
        virtual SimulationResult StepAutoSimulatedWorldsDetailed(float elapsedTime) = 0;

        [[nodiscard]]
        virtual EventView GetEvents(WorldHandle worldHandle) const = 0;

        virtual bool SetContactCallbacks(
            WorldHandle worldHandle,
            IContactCallbacks* callbacks) = 0;

        virtual bool SetBodyPairCollider(
            WorldHandle worldHandle,
            IBodyPairCollider* collider) = 0;

        virtual bool SetSimulationShapeFilter(
            WorldHandle worldHandle,
            ISimulationShapeFilter* filter) = 0;

        virtual bool SetSoftBodyContactCallbacks(
            WorldHandle worldHandle,
            ISoftBodyContactCallbacks* callbacks) = 0;

        virtual bool AddStepListener(
            WorldHandle worldHandle,
            IStepListener* listener) = 0;

        virtual bool RemoveStepListener(
            WorldHandle worldHandle,
            IStepListener* listener) = 0;

        [[nodiscard]]
        virtual HairHandle CreateHair(
            WorldHandle worldHandle,
            const HairConfiguration& configuration) = 0;

        virtual bool DestroyHair(
            WorldHandle worldHandle,
            HairHandle hairHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            HairHandle hairHandle) const = 0;

        virtual bool SetHairTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const WorldTransform& worldTransform,
            bool teleport) = 0;

        virtual bool SetHairScalpToHeadTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& scalpToHeadTransform) = 0;

        virtual bool UpdateHair(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) = 0;

        virtual bool EnableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) = 0;

        virtual bool DisableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle) = 0;

        [[nodiscard]]
        virtual bool GetHairState(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            HairState& state) const = 0;

        //! Reads all requested output streams with one compute synchronization.
        [[nodiscard]]
        virtual bool GetHairReadback(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHairVertexStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairVertexState> states) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHairRenderPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHairScalpPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHairGridCellStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairGridCellState> states) const = 0;

        [[nodiscard]]
        virtual ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const ShapeConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const CompoundShapeConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const DecoratedShapeConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual ShapeHandle CreateShape(
            WorldHandle worldHandle,
            CookedShapeHandle cookedShapeHandle) = 0;

        //! Creates an independent mutable copy of a heightfield or mutable compound shape.
        [[nodiscard]]
        virtual ShapeHandle CloneShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) = 0;

        //! Applies native scale repair and returns a new shape that retains the source shape.
        [[nodiscard]]
        virtual ShapeHandle ScaleShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) = 0;

        virtual bool DestroyShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) const = 0;

        //! Returns storage owned directly by the root shape.
        [[nodiscard]]
        virtual bool GetShapeStats(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const = 0;

        //! Includes each unique child shape once and may allocate traversal bookkeeping.
        [[nodiscard]]
        virtual bool GetShapeStatsRecursive(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const = 0;

        [[nodiscard]]
        virtual bool GetShapeProperties(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeProperties& properties) const = 0;

        [[nodiscard]]
        virtual bool GetShapeSubmergedVolume(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const = 0;

        [[nodiscard]]
        virtual bool GetPrimitiveShapeState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            PrimitiveShapeState& state) const = 0;

        [[nodiscard]]
        virtual bool GetConvexHullState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ConvexHullState& state) const = 0;

        [[nodiscard]]
        virtual BufferResult GetConvexHullPointsRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Vector3> points) const = 0;

        [[nodiscard]]
        virtual BufferResult GetConvexHullPlanesRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Plane> planes) const = 0;

        [[nodiscard]]
        virtual BufferResult GetConvexHullFaceVertexIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 faceIndex,
            AZStd::span<AZ::u32> vertexIndices) const = 0;

        //! The sub-shape ID must originate from a query against this shape.
        [[nodiscard]]
        virtual bool GetShapeMaterial(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const = 0;

        //! The sub-shape ID must originate from a query against this shape. The position is center-of-mass local space.
        [[nodiscard]]
        virtual bool GetShapeSurfaceNormal(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const = 0;

        [[nodiscard]]
        virtual bool GetShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u64& userData) const = 0;

        //! The sub-shape ID must originate from a query against this shape.
        [[nodiscard]]
        virtual bool GetShapeSubShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const = 0;

        //! Resolves one level of a query sub-shape path in the root shape's center-of-mass space.
        [[nodiscard]]
        virtual bool GetDirectChildShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const = 0;

        [[nodiscard]]
        virtual bool GetDecoratedShapeConfiguration(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            DecoratedShapeConfiguration& configuration) const = 0;

        //! Returns the ordered material list for a mesh or decorated mesh root.
        [[nodiscard]]
        virtual BufferResult GetMeshMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const = 0;

        //! The sub-shape ID must identify a mesh triangle in this shape.
        [[nodiscard]]
        virtual bool GetMeshTriangleMaterialIndex(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const = 0;

        //! The sub-shape ID must identify a mesh triangle in this shape.
        [[nodiscard]]
        virtual bool GetMeshTriangleUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const = 0;

        [[nodiscard]]
        virtual bool IsShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) const = 0;

        [[nodiscard]]
        virtual bool MakeShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const = 0;

        [[nodiscard]]
        virtual bool GetHeightfieldState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            HeightfieldState& state) const = 0;

        [[nodiscard]]
        virtual bool GetHeightfieldPosition(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const = 0;

        [[nodiscard]]
        virtual bool ProjectOntoHeightfield(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::Vector3& surfacePosition,
            SubShapeId& subShapeId) const = 0;

        [[nodiscard]]
        virtual bool IsHeightfieldNoCollision(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<float> heights) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHeightfieldMaterialIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<AZ::u8> materialIndices) const = 0;

        [[nodiscard]]
        virtual QueryResult GetHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const = 0;

        [[nodiscard]]
        virtual bool GetHeightfieldSubShapeCoordinates(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const = 0;

        virtual bool UpdateHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const float> heights,
            const HeightfieldUpdateConfiguration& configuration = {}) = 0;

        virtual bool UpdateHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const AZ::u8> materialIndices,
            AZStd::span<const MaterialHandle> materialHandles,
            bool activateBodies = true) = 0;

        virtual bool AddMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            const CompoundChildConfiguration& child,
            AZ::u32 insertionIndex,
            AZ::u32& childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) = 0;

        virtual bool RemoveMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) = 0;

        virtual bool UpdateMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const CompoundChildConfiguration& child,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) = 0;

        virtual bool UpdateMutableCompoundChildTransforms(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 startIndex,
            AZStd::span<const AZ::Vector3> positions,
            AZStd::span<const AZ::Quaternion> rotations,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {}) = 0;

        virtual bool AdjustMutableCompoundCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            bool updateMassProperties,
            bool activateBodies) = 0;

        [[nodiscard]]
        virtual bool GetCompoundChildCount(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32& childCount) const = 0;

        [[nodiscard]]
        virtual bool GetCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const = 0;

        //! The sub-shape ID must originate from a query against this compound shape.
        [[nodiscard]]
        virtual bool GetCompoundChildIndex(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const = 0;

        [[nodiscard]]
        virtual BodyHandle CreateBody(
            WorldHandle worldHandle,
            const BodyConfiguration& configuration) = 0;

        //! Creates a body with an explicit simulation identity for synchronized peers.
        [[nodiscard]]
        virtual BodyHandle CreateBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const BodyConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual BodyHandle CreateSoftBody(
            WorldHandle worldHandle,
            const SoftBodyConfiguration& configuration) = 0;

        //! Creates a soft body with an explicit simulation identity for synchronized peers.
        [[nodiscard]]
        virtual BodyHandle CreateSoftBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const SoftBodyConfiguration& configuration) = 0;

        virtual bool AddBodyToSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate) = 0;

        virtual bool AddBodiesToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles,
            bool activate) = 0;

        virtual bool RemoveBodyFromSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool RemoveBodiesFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) = 0;

        virtual bool DestroyBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool DestroyBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) = 0;

        [[nodiscard]]
        virtual bool IsBodyInSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const = 0;

        virtual bool SetBodyMoveEventsEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) = 0;

        [[nodiscard]]
        virtual RagdollDefinitionHandle CreateRagdollDefinition(
            WorldHandle worldHandle,
            const RagdollDefinitionConfiguration& configuration) = 0;

        virtual bool DestroyRagdollDefinition(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle) const = 0;

        [[nodiscard]]
        virtual QueryResult GetRagdollBodyConstraintIndices(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<AZ::s32> constraintIndices) const = 0;

        [[nodiscard]]
        virtual QueryResult GetRagdollConstraintBodyPairs(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<RagdollConstraintBodyPair> bodyPairs) const = 0;

        [[nodiscard]]
        virtual RagdollHandle CreateRagdoll(
            WorldHandle worldHandle,
            const RagdollConfiguration& configuration) = 0;

        virtual bool AddRagdollToSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            bool activate) = 0;

        virtual bool RemoveRagdollFromSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) = 0;

        virtual bool DestroyRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const = 0;

        [[nodiscard]]
        virtual bool IsRagdollInSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const = 0;

        [[nodiscard]]
        virtual bool GetRagdollState(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            RagdollState& state) const = 0;

        virtual bool SetRagdollCollisionGroupId(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::u32 collisionGroupId) = 0;

        [[nodiscard]]
        virtual QueryResult GetRagdollBodies(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<BodyHandle> bodyHandles) const = 0;

        [[nodiscard]]
        virtual QueryResult GetRagdollConstraints(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const = 0;

        virtual bool ActivateRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) = 0;

        virtual bool SetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms) = 0;

        [[nodiscard]]
        virtual QueryResult GetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const = 0;

        virtual bool DriveRagdollKinematically(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) = 0;

        virtual bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> modelTransforms) = 0;

        virtual bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) = 0;

        virtual bool ResetRagdollWarmStart(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) = 0;

        virtual bool SetRagdollVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity,
            AZ::Vector3 angularVelocity) = 0;

        virtual bool SetRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity) = 0;

        virtual bool AddRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity) = 0;

        virtual bool AddRagdollImpulse(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 impulse) = 0;

        [[nodiscard]]
        virtual ConstraintHandle CreateConstraint(
            WorldHandle worldHandle,
            const ConstraintConfiguration& configuration) = 0;

        virtual bool AddConstraintToSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual bool AddConstraintsToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles) = 0;

        virtual bool RemoveConstraintFromSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual bool RemoveConstraintsFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles) = 0;

        virtual bool DestroyConstraint(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual bool DestroyConstraints(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles) = 0;

        [[nodiscard]]
        virtual bool IsConstraintInSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const = 0;

        virtual bool SetConstraintEnabled(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            bool enabled) = 0;

        [[nodiscard]]
        virtual bool GetConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintState& state) const = 0;

        [[nodiscard]]
        virtual bool GetConstraintConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64& userData) const = 0;

        virtual bool SetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual bool GetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float& debugDrawSize) const = 0;

        virtual bool SetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float debugDrawSize) = 0;

        [[nodiscard]]
        virtual bool GetConstraintMeasurements(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintMeasurements& measurements) const = 0;

        [[nodiscard]]
        virtual bool GetCustomConstraintInfo(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            CustomConstraintInfo& info) const = 0;

        [[nodiscard]]
        virtual BufferResult GetCustomConstraintImpulses(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<float> impulses) const = 0;

        [[nodiscard]]
        virtual BufferResult GetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<AZ::u8> state) const = 0;

        virtual bool SetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const AZ::u8> state) = 0;

        virtual bool ResetConstraintWarmStart(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual bool UpdateConstraintSolverConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const ConstraintSolverConfiguration& configuration) = 0;

        virtual bool UpdateConeLimit(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float halfConeAngle) = 0;

        virtual bool UpdateDistanceLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumDistance,
            float maximumDistance,
            const SpringConfiguration& spring) = 0;

        virtual bool UpdateHingeLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumAngle,
            float maximumAngle,
            const SpringConfiguration& spring,
            float maximumFrictionTorque) = 0;

        virtual bool UpdateHingeMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetAngle,
            float targetAngularVelocity) = 0;

        //! Sets the target orientation of a hinge motor relative to its first body.
        virtual bool SetHingeTargetOrientation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const AZ::Quaternion& targetOrientation) = 0;

        virtual bool UpdatePathMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPathFraction,
            float targetVelocity) = 0;

        virtual bool UpdatePathProperties(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            PathHandle pathHandle,
            float pathFraction,
            float maximumFrictionForce) = 0;

        virtual bool UpdatePointAnchors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintSpace space,
            const WorldPosition& firstPoint,
            const WorldPosition& secondPoint) = 0;

        virtual bool UpdatePulleyLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumLength,
            float maximumLength) = 0;

        virtual bool UpdateSixDofLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const SixDofAxisLimitConfiguration> axes) = 0;

        virtual bool UpdateSixDofMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const MotorConfiguration> motors,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation,
            const AZ::Vector3& targetPosition,
            const AZ::Vector3& targetVelocity) = 0;

        virtual bool UpdateSliderMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPosition,
            float targetVelocity) = 0;

        virtual bool UpdateSliderLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumPosition,
            float maximumPosition,
            const SpringConfiguration& spring,
            float maximumFrictionForce) = 0;

        virtual bool UpdateSwingTwistMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& swingMotor,
            const MotorConfiguration& twistMotor,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation) = 0;

        virtual bool UpdateSwingTwistLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float normalHalfConeAngle,
            float planeHalfConeAngle,
            float twistMinimumAngle,
            float twistMaximumAngle,
            float maximumFrictionTorque) = 0;

        [[nodiscard]]
        virtual bool GetBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyState& state) const = 0;

        [[nodiscard]]
        virtual bool GetBodyCenterOfMassTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldTransform& transform) const = 0;

        [[nodiscard]]
        virtual bool GetBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64& userData) const = 0;

        virtual bool SetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual bool GetBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyRuntimeConfiguration& configuration) const = 0;

        //! Returns false when per-body statistics were not enabled when the native library was built.
        [[nodiscard]]
        virtual bool GetBodySimulationStatistics(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodySimulationStatistics& statistics) const = 0;

        //! Replaces rigid-body creation settings while preserving its native allocation capability.
        //! The body and configuration must remain outside the simulation, and the body cannot belong to a composite runtime object.
        virtual bool ApplyBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyFaces(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyFace> faces) const = 0;

        [[nodiscard]]
        virtual bool GetSoftBodyLocalBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Aabb& bounds) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyMaterials(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<MaterialHandle> materials) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyRodStates(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyRodState> rods) const = 0;

        [[nodiscard]]
        virtual bool GetSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SoftBodyRuntimeConfiguration& configuration) const = 0;

        //! Replaces soft-body creation settings and resets its simulated vertices.
        //! The body must remain manually updated and cannot belong to a scene instance.
        virtual bool ApplySoftBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual QueryResult GetSoftBodyVertices(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyVertex> vertices) const = 0;

        [[nodiscard]]
        virtual bool GetSoftBodyVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& volume) const = 0;

        virtual bool RecalculateSoftBodyMassProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate) = 0;

        virtual bool SkinSoftBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll) = 0;

        virtual bool UpdateSoftBodyManually(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float deltaTime) = 0;

        virtual bool UpdateSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyRuntimeConfiguration& configuration) = 0;

        virtual bool SetSoftBodyVertexInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            float inverseMass) = 0;

        virtual bool SetSoftBodyVertexInverseMasses(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses) = 0;

        virtual bool SetSoftBodyVertexVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity) = 0;

        virtual bool SetSoftBodyVertexVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities) = 0;

        [[nodiscard]]
        virtual VirtualCharacterHandle CreateVirtualCharacter(
            WorldHandle worldHandle,
            const VirtualCharacterConfiguration& configuration) = 0;

        virtual bool DestroyVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) const = 0;

        [[nodiscard]]
        virtual bool GetVirtualCharacterState(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterState& state) const = 0;

        [[nodiscard]]
        virtual bool GetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64& userData) const = 0;

        virtual bool SetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual bool GetVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterRuntimeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual QueryResult CheckVirtualCharacterCollision(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter = nullptr) const = 0;

        virtual bool UpdateVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterRuntimeConfiguration& configuration) = 0;

        virtual bool SetVirtualCharacterShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth) = 0;

        virtual bool SetVirtualCharacterInnerBodyShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle) = 0;

        virtual bool SetVirtualCharacterTransform(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const WorldTransform& transform) = 0;

        virtual bool SetVirtualCharacterVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& velocity) = 0;

        [[nodiscard]]
        virtual bool CancelVirtualCharacterVelocityTowardsSteepSlopes(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity,
            AZ::Vector3& adjustedVelocity) const = 0;

        virtual bool BeginVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) = 0;

        virtual bool EndVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) = 0;

        virtual bool SetVirtualCharacterContactCallbacks(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            IVirtualCharacterContactCallbacks* callbacks) = 0;

        [[nodiscard]]
        virtual bool CanVirtualCharacterWalkStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity) const = 0;

        virtual bool WalkVirtualCharacterStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter = nullptr) = 0;

        virtual bool StickVirtualCharacterToFloor(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter = nullptr) = 0;

        virtual bool RefreshVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const IQueryFilter* filter = nullptr) = 0;

        virtual bool UpdateVirtualCharacterGroundVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) = 0;

        [[nodiscard]]
        virtual QueryResult GetVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZStd::span<VirtualCharacterContact> contacts) const = 0;

        [[nodiscard]]
        virtual bool HasVirtualCharacterCollidedWith(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle) const = 0;

        [[nodiscard]]
        virtual bool HaveVirtualCharactersCollided(
            WorldHandle worldHandle,
            VirtualCharacterHandle firstCharacterHandle,
            VirtualCharacterHandle secondCharacterHandle) const = 0;

        virtual bool UpdateVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration) = 0;

        virtual bool EnableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterUpdateConfiguration& configuration) = 0;

        virtual bool DisableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) = 0;

        [[nodiscard]]
        virtual CharacterHandle CreateCharacter(
            WorldHandle worldHandle,
            const CharacterConfiguration& configuration) = 0;

        virtual bool DestroyCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) const = 0;

        [[nodiscard]]
        virtual bool GetCharacterState(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterState& state) const = 0;

        [[nodiscard]]
        virtual bool GetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64& userData) const = 0;

        virtual bool SetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual bool GetCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterRuntimeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual QueryResult CheckCharacterCollision(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter = nullptr) const = 0;

        virtual bool UpdateCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterRuntimeConfiguration& configuration) = 0;

        virtual bool SetCharacterShape(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth) = 0;

        virtual bool SetCharacterTransform(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const WorldTransform& transform,
            bool activate) = 0;

        virtual bool SetCharacterVelocity(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity) = 0;

        virtual bool AddCharacterImpulse(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& impulse) = 0;

        virtual bool ApplyVehicleEngineDamping(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float deltaTime) = 0;

        virtual bool ApplyVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float torque,
            float deltaTime) = 0;

        [[nodiscard]]
        virtual bool CalculateVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float acceleration,
            float& torque) const = 0;

        [[nodiscard]]
        virtual VehicleHandle CreateWheeledVehicle(
            WorldHandle worldHandle,
            const WheeledVehicleConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual VehicleHandle CreateMotorcycle(
            WorldHandle worldHandle,
            const MotorcycleConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual VehicleHandle CreateTrackedVehicle(
            WorldHandle worldHandle,
            const TrackedVehicleConfiguration& configuration) = 0;

        virtual bool DestroyVehicle(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle) const = 0;

        [[nodiscard]]
        virtual QueryResult GetWheeledVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual QueryResult GetMotorcycleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual QueryResult GetTrackedVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual bool GetVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleCollisionConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float& ratio) const = 0;

        [[nodiscard]]
        virtual bool GetVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleEngineConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetVehiclePowertrainState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehiclePowertrainState& state) const = 0;

        [[nodiscard]]
        virtual bool GetVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleRuntimeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleTransmissionConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            VehicleTrackConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetWheelLocalBasis(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            WheelBasis& basis) const = 0;

        [[nodiscard]]
        virtual bool GetWheelLocalTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            AZ::Transform& transform) const = 0;

        [[nodiscard]]
        virtual bool GetWheelWorldTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            WorldTransform& transform) const = 0;

        [[nodiscard]]
        virtual QueryResult QueryVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const = 0;

        [[nodiscard]]
        virtual QueryResult QueryVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleDifferentialConfiguration> differentials) const = 0;

        virtual bool SetTrackedVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const TrackedVehicleInput& input) = 0;

        virtual bool SetVehicleCallbacks(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            IVehicleCallbacks* callbacks) = 0;

        virtual bool SetVehicleCollisionFilter(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const IVehicleCollisionFilter* filter) = 0;

        virtual bool SetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float ratio) = 0;

        virtual bool SetVehiclePowertrainControl(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehiclePowertrainControl& control) = 0;

        virtual bool SetVehicleTrackAngularVelocity(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            float angularVelocity) = 0;

        virtual bool SetWheelMotion(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const WheelMotion& motion) = 0;

        virtual bool SetWheeledVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const WheeledVehicleInput& input) = 0;

        virtual bool UpdateMotorcycleController(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const MotorcycleControllerUpdateConfiguration& configuration) = 0;

        virtual bool UpdateVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars) = 0;

        virtual bool UpdateVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleCollisionConfiguration& configuration) = 0;

        virtual bool UpdateVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleDifferentialConfiguration> differentials) = 0;

        virtual bool UpdateVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleEngineConfiguration& configuration) = 0;

        virtual bool UpdateVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleRuntimeConfiguration& configuration) = 0;

        virtual bool UpdateVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleTransmissionConfiguration& configuration) = 0;

        virtual bool UpdateVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual BodySnapshotHandle CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodySnapshotHandle snapshotHandle) = 0;

        virtual bool DestroyBodyStateSnapshot(
            WorldHandle worldHandle,
            BodySnapshotHandle snapshotHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            BodySnapshotHandle snapshotHandle) const = 0;

        virtual bool RestoreBodyState(
            WorldHandle worldHandle,
            BodySnapshotHandle snapshotHandle) = 0;

        [[nodiscard]]
        virtual StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle) = 0;

        virtual bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        [[nodiscard]]
        virtual StateSnapshotHandle CaptureWorldState(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles) = 0;

        virtual bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles) = 0;

        virtual bool CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles,
            AZStd::span<const AZ::u32> partitionBodyCounts,
            AZStd::span<StateSnapshotHandle> snapshotHandles) = 0;

        //! Exports one snapshot or a complete multipart batch for a matching native build.
        virtual bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles,
            StateSnapshotArchive& archive) = 0;

        //! Imports into matching topology without publishing partial results on failure.
        virtual bool ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive,
            AZStd::span<StateSnapshotHandle> snapshotHandles) = 0;

        virtual bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        [[nodiscard]]
        virtual bool IsValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const = 0;

        virtual bool RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        //! Prevalidates a batch returned by CaptureWorldStateParts before beginning restore.
        virtual bool RestoreWorldStateParts(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles) = 0;

        virtual bool ValidateWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result) = 0;

        [[nodiscard]]
        virtual bool GetWorldStateDigest(
            WorldHandle worldHandle,
            WorldStateDigest& digest) const = 0;

        [[nodiscard]]
        virtual bool GetWorldStatistics(
            WorldHandle worldHandle,
            WorldStatistics& statistics) const = 0;

        virtual bool DrawDebug(
            WorldHandle worldHandle,
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer,
            const IDebugFilter* filter = nullptr) = 0;

        virtual bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const = 0;

        [[nodiscard]]
        virtual QueryResult GetBodies(
            WorldHandle worldHandle,
            BodyKind kind,
            bool activeOnly,
            AZStd::span<BodyHandle> bodies) const = 0;

        //! Returns the identity that participates in native deterministic ordering.
        [[nodiscard]]
        virtual bool GetBodyId(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyId& bodyId) const = 0;

        virtual bool ActivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool ActivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) = 0;

        virtual bool ActivateBodiesInBounds(
            WorldHandle worldHandle,
            const BroadPhaseAabb& bounds,
            ObjectLayer collisionLayer = ObjectLayer::Invalid) = 0;

        virtual bool DeactivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool DeactivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles) = 0;

        virtual bool ResetBodySleepTimer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool InvalidateBodyContactCache(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        [[nodiscard]]
        virtual bool GetBodyPointVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& point,
            AZ::Vector3& velocity) const = 0;

        [[nodiscard]]
        virtual bool GetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType& motionType) const = 0;

        [[nodiscard]]
        virtual bool GetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer& objectLayer) const = 0;

        [[nodiscard]]
        virtual bool GetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            CollisionGroupConfiguration& collisionGroup) const = 0;

        [[nodiscard]]
        virtual bool GetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle& shapeHandle) const = 0;

        [[nodiscard]]
        virtual bool GetBodyAccumulatedForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& force,
            AZ::Vector3& torque) const = 0;

        virtual bool ResetBodyAccumulatedForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool ResetBodyAccumulatedTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool ResetBodyMotion(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        [[nodiscard]]
        virtual bool GetBodyBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BroadPhaseAabb& bounds) const = 0;

        [[nodiscard]]
        virtual bool GetBodySubmergedVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const = 0;

        [[nodiscard]]
        virtual bool GetBodySurfaceNormal(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const = 0;

        [[nodiscard]]
        virtual bool GetBodyMaterial(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const = 0;

        [[nodiscard]]
        virtual bool GetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldPosition& position) const = 0;

        [[nodiscard]]
        virtual bool GetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Quaternion& rotation) const = 0;

        [[nodiscard]]
        virtual bool GetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const = 0;

        [[nodiscard]]
        virtual bool GetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity) const = 0;

        [[nodiscard]]
        virtual bool GetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& angularVelocity) const = 0;

        virtual bool SetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& position,
            bool activate) = 0;

        virtual bool SetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Quaternion& rotation,
            bool activate) = 0;

        virtual bool SetBodyTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate) = 0;

        virtual bool SetBodyTransformWhenChanged(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate) = 0;

        virtual bool SetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool SetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity) = 0;

        virtual bool SetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool AddBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool AddBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity) = 0;

        virtual bool SetBodyTransformAndVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool MoveBodyKinematically(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& target,
            float duration) = 0;

        virtual bool AddForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool activate = true) = 0;

        virtual bool AddForceAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate = true) = 0;

        virtual bool AddTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool activate = true) = 0;

        virtual bool AddForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate = true) = 0;

        virtual bool ApplyBuoyancyImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BuoyancyConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool GetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& friction) const = 0;

        virtual bool SetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float friction) = 0;

        [[nodiscard]]
        virtual bool GetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& restitution) const = 0;

        virtual bool SetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float restitution) = 0;

        [[nodiscard]]
        virtual bool GetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& gravityFactor) const = 0;

        virtual bool SetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float gravityFactor) = 0;

        [[nodiscard]]
        virtual bool GetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumLinearVelocity) const = 0;

        virtual bool SetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumLinearVelocity) = 0;

        [[nodiscard]]
        virtual bool GetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumAngularVelocity) const = 0;

        virtual bool SetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumAngularVelocity) = 0;

        [[nodiscard]]
        virtual bool GetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality& motionQuality) const = 0;

        virtual bool SetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality motionQuality) = 0;

        [[nodiscard]]
        virtual bool IsBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const = 0;

        virtual bool SetBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sensor) const = 0;

        virtual bool SetBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sensor) = 0;

        [[nodiscard]]
        virtual bool GetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& linearDamping) const = 0;

        virtual bool SetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float linearDamping) = 0;

        [[nodiscard]]
        virtual bool GetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& angularDamping) const = 0;

        virtual bool SetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float angularDamping) = 0;

        [[nodiscard]]
        virtual bool IsBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sleepingAllowed) const = 0;

        virtual bool SetBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sleepingAllowed) = 0;

        [[nodiscard]]
        virtual bool IsBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const = 0;

        virtual bool SetBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const = 0;

        virtual bool SetBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const = 0;

        virtual bool SetBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) = 0;

        [[nodiscard]]
        virtual bool GetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const = 0;

        virtual bool SetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount) = 0;

        virtual bool UpdateBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyRuntimeConfiguration& configuration,
            bool activate) = 0;

        [[nodiscard]]
        virtual bool GetBodyInverseInertia(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Matrix3x3& inverseInertia) const = 0;

        [[nodiscard]]
        virtual bool GetBodyInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& inverseMass) const = 0;

        virtual bool AddImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse) = 0;

        virtual bool AddImpulseAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const WorldPosition& position) = 0;

        virtual bool AddAngularImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularImpulse) = 0;

        virtual bool SetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle shapeHandle,
            bool updateMassProperties,
            bool activate) = 0;

        virtual bool SetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType motionType,
            bool activate) = 0;

        virtual bool SetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer objectLayer) = 0;

        virtual bool SetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const CollisionGroupConfiguration& collisionGroup,
            bool activate) = 0;

        [[nodiscard]]
        virtual bool RaycastShapeClosest(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            ShapeRaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastShapeAll(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            AZStd::span<ShapeRaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideShapePoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter,
            AZStd::span<ShapePointHit> hits) const = 0;

        [[nodiscard]]
        virtual bool CollideShapePointAny(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter = nullptr) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectShapeTriangles(
            WorldHandle worldHandle,
            const ShapeTriangleCollectionRequest& request,
            AZStd::span<ShapeTriangle> triangles) const = 0;

        [[nodiscard]]
        virtual bool RaycastTransformedShapeClosest(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            RaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastTransformedShapeAll(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZStd::span<RaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideTransformedShapePoint(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter,
            AZStd::span<OverlapHit> hits) const = 0;

        [[nodiscard]]
        virtual bool CollideTransformedShapePointAny(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter = nullptr) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTransformedShapeChildren(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            const IQueryFilter* filter,
            AZStd::span<TransformedShape> children) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTransformedShapeTriangles(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZStd::span<TransformedTriangle> triangles) const = 0;

        [[nodiscard]]
        virtual bool GetTransformedShapeSurfaceNormal(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position,
            AZ::Vector3& normal) const = 0;

        [[nodiscard]]
        virtual QueryResult GetTransformedShapeSupportingFace(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<WorldPosition> vertices) const = 0;
        [[nodiscard]]
        virtual bool RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            RaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual BufferResult RaycastClosestBatch(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastClosestPerBody(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual bool RaycastAny(
            WorldHandle worldHandle,
            const RaycastRequest& request) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastAll(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult OverlapPoint(
            WorldHandle worldHandle,
            const PointOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const = 0;

        [[nodiscard]]
        virtual bool OverlapPointAny(
            WorldHandle worldHandle,
            const PointOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<ShapeOverlapHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult OverlapShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const = 0;

        [[nodiscard]]
        virtual bool OverlapShapeAny(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual bool CastShapeClosest(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult CastShapeClosestPerBody(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult CastShapeAll(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult OverlapBroadPhase(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request,
            AZStd::span<BroadPhaseHit> hits) const = 0;

        [[nodiscard]]
        virtual bool OverlapBroadPhaseAny(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual bool CastBroadPhaseClosest(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            BroadPhaseCastHit& hit) const = 0;

        [[nodiscard]]
        virtual QueryResult CastBroadPhaseAll(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            AZStd::span<BroadPhaseCastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectShapesInBounds(
            WorldHandle worldHandle,
            const ShapeCollectionRequest& request,
            AZStd::span<TransformedShape> shapes) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSupportingFace(
            WorldHandle worldHandle,
            const SupportingFaceRequest& request,
            AZStd::span<WorldPosition> vertices) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTriangles(
            WorldHandle worldHandle,
            const TriangleCollectionRequest& request,
            AZStd::span<TransformedTriangle> triangles) const = 0;

        [[nodiscard]]
        virtual bool GetBroadPhaseBounds(
            WorldHandle worldHandle,
            BroadPhaseAabb& bounds) const = 0;

        virtual bool OptimizeBroadPhase(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual bool WereBodiesInContact(
            WorldHandle worldHandle,
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const = 0;
    };
} // namespace Jolt
