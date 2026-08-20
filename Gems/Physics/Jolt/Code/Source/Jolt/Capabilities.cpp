/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>

namespace Jolt
{
    template<class Capability>
    RuntimeImplementation& GetRuntimeImplementation(Capability& capability)
    {
        return static_cast<RuntimeImplementation&>(static_cast<Runtime&>(capability));
    }

    template<class Capability>
    const RuntimeImplementation& GetRuntimeImplementation(const Capability& capability)
    {
        return static_cast<const RuntimeImplementation&>(static_cast<const Runtime&>(capability));
    }

    AZStd::atomic<RuntimeConfiguration*> RuntimeConfiguration::s_instance;

    RuntimeConfiguration* RuntimeConfiguration::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    const SystemConfiguration& RuntimeConfiguration::GetConfiguration() const
    {
        return GetRuntimeImplementation(*this).GetConfiguration();
    }

    RuntimeInfo RuntimeConfiguration::GetRuntimeInfo() const
    {
        return GetRuntimeImplementation(*this).GetRuntimeInfo();
    }

    AZStd::atomic<Extensions*> Extensions::s_instance;

    Extensions* Extensions::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IBodyPairCollider* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IContactCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        ICustomConstraintProvider* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        ICustomConvexShapeProvider* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        ICustomPathProvider* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        ICustomShapeProvider* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IGroupFilter* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        ISimulationShapeFilter* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        ISoftBodyContactCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IStepListener* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IVehicleCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IVehicleCollisionFilter* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationResult Extensions::RegisterExtension(
        IVirtualCharacterContactCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        return GetRuntimeImplementation(*this).RegisterExtension(extension, AZStd::move(hostLease));
    }

    ExtensionRegistrationStatus Extensions::UnregisterExtension(
        const ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).UnregisterExtension(extensionHandle);
    }

    bool Extensions::GetExtensionInformation(
        const ExtensionHandle extensionHandle,
        ExtensionInformation& information) const
    {
        return GetRuntimeImplementation(*this).GetExtensionInformation(extensionHandle, information);
    }

    AZStd::atomic<Materials*> Materials::s_instance;

    Materials* Materials::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    MaterialHandle Materials::CreateMaterial(const MaterialConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateMaterial(configuration);
    }

    bool Materials::DestroyMaterial(MaterialHandle materialHandle)
    {
        return GetRuntimeImplementation(*this).DestroyMaterial(materialHandle);
    }

    bool Materials::IsValid(MaterialHandle materialHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(materialHandle);
    }

    AZStd::atomic<CollisionFilters*> CollisionFilters::s_instance;

    CollisionFilters* CollisionFilters::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    GroupFilterHandle CollisionFilters::CreateGroupFilter(
        AZ::u32 subGroupCount,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).CreateGroupFilter(subGroupCount, extensionHandle);
    }

    GroupFilterHandle CollisionFilters::CreateGroupFilterTable(const GroupFilterTableConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateGroupFilterTable(configuration);
    }

    bool CollisionFilters::DestroyGroupFilter(GroupFilterHandle filterHandle)
    {
        return GetRuntimeImplementation(*this).DestroyGroupFilter(filterHandle);
    }

    bool CollisionFilters::IsValid(GroupFilterHandle filterHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(filterHandle);
    }

    bool CollisionFilters::NotifyGroupFilterChanged(GroupFilterHandle filterHandle)
    {
        return GetRuntimeImplementation(*this).NotifyGroupFilterChanged(filterHandle);
    }

    bool CollisionFilters::GetSubGroupCollisionEnabled(
        GroupFilterHandle filterHandle,
        CollisionSubGroupId firstSubGroup,
        CollisionSubGroupId secondSubGroup,
        bool& enabled) const
    {
        return GetRuntimeImplementation(*this).GetSubGroupCollisionEnabled(filterHandle, firstSubGroup, secondSubGroup, enabled);
    }

    bool CollisionFilters::SetSubGroupCollisionEnabled(
        GroupFilterHandle filterHandle,
        CollisionSubGroupId firstSubGroup,
        CollisionSubGroupId secondSubGroup,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetSubGroupCollisionEnabled(filterHandle, firstSubGroup, secondSubGroup, enabled);
    }

    AZStd::atomic<Cooking*> Cooking::s_instance;

    Cooking* Cooking::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    CookedShapeHandle Cooking::CookShape(const ShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CookShape(configuration);
    }

    Operation<CookedShapeHandle> Cooking::CookShapeAsync(const ShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CookShapeAsync(configuration);
    }

    CookedShapeHandle Cooking::CookShape(const CookedCompoundShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CookShape(configuration);
    }

    Operation<CookedShapeHandle> Cooking::CookShapeAsync(const CookedCompoundShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CookShapeAsync(configuration);
    }

    CookedShapeHandle Cooking::CookShape(const CookedDecoratedShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CookShape(configuration);
    }

    Operation<CookedShapeHandle> Cooking::CookShapeAsync(const CookedDecoratedShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CookShapeAsync(configuration);
    }

    bool Cooking::ExportShape(
        CookedShapeHandle cookedShapeHandle,
        CookedShapeArchive& archive,
        AZStd::vector<MaterialHandle>& materialHandles,
        AZStd::vector<CookedShapeHandle>& childShapeHandles) const
    {
        return GetRuntimeImplementation(*this).ExportShape(cookedShapeHandle, archive, materialHandles, childShapeHandles);
    }

    CookedShapeHandle Cooking::ImportShape(
        const CookedShapeArchive& archive,
        AZStd::span<const MaterialHandle> materialHandles,
        AZStd::span<const CookedShapeHandle> childShapeHandles)
    {
        return GetRuntimeImplementation(*this).ImportShape(archive, materialHandles, childShapeHandles);
    }

    bool Cooking::DestroyCookedShape(CookedShapeHandle cookedShapeHandle)
    {
        return GetRuntimeImplementation(*this).DestroyCookedShape(cookedShapeHandle);
    }

    bool Cooking::IsValid(CookedShapeHandle cookedShapeHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(cookedShapeHandle);
    }

    bool Cooking::GetStats(
        CookedShapeHandle cookedShapeHandle,
        ShapeStats& stats) const
    {
        return GetRuntimeImplementation(*this).GetStats(cookedShapeHandle, stats);
    }

    bool Cooking::GetStatsRecursive(
        CookedShapeHandle cookedShapeHandle,
        ShapeStats& stats) const
    {
        return GetRuntimeImplementation(*this).GetStatsRecursive(cookedShapeHandle, stats);
    }

    bool Cooking::GetProperties(
        CookedShapeHandle cookedShapeHandle,
        ShapeProperties& properties) const
    {
        return GetRuntimeImplementation(*this).GetProperties(cookedShapeHandle, properties);
    }

    bool Cooking::GetUserData(
        CookedShapeHandle cookedShapeHandle,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetUserData(cookedShapeHandle, userData);
    }

    bool Cooking::GetCustomConvexShapeInfo(
        CookedShapeHandle cookedShapeHandle,
        CustomConvexShapeInfo& info) const
    {
        return GetRuntimeImplementation(*this).GetCustomConvexShapeInfo(cookedShapeHandle, info);
    }

    bool Cooking::GetCustomShapeInfo(
        CookedShapeHandle cookedShapeHandle,
        CustomShapeInfo& info) const
    {
        return GetRuntimeImplementation(*this).GetCustomShapeInfo(cookedShapeHandle, info);
    }

    BufferResult Cooking::GetCustomShapeDependencies(
        CookedShapeHandle cookedShapeHandle,
        AZStd::span<CustomShapeDependency> dependencies) const
    {
        return GetRuntimeImplementation(*this).GetCustomShapeDependencies(cookedShapeHandle, dependencies);
    }

    bool Cooking::GetSubShapeUserData(
        CookedShapeHandle cookedShapeHandle,
        SubShapeId subShapeId,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetSubShapeUserData(cookedShapeHandle, subShapeId, userData);
    }

    bool Cooking::GetDirectChildShape(
        CookedShapeHandle cookedShapeHandle,
        SubShapeId subShapeId,
        CookedShapeHandle& childShapeHandle,
        SubShapeTransform& transform) const
    {
        return GetRuntimeImplementation(*this).GetDirectChildShape(cookedShapeHandle, subShapeId, childShapeHandle, transform);
    }

    BufferResult Cooking::GetMeshMaterials(
        CookedShapeHandle cookedShapeHandle,
        AZStd::span<MaterialHandle> materialHandles) const
    {
        return GetRuntimeImplementation(*this).GetMeshMaterials(cookedShapeHandle, materialHandles);
    }

    bool Cooking::GetMeshTriangleMaterialIndex(
        CookedShapeHandle cookedShapeHandle,
        SubShapeId subShapeId,
        AZ::u32& materialIndex) const
    {
        return GetRuntimeImplementation(*this).GetMeshTriangleMaterialIndex(cookedShapeHandle, subShapeId, materialIndex);
    }

    bool Cooking::GetMeshTriangleUserData(
        CookedShapeHandle cookedShapeHandle,
        SubShapeId subShapeId,
        AZ::u32& userData) const
    {
        return GetRuntimeImplementation(*this).GetMeshTriangleUserData(cookedShapeHandle, subShapeId, userData);
    }

    bool Cooking::GetCompoundChildCount(
        CookedShapeHandle cookedShapeHandle,
        AZ::u32& childCount) const
    {
        return GetRuntimeImplementation(*this).GetCompoundChildCount(cookedShapeHandle, childCount);
    }

    bool Cooking::GetCompoundChild(
        CookedShapeHandle cookedShapeHandle,
        AZ::u32 childIndex,
        CookedCompoundChildConfiguration& child) const
    {
        return GetRuntimeImplementation(*this).GetCompoundChild(cookedShapeHandle, childIndex, child);
    }

    bool Cooking::GetCompoundChildIndex(
        CookedShapeHandle cookedShapeHandle,
        SubShapeId subShapeId,
        AZ::u32& childIndex) const
    {
        return GetRuntimeImplementation(*this).GetCompoundChildIndex(cookedShapeHandle, subShapeId, childIndex);
    }

    bool Cooking::Raycast(
        CookedShapeHandle cookedShapeHandle,
        const AZ::Vector3& start,
        const AZ::Vector3& direction,
        float distance,
        CookedRaycastHit& hit) const
    {
        return GetRuntimeImplementation(*this).Raycast(cookedShapeHandle, start, direction, distance, hit);
    }

    AZStd::atomic<Paths*> Paths::s_instance;

    Paths* Paths::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    PathHandle Paths::CreatePath(const HermitePathConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreatePath(configuration);
    }

    PathHandle Paths::CreatePath(const CustomPathConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreatePath(configuration);
    }

    bool Paths::DestroyPath(PathHandle pathHandle)
    {
        return GetRuntimeImplementation(*this).DestroyPath(pathHandle);
    }

    bool Paths::IsValid(PathHandle pathHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(pathHandle);
    }

    bool Paths::GetPathState(
        PathHandle pathHandle,
        PathState& state) const
    {
        return GetRuntimeImplementation(*this).GetPathState(pathHandle, state);
    }

    bool Paths::GetCustomPathInfo(
        PathHandle pathHandle,
        CustomPathInfo& info) const
    {
        return GetRuntimeImplementation(*this).GetCustomPathInfo(pathHandle, info);
    }

    bool Paths::SamplePath(
        PathHandle pathHandle,
        float fraction,
        PathSample& sample) const
    {
        return GetRuntimeImplementation(*this).SamplePath(pathHandle, fraction, sample);
    }

    bool Paths::FindClosestPathPoint(
        PathHandle pathHandle,
        const AZ::Vector3& position,
        float fractionHint,
        PathSample& sample) const
    {
        return GetRuntimeImplementation(*this).FindClosestPathPoint(pathHandle, position, fractionHint, sample);
    }

    AZStd::atomic<Skeletons*> Skeletons::s_instance;

    Skeletons* Skeletons::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    SkeletonDefinitionHandle Skeletons::CreateSkeletonDefinition(const SkeletonDefinitionConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateSkeletonDefinition(configuration);
    }

    SkeletonDefinitionHandle Skeletons::CreateSkeletonDefinition(const SkeletonDefinitionSource& source)
    {
        return GetRuntimeImplementation(*this).CreateSkeletonDefinition(source);
    }

    bool Skeletons::ExportSkeletonDefinition(
        SkeletonDefinitionHandle skeletonHandle,
        SkeletonDefinitionArchive& archive) const
    {
        return GetRuntimeImplementation(*this).ExportSkeletonDefinition(skeletonHandle, archive);
    }

    SkeletonDefinitionHandle Skeletons::ImportSkeletonDefinition(const SkeletonDefinitionArchive& archive)
    {
        return GetRuntimeImplementation(*this).ImportSkeletonDefinition(archive);
    }

    bool Skeletons::DestroySkeletonDefinition(SkeletonDefinitionHandle skeletonHandle)
    {
        return GetRuntimeImplementation(*this).DestroySkeletonDefinition(skeletonHandle);
    }

    bool Skeletons::IsValid(SkeletonDefinitionHandle skeletonHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(skeletonHandle);
    }

    QueryResult Skeletons::GetSkeletonJoints(
        SkeletonDefinitionHandle skeletonHandle,
        AZStd::span<SkeletonJoint> joints) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonJoints(skeletonHandle, joints);
    }

    bool Skeletons::FindSkeletonJoint(
        SkeletonDefinitionHandle skeletonHandle,
        AZ::Name jointName,
        AZ::u32& jointIndex) const
    {
        return GetRuntimeImplementation(*this).FindSkeletonJoint(skeletonHandle, jointName, jointIndex);
    }

    SkeletalAnimationHandle Skeletons::CreateSkeletalAnimation(const SkeletalAnimationConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateSkeletalAnimation(configuration);
    }

    SkeletalAnimationHandle Skeletons::CreateSkeletalAnimation(const SkeletalAnimationSource& source)
    {
        return GetRuntimeImplementation(*this).CreateSkeletalAnimation(source);
    }

    bool Skeletons::ExportSkeletalAnimation(
        SkeletalAnimationHandle animationHandle,
        SkeletalAnimationArchive& archive) const
    {
        return GetRuntimeImplementation(*this).ExportSkeletalAnimation(animationHandle, archive);
    }

    SkeletalAnimationHandle Skeletons::ImportSkeletalAnimation(const SkeletalAnimationArchive& archive)
    {
        return GetRuntimeImplementation(*this).ImportSkeletalAnimation(archive);
    }

    bool Skeletons::UpdateSkeletalAnimation(
        SkeletalAnimationHandle animationHandle,
        const SkeletalAnimationConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateSkeletalAnimation(animationHandle, configuration);
    }

    bool Skeletons::DestroySkeletalAnimation(SkeletalAnimationHandle animationHandle)
    {
        return GetRuntimeImplementation(*this).DestroySkeletalAnimation(animationHandle);
    }

    bool Skeletons::IsValid(SkeletalAnimationHandle animationHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(animationHandle);
    }

    bool Skeletons::GetSkeletalAnimationState(
        SkeletalAnimationHandle animationHandle,
        SkeletalAnimationState& state) const
    {
        return GetRuntimeImplementation(*this).GetSkeletalAnimationState(animationHandle, state);
    }

    bool Skeletons::GetSkeletalAnimatedJointName(
        SkeletalAnimationHandle animationHandle,
        AZ::u32 jointIndex,
        AZ::Name& jointName) const
    {
        return GetRuntimeImplementation(*this).GetSkeletalAnimatedJointName(animationHandle, jointIndex, jointName);
    }

    QueryResult Skeletons::GetSkeletalAnimationKeyframes(
        SkeletalAnimationHandle animationHandle,
        AZ::u32 jointIndex,
        AZStd::span<SkeletalAnimationKeyframe> keyframes) const
    {
        return GetRuntimeImplementation(*this).GetSkeletalAnimationKeyframes(animationHandle, jointIndex, keyframes);
    }

    bool Skeletons::SetSkeletalAnimationLooping(
        SkeletalAnimationHandle animationHandle,
        bool isLooping)
    {
        return GetRuntimeImplementation(*this).SetSkeletalAnimationLooping(animationHandle, isLooping);
    }

    bool Skeletons::ScaleSkeletalAnimation(
        SkeletalAnimationHandle animationHandle,
        float scale)
    {
        return GetRuntimeImplementation(*this).ScaleSkeletalAnimation(animationHandle, scale);
    }

    SkeletonPoseHandle Skeletons::CreateSkeletonPose(SkeletonDefinitionHandle skeletonHandle)
    {
        return GetRuntimeImplementation(*this).CreateSkeletonPose(skeletonHandle);
    }

    bool Skeletons::DestroySkeletonPose(SkeletonPoseHandle poseHandle)
    {
        return GetRuntimeImplementation(*this).DestroySkeletonPose(poseHandle);
    }

    bool Skeletons::IsValid(SkeletonPoseHandle poseHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(poseHandle);
    }

    bool Skeletons::GetSkeletonPoseState(
        SkeletonPoseHandle poseHandle,
        SkeletonPoseState& state) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonPoseState(poseHandle, state);
    }

    bool Skeletons::SetSkeletonPoseRootOffset(
        SkeletonPoseHandle poseHandle,
        const WorldPosition& rootOffset)
    {
        return GetRuntimeImplementation(*this).SetSkeletonPoseRootOffset(poseHandle, rootOffset);
    }

    bool Skeletons::SetSkeletonPoseLocalTransforms(
        SkeletonPoseHandle poseHandle,
        AZStd::span<const AZ::Transform> localTransforms)
    {
        return GetRuntimeImplementation(*this).SetSkeletonPoseLocalTransforms(poseHandle, localTransforms);
    }

    bool Skeletons::SetSkeletonPoseModelTransforms(
        SkeletonPoseHandle poseHandle,
        AZStd::span<const AZ::Transform> modelTransforms)
    {
        return GetRuntimeImplementation(*this).SetSkeletonPoseModelTransforms(poseHandle, modelTransforms);
    }

    QueryResult Skeletons::GetSkeletonPoseLocalTransforms(
        SkeletonPoseHandle poseHandle,
        AZStd::span<AZ::Transform> localTransforms) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonPoseLocalTransforms(poseHandle, localTransforms);
    }

    QueryResult Skeletons::GetSkeletonPoseModelTransforms(
        SkeletonPoseHandle poseHandle,
        AZStd::span<AZ::Transform> modelTransforms) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonPoseModelTransforms(poseHandle, modelTransforms);
    }

    bool Skeletons::SampleSkeletalAnimation(
        SkeletalAnimationHandle animationHandle,
        SkeletonPoseHandle poseHandle,
        float time)
    {
        return GetRuntimeImplementation(*this).SampleSkeletalAnimation(animationHandle, poseHandle, time);
    }

    SkeletonMapperHandle Skeletons::CreateSkeletonMapper(const SkeletonMapperConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateSkeletonMapper(configuration);
    }

    bool Skeletons::DestroySkeletonMapper(SkeletonMapperHandle mapperHandle)
    {
        return GetRuntimeImplementation(*this).DestroySkeletonMapper(mapperHandle);
    }

    bool Skeletons::IsValid(SkeletonMapperHandle mapperHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(mapperHandle);
    }

    bool Skeletons::GetSkeletonMapperState(
        SkeletonMapperHandle mapperHandle,
        SkeletonMapperState& state) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperState(mapperHandle, state);
    }

    QueryResult Skeletons::GetSkeletonMapperMappings(
        SkeletonMapperHandle mapperHandle,
        AZStd::span<SkeletonMapperMappingState> mappings) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperMappings(mapperHandle, mappings);
    }

    bool Skeletons::GetSkeletonMapperChainState(
        SkeletonMapperHandle mapperHandle,
        AZ::u32 chainIndex,
        SkeletonMapperChainState& state) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperChainState(mapperHandle, chainIndex, state);
    }

    QueryResult Skeletons::GetSkeletonMapperSourceChain(
        SkeletonMapperHandle mapperHandle,
        AZ::u32 chainIndex,
        AZStd::span<AZ::u32> jointIndices) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperSourceChain(mapperHandle, chainIndex, jointIndices);
    }

    QueryResult Skeletons::GetSkeletonMapperTargetChain(
        SkeletonMapperHandle mapperHandle,
        AZ::u32 chainIndex,
        AZStd::span<AZ::u32> jointIndices) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperTargetChain(mapperHandle, chainIndex, jointIndices);
    }

    QueryResult Skeletons::GetSkeletonMapperUnmappedJoints(
        SkeletonMapperHandle mapperHandle,
        AZStd::span<SkeletonMapperUnmappedJoint> joints) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperUnmappedJoints(mapperHandle, joints);
    }

    QueryResult Skeletons::GetSkeletonMapperLockedTranslations(
        SkeletonMapperHandle mapperHandle,
        AZStd::span<SkeletonMapperLockedTranslation> translations) const
    {
        return GetRuntimeImplementation(*this).GetSkeletonMapperLockedTranslations(mapperHandle, translations);
    }

    bool Skeletons::GetMappedSkeletonJoint(
        SkeletonMapperHandle mapperHandle,
        AZ::u32 sourceJointIndex,
        AZ::u32& targetJointIndex) const
    {
        return GetRuntimeImplementation(*this).GetMappedSkeletonJoint(mapperHandle, sourceJointIndex, targetJointIndex);
    }

    bool Skeletons::IsSkeletonJointTranslationLocked(
        SkeletonMapperHandle mapperHandle,
        AZ::u32 targetJointIndex,
        bool& locked) const
    {
        return GetRuntimeImplementation(*this).IsSkeletonJointTranslationLocked(mapperHandle, targetJointIndex, locked);
    }

    bool Skeletons::MapSkeletonPose(
        SkeletonMapperHandle mapperHandle,
        AZStd::span<const AZ::Transform> sourceModelTransforms,
        AZStd::span<const AZ::Transform> targetLocalTransforms,
        AZStd::span<AZ::Transform> targetModelTransforms) const
    {
        return GetRuntimeImplementation(*this).MapSkeletonPose(mapperHandle, sourceModelTransforms, targetLocalTransforms, targetModelTransforms);
    }

    bool Skeletons::MapSkeletonPoseReverse(
        SkeletonMapperHandle mapperHandle,
        AZStd::span<const AZ::Transform> targetModelTransforms,
        AZStd::span<AZ::Transform> sourceModelTransforms) const
    {
        return GetRuntimeImplementation(*this).MapSkeletonPoseReverse(mapperHandle, targetModelTransforms, sourceModelTransforms);
    }

    AZStd::atomic<Scenes*> Scenes::s_instance;

    Scenes* Scenes::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    SceneDefinitionHandle Scenes::CreateSceneDefinition(const SceneConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateSceneDefinition(configuration);
    }

    SceneDefinitionHandle Scenes::CreateSceneDefinition(const SceneAssetData& assetData)
    {
        return GetRuntimeImplementation(*this).CreateSceneDefinition(assetData);
    }

    bool Scenes::BuildSceneAsset(
        const SceneSourceData& sourceData,
        SceneAssetData& assetData)
    {
        return GetRuntimeImplementation(*this).BuildSceneAsset(sourceData, assetData);
    }

    bool Scenes::DestroySceneDefinition(SceneDefinitionHandle definitionHandle)
    {
        return GetRuntimeImplementation(*this).DestroySceneDefinition(definitionHandle);
    }

    bool Scenes::IsValid(SceneDefinitionHandle definitionHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(definitionHandle);
    }

    bool Scenes::GetSceneDefinitionState(
        SceneDefinitionHandle definitionHandle,
        SceneDefinitionState& state) const
    {
        return GetRuntimeImplementation(*this).GetSceneDefinitionState(definitionHandle, state);
    }

    SceneInstanceHandle Scenes::InstantiateScene(
        WorldHandle worldHandle,
        SceneDefinitionHandle definitionHandle)
    {
        return GetRuntimeImplementation(*this).InstantiateScene(worldHandle, definitionHandle);
    }

    Operation<SceneInstanceHandle> Scenes::InstantiateSceneAsync(
        WorldHandle worldHandle,
        SceneDefinitionHandle definitionHandle)
    {
        return GetRuntimeImplementation(*this).InstantiateSceneAsync(worldHandle, definitionHandle);
    }

    bool Scenes::DestroySceneInstance(
        WorldHandle worldHandle,
        SceneInstanceHandle instanceHandle)
    {
        return GetRuntimeImplementation(*this).DestroySceneInstance(worldHandle, instanceHandle);
    }

    bool Scenes::IsValid(
        WorldHandle worldHandle,
        SceneInstanceHandle instanceHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, instanceHandle);
    }

    bool Scenes::GetSceneInstanceState(
        WorldHandle worldHandle,
        SceneInstanceHandle instanceHandle,
        SceneInstanceState& state) const
    {
        return GetRuntimeImplementation(*this).GetSceneInstanceState(worldHandle, instanceHandle, state);
    }

    QueryResult Scenes::GetSceneBodies(
        WorldHandle worldHandle,
        SceneInstanceHandle instanceHandle,
        AZStd::span<BodyHandle> bodyHandles) const
    {
        return GetRuntimeImplementation(*this).GetSceneBodies(worldHandle, instanceHandle, bodyHandles);
    }

    QueryResult Scenes::GetSceneConstraints(
        WorldHandle worldHandle,
        SceneInstanceHandle instanceHandle,
        AZStd::span<ConstraintHandle> constraintHandles) const
    {
        return GetRuntimeImplementation(*this).GetSceneConstraints(worldHandle, instanceHandle, constraintHandles);
    }

    AZStd::atomic<Worlds*> Worlds::s_instance;

    Worlds* Worlds::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    WorldHandle Worlds::CreateWorld(const WorldConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateWorld(configuration);
    }

    bool Worlds::DestroyWorld(WorldHandle worldHandle)
    {
        return GetRuntimeImplementation(*this).DestroyWorld(worldHandle);
    }

    WorldHandle Worlds::GetDefaultWorldHandle() const
    {
        return GetRuntimeImplementation(*this).GetDefaultWorldHandle();
    }

    const IWorldQueries* Worlds::GetWorldQueries(WorldHandle worldHandle) const
    {
        return GetRuntimeImplementation(*this).GetWorldQueries(worldHandle);
    }

    bool Worlds::IsValid(WorldHandle worldHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle);
    }

    bool Worlds::GetWorldGravity(
        WorldHandle worldHandle,
        AZ::Vector3& gravity) const
    {
        return GetRuntimeImplementation(*this).GetWorldGravity(worldHandle, gravity);
    }

    bool Worlds::SetWorldGravity(
        WorldHandle worldHandle,
        const AZ::Vector3& gravity)
    {
        return GetRuntimeImplementation(*this).SetWorldGravity(worldHandle, gravity);
    }

    bool Worlds::GetSimulationConfiguration(
        WorldHandle worldHandle,
        SimulationConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetSimulationConfiguration(worldHandle, configuration);
    }

    bool Worlds::UpdateSimulationConfiguration(
        WorldHandle worldHandle,
        const SimulationConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateSimulationConfiguration(worldHandle, configuration);
    }

    bool Worlds::GetWorldRuntimeConfiguration(
        WorldHandle worldHandle,
        WorldRuntimeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetWorldRuntimeConfiguration(worldHandle, configuration);
    }

    bool Worlds::UpdateWorldRuntimeConfiguration(
        WorldHandle worldHandle,
        const WorldRuntimeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateWorldRuntimeConfiguration(worldHandle, configuration);
    }

    AZStd::atomic<WorldSimulation*> WorldSimulation::s_instance;

    WorldSimulation* WorldSimulation::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    bool WorldSimulation::StepWorld(
        WorldHandle worldHandle,
        float fixedTimeStep)
    {
        return GetRuntimeImplementation(*this).StepWorld(worldHandle, fixedTimeStep);
    }

    SimulationResult WorldSimulation::StepWorldDetailed(
        WorldHandle worldHandle,
        float fixedTimeStep)
    {
        return GetRuntimeImplementation(*this).StepWorldDetailed(worldHandle, fixedTimeStep);
    }

    Operation<SimulationResult> WorldSimulation::StepWorldAsync(
        WorldHandle worldHandle,
        float fixedTimeStep)
    {
        return GetRuntimeImplementation(*this).StepWorldAsync(worldHandle, fixedTimeStep);
    }

    bool WorldSimulation::StepAutoSimulatedWorlds(float elapsedTime)
    {
        return GetRuntimeImplementation(*this).StepAutoSimulatedWorlds(elapsedTime);
    }

    SimulationResult WorldSimulation::StepAutoSimulatedWorldsDetailed(float elapsedTime)
    {
        return GetRuntimeImplementation(*this).StepAutoSimulatedWorldsDetailed(elapsedTime);
    }

    Operation<AutoSimulationOperationResult> WorldSimulation::StepAutoSimulatedWorldsAsync(float elapsedTime)
    {
        return GetRuntimeImplementation(*this).StepAutoSimulatedWorldsAsync(elapsedTime);
    }

    SimulationResult WorldSimulation::StepAutoSimulatedWorldsDetailed(
        const float elapsedTime,
        AZStd::span<WorldEventBatch, MaximumWorldCount> eventBatches,
        AZ::u32& eventBatchCount)
    {
        return GetRuntimeImplementation(*this).StepAutoSimulatedWorldsDetailed(
            elapsedTime,
            eventBatches,
            eventBatchCount);
    }

    EventBatch WorldSimulation::GetEvents(WorldHandle worldHandle) const
    {
        return GetRuntimeImplementation(*this).GetEvents(worldHandle);
    }

    bool WorldSimulation::SetContactCallbacks(
        WorldHandle worldHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetContactCallbacks(worldHandle, extensionHandle);
    }

    bool WorldSimulation::SetBodyPairCollider(
        WorldHandle worldHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetBodyPairCollider(worldHandle, extensionHandle);
    }

    bool WorldSimulation::SetSimulationShapeFilter(
        WorldHandle worldHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetSimulationShapeFilter(worldHandle, extensionHandle);
    }

    bool WorldSimulation::SetSoftBodyContactCallbacks(
        WorldHandle worldHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetSoftBodyContactCallbacks(worldHandle, extensionHandle);
    }

    bool WorldSimulation::AddStepListener(
        WorldHandle worldHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).AddStepListener(worldHandle, extensionHandle);
    }

    bool WorldSimulation::RemoveStepListener(
        WorldHandle worldHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).RemoveStepListener(worldHandle, extensionHandle);
    }

    AZStd::atomic<WorldQueries*> WorldQueries::s_instance;

    WorldQueries* WorldQueries::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    bool WorldQueries::RaycastShapeClosest(
        WorldHandle worldHandle,
        const ShapeRaycastRequest& request,
        ShapeRaycastHit& hit) const
    {
        return GetRuntimeImplementation(*this).RaycastShapeClosest(worldHandle, request, hit);
    }

    QueryResult WorldQueries::RaycastShapeAll(
        WorldHandle worldHandle,
        const ShapeRaycastRequest& request,
        AZStd::span<ShapeRaycastHit> hits) const
    {
        return GetRuntimeImplementation(*this).RaycastShapeAll(worldHandle, request, hits);
    }

    QueryResult WorldQueries::CollideShapePoint(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        const IQueryFilter* filter,
        AZStd::span<ShapePointHit> hits) const
    {
        return GetRuntimeImplementation(*this).CollideShapePoint(worldHandle, shapeHandle, localPosition, filter, hits);
    }

    bool WorldQueries::CollideShapePointAny(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        const IQueryFilter* filter) const
    {
        return GetRuntimeImplementation(*this).CollideShapePointAny(worldHandle, shapeHandle, localPosition, filter);
    }

    QueryResult WorldQueries::CollectShapeTriangles(
        WorldHandle worldHandle,
        const ShapeTriangleCollectionRequest& request,
        AZStd::span<ShapeTriangle> triangles) const
    {
        return GetRuntimeImplementation(*this).CollectShapeTriangles(worldHandle, request, triangles);
    }

    bool WorldQueries::RaycastTransformedShapeClosest(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        const TransformedShapeRaycastRequest& request,
        RaycastHit& hit) const
    {
        return GetRuntimeImplementation(*this).RaycastTransformedShapeClosest(worldHandle, shape, request, hit);
    }

    QueryResult WorldQueries::RaycastTransformedShapeAll(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        const TransformedShapeRaycastRequest& request,
        AZStd::span<RaycastHit> hits) const
    {
        return GetRuntimeImplementation(*this).RaycastTransformedShapeAll(worldHandle, shape, request, hits);
    }

    QueryResult WorldQueries::CollideTransformedShapePoint(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        const WorldPosition& position,
        const IQueryFilter* filter,
        AZStd::span<OverlapHit> hits) const
    {
        return GetRuntimeImplementation(*this).CollideTransformedShapePoint(worldHandle, shape, position, filter, hits);
    }

    bool WorldQueries::CollideTransformedShapePointAny(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        const WorldPosition& position,
        const IQueryFilter* filter) const
    {
        return GetRuntimeImplementation(*this).CollideTransformedShapePointAny(worldHandle, shape, position, filter);
    }

    QueryResult WorldQueries::CollectTransformedShapeChildren(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        const BroadPhaseAabb& bounds,
        const IQueryFilter* filter,
        AZStd::span<TransformedShape> children) const
    {
        return GetRuntimeImplementation(*this).CollectTransformedShapeChildren(worldHandle, shape, bounds, filter, children);
    }

    QueryResult WorldQueries::CollectTransformedShapeTriangles(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        const BroadPhaseAabb& bounds,
        AZStd::span<TransformedTriangle> triangles) const
    {
        return GetRuntimeImplementation(*this).CollectTransformedShapeTriangles(worldHandle, shape, bounds, triangles);
    }

    bool WorldQueries::GetTransformedShapeSurfaceNormal(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        SubShapeId subShapeId,
        const WorldPosition& position,
        AZ::Vector3& normal) const
    {
        return GetRuntimeImplementation(*this).GetTransformedShapeSurfaceNormal(worldHandle, shape, subShapeId, position, normal);
    }

    QueryResult WorldQueries::GetTransformedShapeSupportingFace(
        WorldHandle worldHandle,
        const TransformedShape& shape,
        SubShapeId subShapeId,
        const AZ::Vector3& direction,
        AZStd::span<WorldPosition> vertices) const
    {
        return GetRuntimeImplementation(*this).GetTransformedShapeSupportingFace(worldHandle, shape, subShapeId, direction, vertices);
    }

    bool WorldQueries::RetainShape(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const WorldTransform& transform,
        float uniformScale,
        TransformedShape& shape) const
    {
        return GetRuntimeImplementation(*this).RetainShape(worldHandle, shapeHandle, transform, uniformScale, shape);
    }

    QueryResult WorldQueries::CollideTransformedShapes(
        WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCollisionRequest& request,
        AZStd::span<TransformedShapeCollisionHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CollideTransformedShapes(worldHandle, firstShape, secondShape, request, hits, faceBuffers);
    }

    bool WorldQueries::CollideTransformedShapes(
        WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCollisionRequest& request,
        ITransformedShapeCollisionCollector& collector) const
    {
        return GetRuntimeImplementation(*this).CollideTransformedShapes(worldHandle, firstShape, secondShape, request, collector);
    }

    QueryResult WorldQueries::CollideTransformedShapes(
        WorldHandle worldHandle,
        const ShapePlacement& firstShape,
        const ShapePlacement& secondShape,
        const TransformedShapeCollisionRequest& request,
        AZStd::span<TransformedShapeCollisionHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CollideTransformedShapes(worldHandle, firstShape, secondShape, request, hits, faceBuffers);
    }

    QueryResult WorldQueries::CastTransformedShape(
        WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCastRequest& request,
        AZStd::span<TransformedShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CastTransformedShape(worldHandle, firstShape, secondShape, request, hits, faceBuffers);
    }

    bool WorldQueries::CastTransformedShape(
        WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCastRequest& request,
        ITransformedShapeCastCollector& collector) const
    {
        return GetRuntimeImplementation(*this).CastTransformedShape(worldHandle, firstShape, secondShape, request, collector);
    }

    QueryResult WorldQueries::CastTransformedShape(
        WorldHandle worldHandle,
        const ShapePlacement& firstShape,
        const ShapePlacement& secondShape,
        const TransformedShapeCastRequest& request,
        AZStd::span<TransformedShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CastTransformedShape(worldHandle, firstShape, secondShape, request, hits, faceBuffers);
    }

    bool WorldQueries::RaycastClosest(
        WorldHandle worldHandle,
        const RaycastRequest& request,
        RaycastHit& hit) const
    {
        return GetRuntimeImplementation(*this).RaycastClosest(worldHandle, request, hit);
    }

    BufferResult WorldQueries::RaycastClosestBatch(
        WorldHandle worldHandle,
        AZStd::span<const RaycastRequest> requests,
        AZStd::span<ClosestRaycastResult> results) const
    {
        return GetRuntimeImplementation(*this).RaycastClosestBatch(worldHandle, requests, results);
    }

    Operation<RaycastBatchOperationResult> WorldQueries::RaycastClosestBatchAsync(
        WorldHandle worldHandle,
        AZStd::span<const RaycastRequest> requests) const
    {
        return GetRuntimeImplementation(*this).RaycastClosestBatchAsync(worldHandle, requests);
    }

    QueryResult WorldQueries::RaycastClosestPerBody(
        WorldHandle worldHandle,
        const RaycastRequest& request,
        AZStd::span<RaycastHit> hits) const
    {
        return GetRuntimeImplementation(*this).RaycastClosestPerBody(worldHandle, request, hits);
    }

    bool WorldQueries::RaycastAny(
        WorldHandle worldHandle,
        const RaycastRequest& request) const
    {
        return GetRuntimeImplementation(*this).RaycastAny(worldHandle, request);
    }

    QueryResult WorldQueries::RaycastAll(
        WorldHandle worldHandle,
        const RaycastRequest& request,
        AZStd::span<RaycastHit> hits) const
    {
        return GetRuntimeImplementation(*this).RaycastAll(worldHandle, request, hits);
    }

    QueryResult WorldQueries::OverlapPoint(
        WorldHandle worldHandle,
        const PointOverlapRequest& request,
        AZStd::span<OverlapHit> hits) const
    {
        return GetRuntimeImplementation(*this).OverlapPoint(worldHandle, request, hits);
    }

    bool WorldQueries::OverlapPointAny(
        WorldHandle worldHandle,
        const PointOverlapRequest& request) const
    {
        return GetRuntimeImplementation(*this).OverlapPointAny(worldHandle, request);
    }

    QueryResult WorldQueries::CollideShape(
        WorldHandle worldHandle,
        const ShapeOverlapRequest& request,
        AZStd::span<ShapeOverlapHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CollideShape(worldHandle, request, hits, faceBuffers);
    }

    QueryResult WorldQueries::OverlapShape(
        WorldHandle worldHandle,
        const ShapeOverlapRequest& request,
        AZStd::span<OverlapHit> hits) const
    {
        return GetRuntimeImplementation(*this).OverlapShape(worldHandle, request, hits);
    }

    bool WorldQueries::OverlapShapeAny(
        WorldHandle worldHandle,
        const ShapeOverlapRequest& request) const
    {
        return GetRuntimeImplementation(*this).OverlapShapeAny(worldHandle, request);
    }

    bool WorldQueries::CastShapeClosest(
        WorldHandle worldHandle,
        const ShapeCastRequest& request,
        ShapeCastHit& hit,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CastShapeClosest(worldHandle, request, hit, faceBuffers);
    }

    QueryResult WorldQueries::CastShapeClosestPerBody(
        WorldHandle worldHandle,
        const ShapeCastRequest& request,
        AZStd::span<ShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CastShapeClosestPerBody(worldHandle, request, hits, faceBuffers);
    }

    QueryResult WorldQueries::CastShapeAll(
        WorldHandle worldHandle,
        const ShapeCastRequest& request,
        AZStd::span<ShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        return GetRuntimeImplementation(*this).CastShapeAll(worldHandle, request, hits, faceBuffers);
    }

    QueryResult WorldQueries::OverlapBroadPhase(
        WorldHandle worldHandle,
        const BroadPhaseOverlapRequest& request,
        AZStd::span<BroadPhaseHit> hits) const
    {
        return GetRuntimeImplementation(*this).OverlapBroadPhase(worldHandle, request, hits);
    }

    bool WorldQueries::OverlapBroadPhaseAny(
        WorldHandle worldHandle,
        const BroadPhaseOverlapRequest& request) const
    {
        return GetRuntimeImplementation(*this).OverlapBroadPhaseAny(worldHandle, request);
    }

    bool WorldQueries::CastBroadPhaseClosest(
        WorldHandle worldHandle,
        const BroadPhaseCastRequest& request,
        BroadPhaseCastHit& hit) const
    {
        return GetRuntimeImplementation(*this).CastBroadPhaseClosest(worldHandle, request, hit);
    }

    QueryResult WorldQueries::CastBroadPhaseAll(
        WorldHandle worldHandle,
        const BroadPhaseCastRequest& request,
        AZStd::span<BroadPhaseCastHit> hits) const
    {
        return GetRuntimeImplementation(*this).CastBroadPhaseAll(worldHandle, request, hits);
    }

    QueryResult WorldQueries::CollectShapesInBounds(
        WorldHandle worldHandle,
        const ShapeCollectionRequest& request,
        AZStd::span<TransformedShape> shapes) const
    {
        return GetRuntimeImplementation(*this).CollectShapesInBounds(worldHandle, request, shapes);
    }

    QueryResult WorldQueries::GetSupportingFace(
        WorldHandle worldHandle,
        const SupportingFaceRequest& request,
        AZStd::span<WorldPosition> vertices) const
    {
        return GetRuntimeImplementation(*this).GetSupportingFace(worldHandle, request, vertices);
    }

    QueryResult WorldQueries::CollectTriangles(
        WorldHandle worldHandle,
        const TriangleCollectionRequest& request,
        AZStd::span<TransformedTriangle> triangles) const
    {
        return GetRuntimeImplementation(*this).CollectTriangles(worldHandle, request, triangles);
    }

    bool WorldQueries::GetBroadPhaseBounds(
        WorldHandle worldHandle,
        BroadPhaseAabb& bounds) const
    {
        return GetRuntimeImplementation(*this).GetBroadPhaseBounds(worldHandle, bounds);
    }

    bool WorldQueries::OptimizeBroadPhase(WorldHandle worldHandle)
    {
        return GetRuntimeImplementation(*this).OptimizeBroadPhase(worldHandle);
    }

    bool WorldQueries::WereBodiesInContact(
        WorldHandle worldHandle,
        BodyHandle firstBodyHandle,
        BodyHandle secondBodyHandle) const
    {
        return GetRuntimeImplementation(*this).WereBodiesInContact(worldHandle, firstBodyHandle, secondBodyHandle);
    }

    AZStd::atomic<Shapes*> Shapes::s_instance;

    Shapes* Shapes::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    ShapeHandle Shapes::CreateShape(
        WorldHandle worldHandle,
        const ShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateShape(worldHandle, configuration);
    }

    ShapeHandle Shapes::CreateShape(
        WorldHandle worldHandle,
        const CompoundShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateShape(worldHandle, configuration);
    }

    ShapeHandle Shapes::CreateShape(
        WorldHandle worldHandle,
        const DecoratedShapeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateShape(worldHandle, configuration);
    }

    ShapeHandle Shapes::CreateShape(
        WorldHandle worldHandle,
        CookedShapeHandle cookedShapeHandle)
    {
        return GetRuntimeImplementation(*this).CreateShape(worldHandle, cookedShapeHandle);
    }

    ShapeHandle Shapes::CloneShape(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle)
    {
        return GetRuntimeImplementation(*this).CloneShape(worldHandle, shapeHandle);
    }

    ShapeHandle Shapes::ScaleShape(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& scale)
    {
        return GetRuntimeImplementation(*this).ScaleShape(worldHandle, shapeHandle, scale);
    }

    bool Shapes::DestroyShape(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle)
    {
        return GetRuntimeImplementation(*this).DestroyShape(worldHandle, shapeHandle);
    }

    bool Shapes::IsValid(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, shapeHandle);
    }

    bool Shapes::GetShapeStats(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        ShapeStats& stats) const
    {
        return GetRuntimeImplementation(*this).GetShapeStats(worldHandle, shapeHandle, stats);
    }

    bool Shapes::GetShapeStatsRecursive(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        ShapeStats& stats) const
    {
        return GetRuntimeImplementation(*this).GetShapeStatsRecursive(worldHandle, shapeHandle, stats);
    }

    bool Shapes::GetShapeProperties(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        ShapeProperties& properties) const
    {
        return GetRuntimeImplementation(*this).GetShapeProperties(worldHandle, shapeHandle, properties);
    }

    bool Shapes::GetShapeSubmergedVolume(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const SubmergedVolumeRequest& request,
        SubmergedVolumeResult& result) const
    {
        return GetRuntimeImplementation(*this).GetShapeSubmergedVolume(worldHandle, shapeHandle, request, result);
    }

    bool Shapes::GetPrimitiveShapeState(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        PrimitiveShapeState& state) const
    {
        return GetRuntimeImplementation(*this).GetPrimitiveShapeState(worldHandle, shapeHandle, state);
    }

    bool Shapes::GetConvexHullState(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        ConvexHullState& state) const
    {
        return GetRuntimeImplementation(*this).GetConvexHullState(worldHandle, shapeHandle, state);
    }

    BufferResult Shapes::GetConvexHullPointsRelativeToCenterOfMass(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZStd::span<AZ::Vector3> points) const
    {
        return GetRuntimeImplementation(*this).GetConvexHullPointsRelativeToCenterOfMass(worldHandle, shapeHandle, points);
    }

    BufferResult Shapes::GetConvexHullPlanesRelativeToCenterOfMass(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZStd::span<AZ::Plane> planes) const
    {
        return GetRuntimeImplementation(*this).GetConvexHullPlanesRelativeToCenterOfMass(worldHandle, shapeHandle, planes);
    }

    BufferResult Shapes::GetConvexHullFaceVertexIndices(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZ::u32 faceIndex,
        AZStd::span<AZ::u32> vertexIndices) const
    {
        return GetRuntimeImplementation(*this).GetConvexHullFaceVertexIndices(worldHandle, shapeHandle, faceIndex, vertexIndices);
    }

    bool Shapes::GetShapeMaterial(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        MaterialHandle& materialHandle) const
    {
        return GetRuntimeImplementation(*this).GetShapeMaterial(worldHandle, shapeHandle, subShapeId, materialHandle);
    }

    bool Shapes::GetShapeSurfaceNormal(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        const AZ::Vector3& localSurfacePosition,
        AZ::Vector3& normal) const
    {
        return GetRuntimeImplementation(*this).GetShapeSurfaceNormal(worldHandle, shapeHandle, subShapeId, localSurfacePosition, normal);
    }

    bool Shapes::GetShapeUserData(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetShapeUserData(worldHandle, shapeHandle, userData);
    }

    bool Shapes::GetShapeSubShapeUserData(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetShapeSubShapeUserData(worldHandle, shapeHandle, subShapeId, userData);
    }

    bool Shapes::GetDirectChildShape(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        ShapeHandle& childShapeHandle,
        SubShapeTransform& transform) const
    {
        return GetRuntimeImplementation(*this).GetDirectChildShape(worldHandle, shapeHandle, subShapeId, childShapeHandle, transform);
    }

    bool Shapes::GetDecoratedShapeConfiguration(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        DecoratedShapeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetDecoratedShapeConfiguration(worldHandle, shapeHandle, configuration);
    }

    BufferResult Shapes::GetMeshMaterials(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZStd::span<MaterialHandle> materialHandles) const
    {
        return GetRuntimeImplementation(*this).GetMeshMaterials(worldHandle, shapeHandle, materialHandles);
    }

    bool Shapes::GetMeshTriangleMaterialIndex(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        AZ::u32& materialIndex) const
    {
        return GetRuntimeImplementation(*this).GetMeshTriangleMaterialIndex(worldHandle, shapeHandle, subShapeId, materialIndex);
    }

    bool Shapes::GetMeshTriangleUserData(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        AZ::u32& userData) const
    {
        return GetRuntimeImplementation(*this).GetMeshTriangleUserData(worldHandle, shapeHandle, subShapeId, userData);
    }

    bool Shapes::IsShapeScaleValid(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& scale) const
    {
        return GetRuntimeImplementation(*this).IsShapeScaleValid(worldHandle, shapeHandle, scale);
    }

    bool Shapes::MakeShapeScaleValid(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& scale,
        AZ::Vector3& validScale) const
    {
        return GetRuntimeImplementation(*this).MakeShapeScaleValid(worldHandle, shapeHandle, scale, validScale);
    }

    bool Shapes::GetHeightfieldState(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        HeightfieldState& state) const
    {
        return GetRuntimeImplementation(*this).GetHeightfieldState(worldHandle, shapeHandle, state);
    }

    bool Shapes::GetHeightfieldPosition(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZ::u32 column,
        AZ::u32 row,
        AZ::Vector3& position) const
    {
        return GetRuntimeImplementation(*this).GetHeightfieldPosition(worldHandle, shapeHandle, column, row, position);
    }

    bool Shapes::ProjectOntoHeightfield(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        AZ::Vector3& surfacePosition,
        SubShapeId& subShapeId) const
    {
        return GetRuntimeImplementation(*this).ProjectOntoHeightfield(worldHandle, shapeHandle, localPosition, surfacePosition, subShapeId);
    }

    bool Shapes::IsHeightfieldNoCollision(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZ::u32 column,
        AZ::u32 row,
        bool& noCollision) const
    {
        return GetRuntimeImplementation(*this).IsHeightfieldNoCollision(worldHandle, shapeHandle, column, row, noCollision);
    }

    QueryResult Shapes::GetHeightfieldHeights(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        AZStd::span<float> heights) const
    {
        return GetRuntimeImplementation(*this).GetHeightfieldHeights(worldHandle, shapeHandle, region, heights);
    }

    QueryResult Shapes::GetHeightfieldMaterialIndices(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        AZStd::span<AZ::u8> materialIndices) const
    {
        return GetRuntimeImplementation(*this).GetHeightfieldMaterialIndices(worldHandle, shapeHandle, region, materialIndices);
    }

    QueryResult Shapes::GetHeightfieldMaterials(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        AZStd::span<MaterialHandle> materialHandles) const
    {
        return GetRuntimeImplementation(*this).GetHeightfieldMaterials(worldHandle, shapeHandle, materialHandles);
    }

    bool Shapes::GetHeightfieldSubShapeCoordinates(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        SubShapeId subShapeId,
        HeightfieldSubShapeCoordinates& coordinates) const
    {
        return GetRuntimeImplementation(*this).GetHeightfieldSubShapeCoordinates(worldHandle, shapeHandle, subShapeId, coordinates);
    }

    bool Shapes::UpdateHeightfieldHeights(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        AZStd::span<const float> heights,
        const HeightfieldUpdateConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateHeightfieldHeights(worldHandle, shapeHandle, region, heights, configuration);
    }

    bool Shapes::UpdateHeightfieldMaterials(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        AZStd::span<const AZ::u8> materialIndices,
        AZStd::span<const MaterialHandle> materialHandles,
        bool activateBodies)
    {
        return GetRuntimeImplementation(*this).UpdateHeightfieldMaterials(worldHandle, shapeHandle, region, materialIndices, materialHandles, activateBodies);
    }

    bool Shapes::AddMutableCompoundChild(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        const CompoundChildConfiguration& child,
        AZ::u32 insertionIndex,
        AZ::u32& childIndex,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        return GetRuntimeImplementation(*this).AddMutableCompoundChild(worldHandle, compoundShapeHandle, child, insertionIndex, childIndex, updateConfiguration);
    }

    bool Shapes::RemoveMutableCompoundChild(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        AZ::u32 childIndex,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        return GetRuntimeImplementation(*this).RemoveMutableCompoundChild(worldHandle, compoundShapeHandle, childIndex, updateConfiguration);
    }

    bool Shapes::UpdateMutableCompoundChild(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        AZ::u32 childIndex,
        const CompoundChildConfiguration& child,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        return GetRuntimeImplementation(*this).UpdateMutableCompoundChild(worldHandle, compoundShapeHandle, childIndex, child, updateConfiguration);
    }

    bool Shapes::UpdateMutableCompoundChildTransforms(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        AZ::u32 startIndex,
        AZStd::span<const AZ::Vector3> positions,
        AZStd::span<const AZ::Quaternion> rotations,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        return GetRuntimeImplementation(*this).UpdateMutableCompoundChildTransforms(worldHandle, compoundShapeHandle, startIndex, positions, rotations, updateConfiguration);
    }

    bool Shapes::AdjustMutableCompoundCenterOfMass(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        bool updateMassProperties,
        bool activateBodies)
    {
        return GetRuntimeImplementation(*this).AdjustMutableCompoundCenterOfMass(worldHandle, compoundShapeHandle, updateMassProperties, activateBodies);
    }

    bool Shapes::GetCompoundChildCount(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        AZ::u32& childCount) const
    {
        return GetRuntimeImplementation(*this).GetCompoundChildCount(worldHandle, compoundShapeHandle, childCount);
    }

    bool Shapes::GetCompoundChild(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        AZ::u32 childIndex,
        CompoundChildConfiguration& child) const
    {
        return GetRuntimeImplementation(*this).GetCompoundChild(worldHandle, compoundShapeHandle, childIndex, child);
    }

    bool Shapes::GetCompoundChildIndex(
        WorldHandle worldHandle,
        ShapeHandle compoundShapeHandle,
        SubShapeId subShapeId,
        AZ::u32& childIndex) const
    {
        return GetRuntimeImplementation(*this).GetCompoundChildIndex(worldHandle, compoundShapeHandle, subShapeId, childIndex);
    }

    AZStd::atomic<Bodies*> Bodies::s_instance;

    Bodies* Bodies::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    BodyHandle Bodies::CreateBody(
        WorldHandle worldHandle,
        const BodyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateBody(worldHandle, configuration);
    }

    BodyHandle Bodies::CreateBodyWithId(
        WorldHandle worldHandle,
        BodyId bodyId,
        const BodyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateBodyWithId(worldHandle, bodyId, configuration);
    }

    BodyHandle SoftBodies::CreateSoftBody(
        WorldHandle worldHandle,
        const SoftBodyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateSoftBody(worldHandle, configuration);
    }

    bool Bodies::AddBodiesToSimulation(
        WorldHandle worldHandle,
        AZStd::span<const BodyHandle> bodyHandles,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddBodiesToSimulation(worldHandle, bodyHandles, activate);
    }

    bool Bodies::RemoveBodyFromSimulation(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).RemoveBodyFromSimulation(worldHandle, bodyHandle);
    }

    bool Bodies::RemoveBodiesFromSimulation(
        WorldHandle worldHandle,
        AZStd::span<const BodyHandle> bodyHandles)
    {
        return GetRuntimeImplementation(*this).RemoveBodiesFromSimulation(worldHandle, bodyHandles);
    }

    bool Bodies::DestroyBody(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).DestroyBody(worldHandle, bodyHandle);
    }

    bool Bodies::DestroyBodies(
        WorldHandle worldHandle,
        AZStd::span<const BodyHandle> bodyHandles)
    {
        return GetRuntimeImplementation(*this).DestroyBodies(worldHandle, bodyHandles);
    }

    bool Bodies::IsBodyInSimulation(
        WorldHandle worldHandle,
        BodyHandle bodyHandle) const
    {
        return GetRuntimeImplementation(*this).IsBodyInSimulation(worldHandle, bodyHandle);
    }

    bool Bodies::IsValid(
        WorldHandle worldHandle,
        BodyHandle bodyHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, bodyHandle);
    }

    bool Bodies::SetBodyMoveEventsEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetBodyMoveEventsEnabled(worldHandle, bodyHandle, enabled);
    }

    RagdollDefinitionHandle Ragdolls::CreateRagdollDefinition(
        WorldHandle worldHandle,
        const RagdollDefinitionConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateRagdollDefinition(worldHandle, configuration);
    }

    bool Ragdolls::DestroyRagdollDefinition(
        WorldHandle worldHandle,
        RagdollDefinitionHandle definitionHandle)
    {
        return GetRuntimeImplementation(*this).DestroyRagdollDefinition(worldHandle, definitionHandle);
    }

    bool Ragdolls::IsValid(
        WorldHandle worldHandle,
        RagdollDefinitionHandle definitionHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, definitionHandle);
    }

    QueryResult Ragdolls::GetRagdollBodyConstraintIndices(
        WorldHandle worldHandle,
        RagdollDefinitionHandle definitionHandle,
        AZStd::span<AZ::s32> constraintIndices) const
    {
        return GetRuntimeImplementation(*this).GetRagdollBodyConstraintIndices(worldHandle, definitionHandle, constraintIndices);
    }

    QueryResult Ragdolls::GetRagdollConstraintBodyPairs(
        WorldHandle worldHandle,
        RagdollDefinitionHandle definitionHandle,
        AZStd::span<RagdollConstraintBodyPair> bodyPairs) const
    {
        return GetRuntimeImplementation(*this).GetRagdollConstraintBodyPairs(worldHandle, definitionHandle, bodyPairs);
    }

    bool Constraints::UpdateDistanceLimits(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float minimumDistance,
        float maximumDistance,
        const SpringConfiguration& spring)
    {
        return GetRuntimeImplementation(*this).UpdateDistanceLimits(worldHandle, constraintHandle, minimumDistance, maximumDistance, spring);
    }

    bool Constraints::UpdateHingeLimits(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float minimumAngle,
        float maximumAngle,
        const SpringConfiguration& spring,
        float maximumFrictionTorque)
    {
        return GetRuntimeImplementation(*this).UpdateHingeLimits(worldHandle, constraintHandle, minimumAngle, maximumAngle, spring, maximumFrictionTorque);
    }

    bool Constraints::UpdateHingeMotor(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        const MotorConfiguration& motor,
        float targetAngle,
        float targetAngularVelocity)
    {
        return GetRuntimeImplementation(*this).UpdateHingeMotor(worldHandle, constraintHandle, motor, targetAngle, targetAngularVelocity);
    }

    bool Constraints::SetHingeTargetOrientation(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        const AZ::Quaternion& targetOrientation)
    {
        return GetRuntimeImplementation(*this).SetHingeTargetOrientation(worldHandle, constraintHandle, targetOrientation);
    }

    bool Constraints::UpdatePathMotor(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        const MotorConfiguration& motor,
        float targetPathFraction,
        float targetVelocity)
    {
        return GetRuntimeImplementation(*this).UpdatePathMotor(worldHandle, constraintHandle, motor, targetPathFraction, targetVelocity);
    }

    bool Constraints::UpdatePathProperties(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        PathHandle pathHandle,
        float pathFraction,
        float maximumFrictionForce)
    {
        return GetRuntimeImplementation(*this).UpdatePathProperties(worldHandle, constraintHandle, pathHandle, pathFraction, maximumFrictionForce);
    }

    bool Constraints::UpdatePointAnchors(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        ConstraintSpace space,
        const WorldPosition& firstPoint,
        const WorldPosition& secondPoint)
    {
        return GetRuntimeImplementation(*this).UpdatePointAnchors(worldHandle, constraintHandle, space, firstPoint, secondPoint);
    }

    bool Constraints::UpdatePulleyLimits(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float minimumLength,
        float maximumLength)
    {
        return GetRuntimeImplementation(*this).UpdatePulleyLimits(worldHandle, constraintHandle, minimumLength, maximumLength);
    }

    bool Constraints::UpdateSixDofLimits(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZStd::span<const SixDofAxisLimitConfiguration> axes)
    {
        return GetRuntimeImplementation(*this).UpdateSixDofLimits(worldHandle, constraintHandle, axes);
    }

    bool Constraints::UpdateSixDofMotors(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZStd::span<const MotorConfiguration> motors,
        const AZ::Vector3& targetAngularVelocity,
        const AZ::Quaternion& targetOrientation,
        const AZ::Vector3& targetPosition,
        const AZ::Vector3& targetVelocity)
    {
        return GetRuntimeImplementation(*this).UpdateSixDofMotors(worldHandle, constraintHandle, motors, targetAngularVelocity, targetOrientation, targetPosition, targetVelocity);
    }

    bool Constraints::UpdateSliderMotor(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        const MotorConfiguration& motor,
        float targetPosition,
        float targetVelocity)
    {
        return GetRuntimeImplementation(*this).UpdateSliderMotor(worldHandle, constraintHandle, motor, targetPosition, targetVelocity);
    }

    bool Constraints::UpdateSliderLimits(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float minimumPosition,
        float maximumPosition,
        const SpringConfiguration& spring,
        float maximumFrictionForce)
    {
        return GetRuntimeImplementation(*this).UpdateSliderLimits(worldHandle, constraintHandle, minimumPosition, maximumPosition, spring, maximumFrictionForce);
    }

    bool Constraints::UpdateSwingTwistMotors(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        const MotorConfiguration& swingMotor,
        const MotorConfiguration& twistMotor,
        const AZ::Vector3& targetAngularVelocity,
        const AZ::Quaternion& targetOrientation)
    {
        return GetRuntimeImplementation(*this).UpdateSwingTwistMotors(worldHandle, constraintHandle, swingMotor, twistMotor, targetAngularVelocity, targetOrientation);
    }

    bool Constraints::UpdateSwingTwistLimits(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float normalHalfConeAngle,
        float planeHalfConeAngle,
        float twistMinimumAngle,
        float twistMaximumAngle,
        float maximumFrictionTorque)
    {
        return GetRuntimeImplementation(*this).UpdateSwingTwistLimits(worldHandle, constraintHandle, normalHalfConeAngle, planeHalfConeAngle, twistMinimumAngle, twistMaximumAngle, maximumFrictionTorque);
    }

    bool Bodies::GetBodyState(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        BodyState& state) const
    {
        return GetRuntimeImplementation(*this).GetBodyState(worldHandle, bodyHandle, state);
    }

    bool Bodies::GetBodyCenterOfMassTransform(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        WorldTransform& transform) const
    {
        return GetRuntimeImplementation(*this).GetBodyCenterOfMassTransform(worldHandle, bodyHandle, transform);
    }

    bool Bodies::GetBodyConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        BodyConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetBodyConfiguration(worldHandle, bodyHandle, configuration);
    }

    QueryResult Bodies::GetBodies(
        WorldHandle worldHandle,
        BodyKind kind,
        bool activeOnly,
        AZStd::span<BodyHandle> bodies) const
    {
        return GetRuntimeImplementation(*this).GetBodies(worldHandle, kind, activeOnly, bodies);
    }

    bool Bodies::GetBodyId(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        BodyId& bodyId) const
    {
        return GetRuntimeImplementation(*this).GetBodyId(worldHandle, bodyHandle, bodyId);
    }

    bool Bodies::ActivateBody(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).ActivateBody(worldHandle, bodyHandle);
    }

    bool Bodies::ActivateBodies(
        WorldHandle worldHandle,
        AZStd::span<const BodyHandle> bodyHandles)
    {
        return GetRuntimeImplementation(*this).ActivateBodies(worldHandle, bodyHandles);
    }

    bool Bodies::ActivateBodiesInBounds(
        WorldHandle worldHandle,
        const BroadPhaseAabb& bounds,
        ObjectLayer collisionLayer)
    {
        return GetRuntimeImplementation(*this).ActivateBodiesInBounds(worldHandle, bounds, collisionLayer);
    }

    bool Bodies::DeactivateBody(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).DeactivateBody(worldHandle, bodyHandle);
    }

    bool Bodies::DeactivateBodies(
        WorldHandle worldHandle,
        AZStd::span<const BodyHandle> bodyHandles)
    {
        return GetRuntimeImplementation(*this).DeactivateBodies(worldHandle, bodyHandles);
    }

    bool Bodies::ResetBodySleepTimer(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).ResetBodySleepTimer(worldHandle, bodyHandle);
    }

    bool Bodies::InvalidateBodyContactCache(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).InvalidateBodyContactCache(worldHandle, bodyHandle);
    }

    bool Bodies::GetBodyPointVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldPosition& point,
        AZ::Vector3& velocity) const
    {
        return GetRuntimeImplementation(*this).GetBodyPointVelocity(worldHandle, bodyHandle, point, velocity);
    }

    bool Bodies::GetBodyMotionType(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        MotionType& motionType) const
    {
        return GetRuntimeImplementation(*this).GetBodyMotionType(worldHandle, bodyHandle, motionType);
    }

    bool Bodies::GetBodyObjectLayer(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        ObjectLayer& objectLayer) const
    {
        return GetRuntimeImplementation(*this).GetBodyObjectLayer(worldHandle, bodyHandle, objectLayer);
    }

    bool Bodies::GetBodyCollisionGroup(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        CollisionGroupConfiguration& collisionGroup) const
    {
        return GetRuntimeImplementation(*this).GetBodyCollisionGroup(worldHandle, bodyHandle, collisionGroup);
    }

    bool Bodies::GetBodyShape(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        ShapeHandle& shapeHandle) const
    {
        return GetRuntimeImplementation(*this).GetBodyShape(worldHandle, bodyHandle, shapeHandle);
    }

    bool Bodies::GetBodyAccumulatedForceAndTorque(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Vector3& force,
        AZ::Vector3& torque) const
    {
        return GetRuntimeImplementation(*this).GetBodyAccumulatedForceAndTorque(worldHandle, bodyHandle, force, torque);
    }

    bool Bodies::ResetBodyAccumulatedForce(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).ResetBodyAccumulatedForce(worldHandle, bodyHandle);
    }

    bool Bodies::ResetBodyAccumulatedTorque(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).ResetBodyAccumulatedTorque(worldHandle, bodyHandle);
    }

    bool Bodies::ResetBodyMotion(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).ResetBodyMotion(worldHandle, bodyHandle);
    }

    bool Bodies::GetBodyBounds(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        BroadPhaseAabb& bounds) const
    {
        return GetRuntimeImplementation(*this).GetBodyBounds(worldHandle, bodyHandle, bounds);
    }

    bool Bodies::GetBodySubmergedVolume(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldPosition& surfacePosition,
        const AZ::Vector3& surfaceNormal,
        SubmergedVolumeResult& result) const
    {
        return GetRuntimeImplementation(*this).GetBodySubmergedVolume(worldHandle, bodyHandle, surfacePosition, surfaceNormal, result);
    }

    bool Bodies::GetBodySurfaceNormal(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        SubShapeId subShapeId,
        const WorldPosition& surfacePosition,
        AZ::Vector3& normal) const
    {
        return GetRuntimeImplementation(*this).GetBodySurfaceNormal(worldHandle, bodyHandle, subShapeId, surfacePosition, normal);
    }

    bool Bodies::GetBodyMaterial(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        SubShapeId subShapeId,
        MaterialHandle& materialHandle) const
    {
        return GetRuntimeImplementation(*this).GetBodyMaterial(worldHandle, bodyHandle, subShapeId, materialHandle);
    }

    bool Bodies::GetBodyPosition(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        WorldPosition& position) const
    {
        return GetRuntimeImplementation(*this).GetBodyPosition(worldHandle, bodyHandle, position);
    }

    bool Bodies::GetBodyRotation(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Quaternion& rotation) const
    {
        return GetRuntimeImplementation(*this).GetBodyRotation(worldHandle, bodyHandle, rotation);
    }

    bool Bodies::GetBodyVelocities(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Vector3& linearVelocity,
        AZ::Vector3& angularVelocity) const
    {
        return GetRuntimeImplementation(*this).GetBodyVelocities(worldHandle, bodyHandle, linearVelocity, angularVelocity);
    }

    bool Bodies::GetBodyLinearVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Vector3& linearVelocity) const
    {
        return GetRuntimeImplementation(*this).GetBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity);
    }

    bool Bodies::GetBodyAngularVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Vector3& angularVelocity) const
    {
        return GetRuntimeImplementation(*this).GetBodyAngularVelocity(worldHandle, bodyHandle, angularVelocity);
    }

    bool Bodies::SetBodyPosition(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldPosition& position,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyPosition(worldHandle, bodyHandle, position, activate);
    }

    bool Bodies::SetBodyRotation(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Quaternion& rotation,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyRotation(worldHandle, bodyHandle, rotation, activate);
    }

    bool Bodies::SetBodyTransform(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldTransform& transform,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyTransform(worldHandle, bodyHandle, transform, activate);
    }

    bool Bodies::SetBodyTransformWhenChanged(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldTransform& transform,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyTransformWhenChanged(worldHandle, bodyHandle, transform, activate);
    }

    bool Bodies::SetBodyVelocities(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        return GetRuntimeImplementation(*this).SetBodyVelocities(worldHandle, bodyHandle, linearVelocity, angularVelocity);
    }

    bool Bodies::SetBodyLinearVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity)
    {
        return GetRuntimeImplementation(*this).SetBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity);
    }

    bool Bodies::SetBodyAngularVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& angularVelocity)
    {
        return GetRuntimeImplementation(*this).SetBodyAngularVelocity(worldHandle, bodyHandle, angularVelocity);
    }

    bool Bodies::AddBodyVelocities(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        return GetRuntimeImplementation(*this).AddBodyVelocities(worldHandle, bodyHandle, linearVelocity, angularVelocity);
    }

    bool Bodies::AddBodyLinearVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity)
    {
        return GetRuntimeImplementation(*this).AddBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity);
    }

    bool Bodies::SetBodyTransformAndVelocities(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldTransform& transform,
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        return GetRuntimeImplementation(*this).SetBodyTransformAndVelocities(worldHandle, bodyHandle, transform, linearVelocity, angularVelocity);
    }

    bool Bodies::MoveBodyKinematically(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const WorldTransform& target,
        float duration)
    {
        return GetRuntimeImplementation(*this).MoveBodyKinematically(worldHandle, bodyHandle, target, duration);
    }

    bool Bodies::AddForce(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& force,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddForce(worldHandle, bodyHandle, force, activate);
    }

    bool Bodies::AddForceAtPosition(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const WorldPosition& position,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddForceAtPosition(worldHandle, bodyHandle, force, position, activate);
    }

    bool Bodies::AddTorque(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& torque,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddTorque(worldHandle, bodyHandle, torque, activate);
    }

    bool Bodies::AddForceAndTorque(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const AZ::Vector3& torque,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddForceAndTorque(worldHandle, bodyHandle, force, torque, activate);
    }

    bool Bodies::ApplyBuoyancyImpulse(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const BuoyancyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).ApplyBuoyancyImpulse(worldHandle, bodyHandle, configuration);
    }

    bool Bodies::GetBodyFriction(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& friction) const
    {
        return GetRuntimeImplementation(*this).GetBodyFriction(worldHandle, bodyHandle, friction);
    }

    bool Bodies::SetBodyFriction(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float friction)
    {
        return GetRuntimeImplementation(*this).SetBodyFriction(worldHandle, bodyHandle, friction);
    }

    bool Bodies::GetBodyRestitution(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& restitution) const
    {
        return GetRuntimeImplementation(*this).GetBodyRestitution(worldHandle, bodyHandle, restitution);
    }

    bool Bodies::SetBodyRestitution(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float restitution)
    {
        return GetRuntimeImplementation(*this).SetBodyRestitution(worldHandle, bodyHandle, restitution);
    }

    bool Bodies::GetBodyGravityFactor(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& gravityFactor) const
    {
        return GetRuntimeImplementation(*this).GetBodyGravityFactor(worldHandle, bodyHandle, gravityFactor);
    }

    bool Bodies::SetBodyGravityFactor(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float gravityFactor)
    {
        return GetRuntimeImplementation(*this).SetBodyGravityFactor(worldHandle, bodyHandle, gravityFactor);
    }

    bool Bodies::GetBodyMaximumLinearVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& maximumLinearVelocity) const
    {
        return GetRuntimeImplementation(*this).GetBodyMaximumLinearVelocity(worldHandle, bodyHandle, maximumLinearVelocity);
    }

    bool Bodies::SetBodyMaximumLinearVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float maximumLinearVelocity)
    {
        return GetRuntimeImplementation(*this).SetBodyMaximumLinearVelocity(worldHandle, bodyHandle, maximumLinearVelocity);
    }

    bool Bodies::GetBodyMaximumAngularVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& maximumAngularVelocity) const
    {
        return GetRuntimeImplementation(*this).GetBodyMaximumAngularVelocity(worldHandle, bodyHandle, maximumAngularVelocity);
    }

    bool Bodies::SetBodyMaximumAngularVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float maximumAngularVelocity)
    {
        return GetRuntimeImplementation(*this).SetBodyMaximumAngularVelocity(worldHandle, bodyHandle, maximumAngularVelocity);
    }

    bool Bodies::GetBodyMotionQuality(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        MotionQuality& motionQuality) const
    {
        return GetRuntimeImplementation(*this).GetBodyMotionQuality(worldHandle, bodyHandle, motionQuality);
    }

    bool Bodies::SetBodyMotionQuality(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        MotionQuality motionQuality)
    {
        return GetRuntimeImplementation(*this).SetBodyMotionQuality(worldHandle, bodyHandle, motionQuality);
    }

    bool Bodies::IsBodyManifoldReductionEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool& enabled) const
    {
        return GetRuntimeImplementation(*this).IsBodyManifoldReductionEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::SetBodyManifoldReductionEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetBodyManifoldReductionEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::IsBodySensor(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool& sensor) const
    {
        return GetRuntimeImplementation(*this).IsBodySensor(worldHandle, bodyHandle, sensor);
    }

    bool Bodies::SetBodySensor(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool sensor)
    {
        return GetRuntimeImplementation(*this).SetBodySensor(worldHandle, bodyHandle, sensor);
    }

    bool Bodies::GetBodyLinearDamping(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& linearDamping) const
    {
        return GetRuntimeImplementation(*this).GetBodyLinearDamping(worldHandle, bodyHandle, linearDamping);
    }

    bool Bodies::SetBodyLinearDamping(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float linearDamping)
    {
        return GetRuntimeImplementation(*this).SetBodyLinearDamping(worldHandle, bodyHandle, linearDamping);
    }

    bool Bodies::GetBodyAngularDamping(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& angularDamping) const
    {
        return GetRuntimeImplementation(*this).GetBodyAngularDamping(worldHandle, bodyHandle, angularDamping);
    }

    bool Bodies::SetBodyAngularDamping(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float angularDamping)
    {
        return GetRuntimeImplementation(*this).SetBodyAngularDamping(worldHandle, bodyHandle, angularDamping);
    }

    bool Bodies::IsBodySleepingAllowed(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool& sleepingAllowed) const
    {
        return GetRuntimeImplementation(*this).IsBodySleepingAllowed(worldHandle, bodyHandle, sleepingAllowed);
    }

    bool Bodies::SetBodySleepingAllowed(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool sleepingAllowed)
    {
        return GetRuntimeImplementation(*this).SetBodySleepingAllowed(worldHandle, bodyHandle, sleepingAllowed);
    }

    bool Bodies::IsBodyGyroscopicForceEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool& enabled) const
    {
        return GetRuntimeImplementation(*this).IsBodyGyroscopicForceEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::SetBodyGyroscopicForceEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetBodyGyroscopicForceEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::IsBodyKinematicVsNonDynamicCollisionEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool& enabled) const
    {
        return GetRuntimeImplementation(*this).IsBodyKinematicVsNonDynamicCollisionEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::SetBodyKinematicVsNonDynamicCollisionEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetBodyKinematicVsNonDynamicCollisionEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::IsBodyEnhancedInternalEdgeRemovalEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool& enabled) const
    {
        return GetRuntimeImplementation(*this).IsBodyEnhancedInternalEdgeRemovalEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::SetBodyEnhancedInternalEdgeRemovalEnabled(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetBodyEnhancedInternalEdgeRemovalEnabled(worldHandle, bodyHandle, enabled);
    }

    bool Bodies::GetBodySolverStepCounts(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u8& velocityStepCount,
        AZ::u8& positionStepCount) const
    {
        return GetRuntimeImplementation(*this).GetBodySolverStepCounts(worldHandle, bodyHandle, velocityStepCount, positionStepCount);
    }

    bool Bodies::SetBodySolverStepCounts(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u8 velocityStepCount,
        AZ::u8 positionStepCount)
    {
        return GetRuntimeImplementation(*this).SetBodySolverStepCounts(worldHandle, bodyHandle, velocityStepCount, positionStepCount);
    }

    bool Bodies::UpdateBodyRuntimeConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const BodyRuntimeConfiguration& configuration,
        bool activate)
    {
        return GetRuntimeImplementation(*this).UpdateBodyRuntimeConfiguration(worldHandle, bodyHandle, configuration, activate);
    }

    bool Bodies::GetBodyInverseInertia(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Matrix3x3& inverseInertia) const
    {
        return GetRuntimeImplementation(*this).GetBodyInverseInertia(worldHandle, bodyHandle, inverseInertia);
    }

    bool Bodies::GetBodyInverseMass(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& inverseMass) const
    {
        return GetRuntimeImplementation(*this).GetBodyInverseMass(worldHandle, bodyHandle, inverseMass);
    }

    bool Bodies::AddImpulse(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& impulse)
    {
        return GetRuntimeImplementation(*this).AddImpulse(worldHandle, bodyHandle, impulse);
    }

    bool Bodies::AddImpulseAtPosition(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& impulse,
        const WorldPosition& position)
    {
        return GetRuntimeImplementation(*this).AddImpulseAtPosition(worldHandle, bodyHandle, impulse, position);
    }

    bool Bodies::AddAngularImpulse(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const AZ::Vector3& angularImpulse)
    {
        return GetRuntimeImplementation(*this).AddAngularImpulse(worldHandle, bodyHandle, angularImpulse);
    }

    bool Bodies::SetBodyShape(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        ShapeHandle shapeHandle,
        bool updateMassProperties,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyShape(worldHandle, bodyHandle, shapeHandle, updateMassProperties, activate);
    }

    bool Bodies::SetBodyMotionType(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        MotionType motionType,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyMotionType(worldHandle, bodyHandle, motionType, activate);
    }

    bool Bodies::SetBodyObjectLayer(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        ObjectLayer objectLayer)
    {
        return GetRuntimeImplementation(*this).SetBodyObjectLayer(worldHandle, bodyHandle, objectLayer);
    }

    bool Bodies::SetBodyCollisionGroup(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const CollisionGroupConfiguration& collisionGroup,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetBodyCollisionGroup(worldHandle, bodyHandle, collisionGroup, activate);
    }

    AZStd::atomic<Constraints*> Constraints::s_instance;

    Constraints* Constraints::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    ConstraintHandle Constraints::CreateConstraint(
        WorldHandle worldHandle,
        const ConstraintConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateConstraint(worldHandle, configuration);
    }

    bool Constraints::AddConstraintToSimulation(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle)
    {
        return GetRuntimeImplementation(*this).AddConstraintToSimulation(worldHandle, constraintHandle);
    }

    bool Constraints::AddConstraintsToSimulation(
        WorldHandle worldHandle,
        AZStd::span<const ConstraintHandle> constraintHandles)
    {
        return GetRuntimeImplementation(*this).AddConstraintsToSimulation(worldHandle, constraintHandles);
    }

    bool Constraints::RemoveConstraintFromSimulation(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle)
    {
        return GetRuntimeImplementation(*this).RemoveConstraintFromSimulation(worldHandle, constraintHandle);
    }

    bool Constraints::RemoveConstraintsFromSimulation(
        WorldHandle worldHandle,
        AZStd::span<const ConstraintHandle> constraintHandles)
    {
        return GetRuntimeImplementation(*this).RemoveConstraintsFromSimulation(worldHandle, constraintHandles);
    }

    bool Constraints::DestroyConstraint(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle)
    {
        return GetRuntimeImplementation(*this).DestroyConstraint(worldHandle, constraintHandle);
    }

    bool Constraints::DestroyConstraints(
        WorldHandle worldHandle,
        AZStd::span<const ConstraintHandle> constraintHandles)
    {
        return GetRuntimeImplementation(*this).DestroyConstraints(worldHandle, constraintHandles);
    }

    bool Constraints::IsConstraintInSimulation(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle) const
    {
        return GetRuntimeImplementation(*this).IsConstraintInSimulation(worldHandle, constraintHandle);
    }

    bool Constraints::IsValid(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, constraintHandle);
    }

    bool Constraints::SetConstraintEnabled(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        bool enabled)
    {
        return GetRuntimeImplementation(*this).SetConstraintEnabled(worldHandle, constraintHandle, enabled);
    }

    bool Constraints::GetConstraintState(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        ConstraintState& state) const
    {
        return GetRuntimeImplementation(*this).GetConstraintState(worldHandle, constraintHandle, state);
    }

    bool Constraints::GetConstraintConfiguration(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        ConstraintConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetConstraintConfiguration(worldHandle, constraintHandle, configuration);
    }

    bool Constraints::GetConstraintUserData(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetConstraintUserData(worldHandle, constraintHandle, userData);
    }

    bool Constraints::SetConstraintUserData(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZ::u64 userData)
    {
        return GetRuntimeImplementation(*this).SetConstraintUserData(worldHandle, constraintHandle, userData);
    }

    bool Constraints::GetConstraintDebugDrawSize(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float& debugDrawSize) const
    {
        return GetRuntimeImplementation(*this).GetConstraintDebugDrawSize(worldHandle, constraintHandle, debugDrawSize);
    }

    bool Constraints::SetConstraintDebugDrawSize(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float debugDrawSize)
    {
        return GetRuntimeImplementation(*this).SetConstraintDebugDrawSize(worldHandle, constraintHandle, debugDrawSize);
    }

    bool Constraints::GetConstraintMeasurements(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        ConstraintMeasurements& measurements) const
    {
        return GetRuntimeImplementation(*this).GetConstraintMeasurements(worldHandle, constraintHandle, measurements);
    }

    bool Constraints::GetCustomConstraintInfo(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        CustomConstraintInfo& info) const
    {
        return GetRuntimeImplementation(*this).GetCustomConstraintInfo(worldHandle, constraintHandle, info);
    }

    BufferResult Constraints::GetCustomConstraintImpulses(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZStd::span<float> impulses) const
    {
        return GetRuntimeImplementation(*this).GetCustomConstraintImpulses(worldHandle, constraintHandle, impulses);
    }

    BufferResult Constraints::GetCustomConstraintState(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZStd::span<AZ::u8> state) const
    {
        return GetRuntimeImplementation(*this).GetCustomConstraintState(worldHandle, constraintHandle, state);
    }

    bool Constraints::SetCustomConstraintState(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        AZStd::span<const AZ::u8> state)
    {
        return GetRuntimeImplementation(*this).SetCustomConstraintState(worldHandle, constraintHandle, state);
    }

    bool Constraints::ResetConstraintWarmStart(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle)
    {
        return GetRuntimeImplementation(*this).ResetConstraintWarmStart(worldHandle, constraintHandle);
    }

    bool Constraints::UpdateConstraintSolverConfiguration(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        const ConstraintSolverConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateConstraintSolverConfiguration(worldHandle, constraintHandle, configuration);
    }

    bool Constraints::UpdateConeLimit(
        WorldHandle worldHandle,
        ConstraintHandle constraintHandle,
        float halfConeAngle)
    {
        return GetRuntimeImplementation(*this).UpdateConeLimit(worldHandle, constraintHandle, halfConeAngle);
    }

    AZStd::atomic<Characters*> Characters::s_instance;

    Characters* Characters::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    VirtualCharacterHandle Characters::CreateVirtualCharacter(
        WorldHandle worldHandle,
        const VirtualCharacterConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateVirtualCharacter(worldHandle, configuration);
    }

    bool Characters::DestroyVirtualCharacter(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle)
    {
        return GetRuntimeImplementation(*this).DestroyVirtualCharacter(worldHandle, characterHandle);
    }

    bool Characters::IsValid(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, characterHandle);
    }

    bool Characters::GetVirtualCharacterState(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        VirtualCharacterState& state) const
    {
        return GetRuntimeImplementation(*this).GetVirtualCharacterState(worldHandle, characterHandle, state);
    }

    bool Characters::GetVirtualCharacterUserData(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetVirtualCharacterUserData(worldHandle, characterHandle, userData);
    }

    bool Characters::SetVirtualCharacterUserData(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        AZ::u64 userData)
    {
        return GetRuntimeImplementation(*this).SetVirtualCharacterUserData(worldHandle, characterHandle, userData);
    }

    bool Characters::GetVirtualCharacterRuntimeConfiguration(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        VirtualCharacterRuntimeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetVirtualCharacterRuntimeConfiguration(worldHandle, characterHandle, configuration);
    }

    QueryResult Characters::CheckVirtualCharacterCollision(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const CharacterCollisionRequest& request,
        AZStd::span<CharacterCollisionHit> hits,
        const ICharacterCollisionFilter* filter) const
    {
        return GetRuntimeImplementation(*this).CheckVirtualCharacterCollision(worldHandle, characterHandle, request, hits, filter);
    }

    bool Characters::UpdateVirtualCharacterRuntimeConfiguration(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const VirtualCharacterRuntimeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVirtualCharacterRuntimeConfiguration(worldHandle, characterHandle, configuration);
    }

    bool Characters::SetVirtualCharacterShape(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        ShapeHandle shapeHandle,
        float maximumPenetrationDepth)
    {
        return GetRuntimeImplementation(*this).SetVirtualCharacterShape(worldHandle, characterHandle, shapeHandle, maximumPenetrationDepth);
    }

    bool Characters::SetVirtualCharacterInnerBodyShape(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        ShapeHandle shapeHandle)
    {
        return GetRuntimeImplementation(*this).SetVirtualCharacterInnerBodyShape(worldHandle, characterHandle, shapeHandle);
    }

    bool Characters::SetVirtualCharacterTransform(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const WorldTransform& transform)
    {
        return GetRuntimeImplementation(*this).SetVirtualCharacterTransform(worldHandle, characterHandle, transform);
    }

    bool Characters::SetVirtualCharacterVelocity(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const AZ::Vector3& velocity)
    {
        return GetRuntimeImplementation(*this).SetVirtualCharacterVelocity(worldHandle, characterHandle, velocity);
    }

    bool Characters::CancelVirtualCharacterVelocityTowardsSteepSlopes(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const AZ::Vector3& desiredVelocity,
        AZ::Vector3& adjustedVelocity) const
    {
        return GetRuntimeImplementation(*this).CancelVirtualCharacterVelocityTowardsSteepSlopes(worldHandle, characterHandle, desiredVelocity, adjustedVelocity);
    }

    bool Characters::BeginVirtualCharacterContactTracking(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle)
    {
        return GetRuntimeImplementation(*this).BeginVirtualCharacterContactTracking(worldHandle, characterHandle);
    }

    bool Characters::EndVirtualCharacterContactTracking(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle)
    {
        return GetRuntimeImplementation(*this).EndVirtualCharacterContactTracking(worldHandle, characterHandle);
    }

    bool Characters::SetVirtualCharacterContactCallbacks(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetVirtualCharacterContactCallbacks(
            worldHandle,
            characterHandle,
            extensionHandle);
    }

    bool Characters::CanVirtualCharacterWalkStairs(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const AZ::Vector3& desiredVelocity) const
    {
        return GetRuntimeImplementation(*this).CanVirtualCharacterWalkStairs(worldHandle, characterHandle, desiredVelocity);
    }

    bool Characters::WalkVirtualCharacterStairs(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const VirtualCharacterStairConfiguration& configuration,
        const IQueryFilter* filter)
    {
        return GetRuntimeImplementation(*this).WalkVirtualCharacterStairs(worldHandle, characterHandle, configuration, filter);
    }

    bool Characters::StickVirtualCharacterToFloor(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const AZ::Vector3& stepDown,
        const IQueryFilter* filter)
    {
        return GetRuntimeImplementation(*this).StickVirtualCharacterToFloor(worldHandle, characterHandle, stepDown, filter);
    }

    bool Characters::RefreshVirtualCharacterContacts(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const IQueryFilter* filter)
    {
        return GetRuntimeImplementation(*this).RefreshVirtualCharacterContacts(worldHandle, characterHandle, filter);
    }

    bool Characters::UpdateVirtualCharacterGroundVelocity(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle)
    {
        return GetRuntimeImplementation(*this).UpdateVirtualCharacterGroundVelocity(worldHandle, characterHandle);
    }

    QueryResult Characters::GetVirtualCharacterContacts(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        AZStd::span<VirtualCharacterContact> contacts) const
    {
        return GetRuntimeImplementation(*this).GetVirtualCharacterContacts(worldHandle, characterHandle, contacts);
    }

    bool Characters::HasVirtualCharacterCollidedWith(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        BodyHandle bodyHandle) const
    {
        return GetRuntimeImplementation(*this).HasVirtualCharacterCollidedWith(worldHandle, characterHandle, bodyHandle);
    }

    bool Characters::HaveVirtualCharactersCollided(
        WorldHandle worldHandle,
        VirtualCharacterHandle firstCharacterHandle,
        VirtualCharacterHandle secondCharacterHandle) const
    {
        return GetRuntimeImplementation(*this).HaveVirtualCharactersCollided(worldHandle, firstCharacterHandle, secondCharacterHandle);
    }

    bool Characters::UpdateVirtualCharacter(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        float deltaTime,
        const VirtualCharacterUpdateConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVirtualCharacter(worldHandle, characterHandle, deltaTime, configuration);
    }

    bool Characters::EnableVirtualCharacterAutoUpdate(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle,
        const VirtualCharacterUpdateConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).EnableVirtualCharacterAutoUpdate(worldHandle, characterHandle, configuration);
    }

    bool Characters::DisableVirtualCharacterAutoUpdate(
        WorldHandle worldHandle,
        VirtualCharacterHandle characterHandle)
    {
        return GetRuntimeImplementation(*this).DisableVirtualCharacterAutoUpdate(worldHandle, characterHandle);
    }

    CharacterHandle Characters::CreateCharacter(
        WorldHandle worldHandle,
        const CharacterConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateCharacter(worldHandle, configuration);
    }

    bool Characters::DestroyCharacter(
        WorldHandle worldHandle,
        CharacterHandle characterHandle)
    {
        return GetRuntimeImplementation(*this).DestroyCharacter(worldHandle, characterHandle);
    }

    bool Characters::IsValid(
        WorldHandle worldHandle,
        CharacterHandle characterHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, characterHandle);
    }

    bool Characters::GetCharacterState(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        CharacterState& state) const
    {
        return GetRuntimeImplementation(*this).GetCharacterState(worldHandle, characterHandle, state);
    }

    bool Characters::GetCharacterUserData(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetCharacterUserData(worldHandle, characterHandle, userData);
    }

    bool Characters::SetCharacterUserData(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        AZ::u64 userData)
    {
        return GetRuntimeImplementation(*this).SetCharacterUserData(worldHandle, characterHandle, userData);
    }

    bool Characters::GetCharacterRuntimeConfiguration(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        CharacterRuntimeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetCharacterRuntimeConfiguration(worldHandle, characterHandle, configuration);
    }

    QueryResult Characters::CheckCharacterCollision(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        const CharacterCollisionRequest& request,
        AZStd::span<CharacterCollisionHit> hits,
        const ICharacterCollisionFilter* filter) const
    {
        return GetRuntimeImplementation(*this).CheckCharacterCollision(worldHandle, characterHandle, request, hits, filter);
    }

    bool Characters::UpdateCharacterRuntimeConfiguration(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        const CharacterRuntimeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateCharacterRuntimeConfiguration(worldHandle, characterHandle, configuration);
    }

    bool Characters::SetCharacterShape(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        ShapeHandle shapeHandle,
        float maximumPenetrationDepth)
    {
        return GetRuntimeImplementation(*this).SetCharacterShape(worldHandle, characterHandle, shapeHandle, maximumPenetrationDepth);
    }

    bool Characters::SetCharacterTransform(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        const WorldTransform& transform,
        bool activate)
    {
        return GetRuntimeImplementation(*this).SetCharacterTransform(worldHandle, characterHandle, transform, activate);
    }

    bool Characters::SetCharacterVelocity(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        const AZ::Vector3& velocity)
    {
        return GetRuntimeImplementation(*this).SetCharacterVelocity(worldHandle, characterHandle, velocity);
    }

    bool Characters::AddCharacterImpulse(
        WorldHandle worldHandle,
        CharacterHandle characterHandle,
        const AZ::Vector3& impulse)
    {
        return GetRuntimeImplementation(*this).AddCharacterImpulse(worldHandle, characterHandle, impulse);
    }

    AZStd::atomic<Vehicles*> Vehicles::s_instance;

    Vehicles* Vehicles::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    bool Vehicles::ApplyVehicleEngineDamping(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        float deltaTime)
    {
        return GetRuntimeImplementation(*this).ApplyVehicleEngineDamping(worldHandle, vehicleHandle, deltaTime);
    }

    bool Vehicles::ApplyVehicleEngineTorque(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        float torque,
        float deltaTime)
    {
        return GetRuntimeImplementation(*this).ApplyVehicleEngineTorque(worldHandle, vehicleHandle, torque, deltaTime);
    }

    bool Vehicles::CalculateVehicleEngineTorque(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        float acceleration,
        float& torque) const
    {
        return GetRuntimeImplementation(*this).CalculateVehicleEngineTorque(worldHandle, vehicleHandle, acceleration, torque);
    }

    VehicleHandle Vehicles::CreateWheeledVehicle(
        WorldHandle worldHandle,
        const WheeledVehicleConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateWheeledVehicle(worldHandle, configuration);
    }

    VehicleHandle Vehicles::CreateMotorcycle(
        WorldHandle worldHandle,
        const MotorcycleConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateMotorcycle(worldHandle, configuration);
    }

    VehicleHandle Vehicles::CreateTrackedVehicle(
        WorldHandle worldHandle,
        const TrackedVehicleConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateTrackedVehicle(worldHandle, configuration);
    }

    bool Vehicles::DestroyVehicle(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle)
    {
        return GetRuntimeImplementation(*this).DestroyVehicle(worldHandle, vehicleHandle);
    }

    bool Vehicles::IsValid(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, vehicleHandle);
    }

    QueryResult Vehicles::GetWheeledVehicleState(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        WheeledVehicleState& state,
        AZStd::span<WheelState> wheels) const
    {
        return GetRuntimeImplementation(*this).GetWheeledVehicleState(worldHandle, vehicleHandle, state, wheels);
    }

    QueryResult Vehicles::GetMotorcycleState(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        MotorcycleState& state,
        AZStd::span<WheelState> wheels) const
    {
        return GetRuntimeImplementation(*this).GetMotorcycleState(worldHandle, vehicleHandle, state, wheels);
    }

    QueryResult Vehicles::GetTrackedVehicleState(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        TrackedVehicleState& state,
        AZStd::span<WheelState> wheels) const
    {
        return GetRuntimeImplementation(*this).GetTrackedVehicleState(worldHandle, vehicleHandle, state, wheels);
    }

    bool Vehicles::GetVehicleCollisionConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        VehicleCollisionConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetVehicleCollisionConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::GetVehicleDifferentialLimitedSlipRatio(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        float& ratio) const
    {
        return GetRuntimeImplementation(*this).GetVehicleDifferentialLimitedSlipRatio(worldHandle, vehicleHandle, ratio);
    }

    bool Vehicles::GetVehicleEngineConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        VehicleEngineConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetVehicleEngineConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::GetVehiclePowertrainState(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        VehiclePowertrainState& state) const
    {
        return GetRuntimeImplementation(*this).GetVehiclePowertrainState(worldHandle, vehicleHandle, state);
    }

    bool Vehicles::GetVehicleRuntimeConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        VehicleRuntimeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetVehicleRuntimeConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::GetVehicleTransmissionConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        VehicleTransmissionConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetVehicleTransmissionConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::GetVehicleTrackConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 trackIndex,
        VehicleTrackConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetVehicleTrackConfiguration(worldHandle, vehicleHandle, trackIndex, configuration);
    }

    bool Vehicles::GetWheelLocalBasis(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 wheelIndex,
        WheelBasis& basis) const
    {
        return GetRuntimeImplementation(*this).GetWheelLocalBasis(worldHandle, vehicleHandle, wheelIndex, basis);
    }

    bool Vehicles::GetWheelLocalTransform(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp,
        AZ::Transform& transform) const
    {
        return GetRuntimeImplementation(*this).GetWheelLocalTransform(worldHandle, vehicleHandle, wheelIndex, wheelRight, wheelUp, transform);
    }

    bool Vehicles::GetWheelWorldTransform(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp,
        WorldTransform& transform) const
    {
        return GetRuntimeImplementation(*this).GetWheelWorldTransform(worldHandle, vehicleHandle, wheelIndex, wheelRight, wheelUp, transform);
    }

    QueryResult Vehicles::QueryVehicleAntiRollBars(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const
    {
        return GetRuntimeImplementation(*this).QueryVehicleAntiRollBars(worldHandle, vehicleHandle, antiRollBars);
    }

    QueryResult Vehicles::QueryVehicleDifferentials(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZStd::span<VehicleDifferentialConfiguration> differentials) const
    {
        return GetRuntimeImplementation(*this).QueryVehicleDifferentials(worldHandle, vehicleHandle, differentials);
    }

    bool Vehicles::SetTrackedVehicleInput(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const TrackedVehicleInput& input)
    {
        return GetRuntimeImplementation(*this).SetTrackedVehicleInput(worldHandle, vehicleHandle, input);
    }

    bool Vehicles::SetVehicleCallbacks(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetVehicleCallbacks(worldHandle, vehicleHandle, extensionHandle);
    }

    bool Vehicles::SetVehicleCollisionFilter(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        ExtensionHandle extensionHandle)
    {
        return GetRuntimeImplementation(*this).SetVehicleCollisionFilter(worldHandle, vehicleHandle, extensionHandle);
    }

    bool Vehicles::SetVehicleDifferentialLimitedSlipRatio(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        float ratio)
    {
        return GetRuntimeImplementation(*this).SetVehicleDifferentialLimitedSlipRatio(worldHandle, vehicleHandle, ratio);
    }

    bool Vehicles::SetVehiclePowertrainControl(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const VehiclePowertrainControl& control)
    {
        return GetRuntimeImplementation(*this).SetVehiclePowertrainControl(worldHandle, vehicleHandle, control);
    }

    bool Vehicles::SetVehicleTrackAngularVelocity(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 trackIndex,
        float angularVelocity)
    {
        return GetRuntimeImplementation(*this).SetVehicleTrackAngularVelocity(worldHandle, vehicleHandle, trackIndex, angularVelocity);
    }

    bool Vehicles::SetWheelMotion(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 wheelIndex,
        const WheelMotion& motion)
    {
        return GetRuntimeImplementation(*this).SetWheelMotion(worldHandle, vehicleHandle, wheelIndex, motion);
    }

    bool Vehicles::SetWheeledVehicleInput(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const WheeledVehicleInput& input)
    {
        return GetRuntimeImplementation(*this).SetWheeledVehicleInput(worldHandle, vehicleHandle, input);
    }

    bool Vehicles::UpdateMotorcycleController(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const MotorcycleControllerUpdateConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateMotorcycleController(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::UpdateVehicleAntiRollBars(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleAntiRollBars(worldHandle, vehicleHandle, antiRollBars);
    }

    bool Vehicles::UpdateVehicleCollisionConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const VehicleCollisionConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleCollisionConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::UpdateVehicleDifferentials(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZStd::span<const VehicleDifferentialConfiguration> differentials)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleDifferentials(worldHandle, vehicleHandle, differentials);
    }

    bool Vehicles::UpdateVehicleEngineConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const VehicleEngineConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleEngineConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::UpdateVehicleRuntimeConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const VehicleRuntimeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleRuntimeConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::UpdateVehicleTransmissionConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        const VehicleTransmissionConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleTransmissionConfiguration(worldHandle, vehicleHandle, configuration);
    }

    bool Vehicles::UpdateVehicleTrackConfiguration(
        WorldHandle worldHandle,
        VehicleHandle vehicleHandle,
        AZ::u32 trackIndex,
        const VehicleTrackConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateVehicleTrackConfiguration(worldHandle, vehicleHandle, trackIndex, configuration);
    }

    AZStd::atomic<Ragdolls*> Ragdolls::s_instance;

    Ragdolls* Ragdolls::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    RagdollHandle Ragdolls::CreateRagdoll(
        WorldHandle worldHandle,
        const RagdollConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateRagdoll(worldHandle, configuration);
    }

    bool Ragdolls::AddRagdollToSimulation(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddRagdollToSimulation(worldHandle, ragdollHandle, activate);
    }

    bool Ragdolls::RemoveRagdollFromSimulation(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle)
    {
        return GetRuntimeImplementation(*this).RemoveRagdollFromSimulation(worldHandle, ragdollHandle);
    }

    bool Ragdolls::DestroyRagdoll(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle)
    {
        return GetRuntimeImplementation(*this).DestroyRagdoll(worldHandle, ragdollHandle);
    }

    bool Ragdolls::IsValid(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, ragdollHandle);
    }

    bool Ragdolls::IsRagdollInSimulation(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle) const
    {
        return GetRuntimeImplementation(*this).IsRagdollInSimulation(worldHandle, ragdollHandle);
    }

    bool Ragdolls::GetRagdollState(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        RagdollState& state) const
    {
        return GetRuntimeImplementation(*this).GetRagdollState(worldHandle, ragdollHandle, state);
    }

    bool Ragdolls::SetRagdollCollisionGroupId(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZ::u32 collisionGroupId)
    {
        return GetRuntimeImplementation(*this).SetRagdollCollisionGroupId(worldHandle, ragdollHandle, collisionGroupId);
    }

    QueryResult Ragdolls::GetRagdollBodies(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZStd::span<BodyHandle> bodyHandles) const
    {
        return GetRuntimeImplementation(*this).GetRagdollBodies(worldHandle, ragdollHandle, bodyHandles);
    }

    QueryResult Ragdolls::GetRagdollConstraints(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZStd::span<ConstraintHandle> constraintHandles) const
    {
        return GetRuntimeImplementation(*this).GetRagdollConstraints(worldHandle, ragdollHandle, constraintHandles);
    }

    bool Ragdolls::ActivateRagdoll(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle)
    {
        return GetRuntimeImplementation(*this).ActivateRagdoll(worldHandle, ragdollHandle);
    }

    bool Ragdolls::SetRagdollPose(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        WorldPosition rootPosition,
        AZStd::span<const AZ::Transform> modelTransforms)
    {
        return GetRuntimeImplementation(*this).SetRagdollPose(worldHandle, ragdollHandle, rootPosition, modelTransforms);
    }

    QueryResult Ragdolls::GetRagdollPose(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        WorldPosition& rootPosition,
        AZStd::span<AZ::Transform> modelTransforms) const
    {
        return GetRuntimeImplementation(*this).GetRagdollPose(worldHandle, ragdollHandle, rootPosition, modelTransforms);
    }

    bool Ragdolls::DriveRagdollKinematically(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        WorldPosition rootPosition,
        AZStd::span<const AZ::Transform> modelTransforms,
        float deltaTime)
    {
        return GetRuntimeImplementation(*this).DriveRagdollKinematically(worldHandle, ragdollHandle, rootPosition, modelTransforms, deltaTime);
    }

    bool Ragdolls::DriveRagdollMotors(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZStd::span<const AZ::Transform> modelTransforms)
    {
        return GetRuntimeImplementation(*this).DriveRagdollMotors(worldHandle, ragdollHandle, modelTransforms);
    }

    bool Ragdolls::DriveRagdollMotors(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZStd::span<const AZ::Transform> previousModelTransforms,
        AZStd::span<const AZ::Transform> modelTransforms,
        float deltaTime)
    {
        return GetRuntimeImplementation(*this).DriveRagdollMotors(worldHandle, ragdollHandle, previousModelTransforms, modelTransforms, deltaTime);
    }

    bool Ragdolls::ResetRagdollWarmStart(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle)
    {
        return GetRuntimeImplementation(*this).ResetRagdollWarmStart(worldHandle, ragdollHandle);
    }

    bool Ragdolls::SetRagdollVelocity(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZ::Vector3 linearVelocity,
        AZ::Vector3 angularVelocity)
    {
        return GetRuntimeImplementation(*this).SetRagdollVelocity(worldHandle, ragdollHandle, linearVelocity, angularVelocity);
    }

    bool Ragdolls::SetRagdollLinearVelocity(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZ::Vector3 linearVelocity)
    {
        return GetRuntimeImplementation(*this).SetRagdollLinearVelocity(worldHandle, ragdollHandle, linearVelocity);
    }

    bool Ragdolls::AddRagdollLinearVelocity(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZ::Vector3 linearVelocity)
    {
        return GetRuntimeImplementation(*this).AddRagdollLinearVelocity(worldHandle, ragdollHandle, linearVelocity);
    }

    bool Ragdolls::AddRagdollImpulse(
        WorldHandle worldHandle,
        RagdollHandle ragdollHandle,
        AZ::Vector3 impulse)
    {
        return GetRuntimeImplementation(*this).AddRagdollImpulse(worldHandle, ragdollHandle, impulse);
    }

    AZStd::atomic<SoftBodies*> SoftBodies::s_instance;

    SoftBodies* SoftBodies::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    SoftBodyDefinitionHandle SoftBodies::CreateSoftBodyDefinition(
        const SoftBodyDefinitionConfiguration& configuration,
        SoftBodyOptimizationRemap* optimizationRemap)
    {
        return GetRuntimeImplementation(*this).CreateSoftBodyDefinition(configuration, optimizationRemap);
    }

    bool SoftBodies::ExportSoftBodyDefinition(
        SoftBodyDefinitionHandle definitionHandle,
        SoftBodyDefinitionArchive& archive,
        AZStd::vector<MaterialHandle>& materialHandles) const
    {
        return GetRuntimeImplementation(*this).ExportSoftBodyDefinition(definitionHandle, archive, materialHandles);
    }

    SoftBodyDefinitionHandle SoftBodies::ImportSoftBodyDefinition(
        const SoftBodyDefinitionArchive& archive,
        AZStd::span<const MaterialHandle> materialHandles)
    {
        return GetRuntimeImplementation(*this).ImportSoftBodyDefinition(archive, materialHandles);
    }

    bool SoftBodies::DestroySoftBodyDefinition(SoftBodyDefinitionHandle definitionHandle)
    {
        return GetRuntimeImplementation(*this).DestroySoftBodyDefinition(definitionHandle);
    }

    bool SoftBodies::IsValid(SoftBodyDefinitionHandle definitionHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(definitionHandle);
    }

    bool SoftBodies::GetSoftBodyDefinitionState(
        SoftBodyDefinitionHandle definitionHandle,
        SoftBodyDefinitionState& state) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionState(definitionHandle, state);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionDihedralBendConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyDihedralBendConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionDihedralBendConstraints(definitionHandle, constraints);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionEdgeConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyEdgeConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionEdgeConstraints(definitionHandle, constraints);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionFaces(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyFace> faces) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionFaces(definitionHandle, faces);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionInverseBinds(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyInverseBind> inverseBinds) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionInverseBinds(definitionHandle, inverseBinds);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionLongRangeConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyLongRangeConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionLongRangeConstraints(definitionHandle, constraints);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionMaterials(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<MaterialHandle> materials) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionMaterials(definitionHandle, materials);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionRodBendTwistConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionRodBendTwistConstraints(definitionHandle, constraints);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionRodStretchShearConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionRodStretchShearConstraints(definitionHandle, constraints);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionSkinConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodySkinConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionSkinConstraints(definitionHandle, constraints);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionVertices(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyVertex> vertices) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionVertices(definitionHandle, vertices);
    }

    QueryResult SoftBodies::GetSoftBodyDefinitionVolumeConstraints(
        SoftBodyDefinitionHandle definitionHandle,
        AZStd::span<SoftBodyVolumeConstraint> constraints) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyDefinitionVolumeConstraints(definitionHandle, constraints);
    }

    BodyHandle SoftBodies::CreateSoftBodyWithId(
        WorldHandle worldHandle,
        BodyId bodyId,
        const SoftBodyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateSoftBodyWithId(worldHandle, bodyId, configuration);
    }

    bool Bodies::AddBodyToSimulation(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool activate)
    {
        return GetRuntimeImplementation(*this).AddBodyToSimulation(worldHandle, bodyHandle, activate);
    }

    bool Bodies::GetBodyUserData(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u64& userData) const
    {
        return GetRuntimeImplementation(*this).GetBodyUserData(worldHandle, bodyHandle, userData);
    }

    bool Bodies::SetBodyUserData(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u64 userData)
    {
        return GetRuntimeImplementation(*this).SetBodyUserData(worldHandle, bodyHandle, userData);
    }

    bool Bodies::GetBodyRuntimeConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        BodyRuntimeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetBodyRuntimeConfiguration(worldHandle, bodyHandle, configuration);
    }

    bool Bodies::GetBodySimulationStatistics(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        BodySimulationStatistics& statistics) const
    {
        return GetRuntimeImplementation(*this).GetBodySimulationStatistics(worldHandle, bodyHandle, statistics);
    }

    bool Bodies::ApplyBodyConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const BodyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).ApplyBodyConfiguration(worldHandle, bodyHandle, configuration);
    }

    QueryResult SoftBodies::GetSoftBodyFaces(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZStd::span<SoftBodyFace> faces) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyFaces(worldHandle, bodyHandle, faces);
    }

    bool SoftBodies::GetSoftBodyLocalBounds(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::Aabb& bounds) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyLocalBounds(worldHandle, bodyHandle, bounds);
    }

    QueryResult SoftBodies::GetSoftBodyMaterials(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZStd::span<MaterialHandle> materials) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyMaterials(worldHandle, bodyHandle, materials);
    }

    QueryResult SoftBodies::GetSoftBodyRodStates(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZStd::span<SoftBodyRodState> rods) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyRodStates(worldHandle, bodyHandle, rods);
    }

    bool SoftBodies::GetSoftBodyRuntimeConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        SoftBodyRuntimeConfiguration& configuration) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyRuntimeConfiguration(worldHandle, bodyHandle, configuration);
    }

    bool SoftBodies::ApplySoftBodyConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const SoftBodyConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).ApplySoftBodyConfiguration(worldHandle, bodyHandle, configuration);
    }

    QueryResult SoftBodies::GetSoftBodyVertices(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZStd::span<SoftBodyVertex> vertices) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyVertices(worldHandle, bodyHandle, vertices);
    }

    bool SoftBodies::GetSoftBodyVolume(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float& volume) const
    {
        return GetRuntimeImplementation(*this).GetSoftBodyVolume(worldHandle, bodyHandle, volume);
    }

    bool SoftBodies::RecalculateSoftBodyMassProperties(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        bool activate)
    {
        return GetRuntimeImplementation(*this).RecalculateSoftBodyMassProperties(worldHandle, bodyHandle, activate);
    }

    bool SoftBodies::SkinSoftBody(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
        bool hardSkinAll)
    {
        return GetRuntimeImplementation(*this).SkinSoftBody(worldHandle, bodyHandle, jointTransformsRelativeToCenterOfMass, hardSkinAll);
    }

    bool SoftBodies::UpdateSoftBodyManually(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        float deltaTime)
    {
        return GetRuntimeImplementation(*this).UpdateSoftBodyManually(worldHandle, bodyHandle, deltaTime);
    }

    bool SoftBodies::UpdateSoftBodyRuntimeConfiguration(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        const SoftBodyRuntimeConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).UpdateSoftBodyRuntimeConfiguration(worldHandle, bodyHandle, configuration);
    }

    bool SoftBodies::SetSoftBodyVertexInverseMass(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u32 vertexIndex,
        float inverseMass)
    {
        return GetRuntimeImplementation(*this).SetSoftBodyVertexInverseMass(worldHandle, bodyHandle, vertexIndex, inverseMass);
    }

    bool SoftBodies::SetSoftBodyVertexInverseMasses(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u32 startVertexIndex,
        AZStd::span<const float> inverseMasses)
    {
        return GetRuntimeImplementation(*this).SetSoftBodyVertexInverseMasses(worldHandle, bodyHandle, startVertexIndex, inverseMasses);
    }

    bool SoftBodies::SetSoftBodyVertexVelocity(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u32 vertexIndex,
        const AZ::Vector3& velocity)
    {
        return GetRuntimeImplementation(*this).SetSoftBodyVertexVelocity(worldHandle, bodyHandle, vertexIndex, velocity);
    }

    bool SoftBodies::SetSoftBodyVertexVelocities(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        AZ::u32 startVertexIndex,
        AZStd::span<const AZ::Vector3> velocities)
    {
        return GetRuntimeImplementation(*this).SetSoftBodyVertexVelocities(worldHandle, bodyHandle, startVertexIndex, velocities);
    }

    AZStd::atomic<Hair*> Hair::s_instance;

    Hair* Hair::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    HairDefinitionHandle Hair::CreateHairDefinition(const HairDefinitionConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateHairDefinition(configuration);
    }

    bool Hair::DestroyHairDefinition(HairDefinitionHandle definitionHandle)
    {
        return GetRuntimeImplementation(*this).DestroyHairDefinition(definitionHandle);
    }

    bool Hair::IsValid(HairDefinitionHandle definitionHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(definitionHandle);
    }

    bool Hair::GetHairDefinitionState(
        HairDefinitionHandle definitionHandle,
        HairDefinitionState& state) const
    {
        return GetRuntimeImplementation(*this).GetHairDefinitionState(definitionHandle, state);
    }

    QueryResult Hair::GetHairNeutralDensity(
        HairDefinitionHandle definitionHandle,
        AZStd::span<float> density) const
    {
        return GetRuntimeImplementation(*this).GetHairNeutralDensity(definitionHandle, density);
    }

    bool Hair::SkinHairScalpVertices(
        HairDefinitionHandle definitionHandle,
        const AZ::Transform& jointToHair,
        AZStd::span<const AZ::Transform> jointModelTransforms,
        AZStd::span<AZ::Transform> preparedJointTransforms,
        AZStd::span<AZ::Vector3> scalpVertices) const
    {
        return GetRuntimeImplementation(*this).SkinHairScalpVertices(definitionHandle, jointToHair, jointModelTransforms, preparedJointTransforms, scalpVertices);
    }

    HairHandle Hair::CreateHair(
        WorldHandle worldHandle,
        const HairConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).CreateHair(worldHandle, configuration);
    }

    bool Hair::DestroyHair(
        WorldHandle worldHandle,
        HairHandle hairHandle)
    {
        return GetRuntimeImplementation(*this).DestroyHair(worldHandle, hairHandle);
    }

    bool Hair::IsValid(
        WorldHandle worldHandle,
        HairHandle hairHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, hairHandle);
    }

    bool Hair::SetHairTransform(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        const WorldTransform& worldTransform,
        bool teleport)
    {
        return GetRuntimeImplementation(*this).SetHairTransform(worldHandle, hairHandle, worldTransform, teleport);
    }

    bool Hair::SetHairScalpToHeadTransform(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        const AZ::Transform& scalpToHeadTransform)
    {
        return GetRuntimeImplementation(*this).SetHairScalpToHeadTransform(worldHandle, hairHandle, scalpToHeadTransform);
    }

    bool Hair::UpdateHair(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        float deltaTime,
        const AZ::Transform& jointToHair,
        AZStd::span<const AZ::Transform> jointModelTransforms)
    {
        return GetRuntimeImplementation(*this).UpdateHair(worldHandle, hairHandle, deltaTime, jointToHair, jointModelTransforms);
    }

    bool Hair::EnableHairAutoUpdate(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        const AZ::Transform& jointToHair,
        AZStd::span<const AZ::Transform> jointModelTransforms)
    {
        return GetRuntimeImplementation(*this).EnableHairAutoUpdate(worldHandle, hairHandle, jointToHair, jointModelTransforms);
    }

    bool Hair::DisableHairAutoUpdate(
        WorldHandle worldHandle,
        HairHandle hairHandle)
    {
        return GetRuntimeImplementation(*this).DisableHairAutoUpdate(worldHandle, hairHandle);
    }

    bool Hair::GetHairState(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        HairState& state) const
    {
        return GetRuntimeImplementation(*this).GetHairState(worldHandle, hairHandle, state);
    }

    bool Hair::GetHairReadback(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        const HairReadbackBuffers& buffers,
        HairReadbackResult& result) const
    {
        return GetRuntimeImplementation(*this).GetHairReadback(worldHandle, hairHandle, buffers, result);
    }

    QueryResult Hair::GetHairVertexStates(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        AZStd::span<HairVertexState> states) const
    {
        return GetRuntimeImplementation(*this).GetHairVertexStates(worldHandle, hairHandle, states);
    }

    QueryResult Hair::GetHairRenderPositions(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        AZStd::span<AZ::Vector3> positions) const
    {
        return GetRuntimeImplementation(*this).GetHairRenderPositions(worldHandle, hairHandle, positions);
    }

    QueryResult Hair::GetHairScalpPositions(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        AZStd::span<AZ::Vector3> positions) const
    {
        return GetRuntimeImplementation(*this).GetHairScalpPositions(worldHandle, hairHandle, positions);
    }

    QueryResult Hair::GetHairGridCellStates(
        WorldHandle worldHandle,
        HairHandle hairHandle,
        AZStd::span<HairGridCellState> states) const
    {
        return GetRuntimeImplementation(*this).GetHairGridCellStates(worldHandle, hairHandle, states);
    }

    AZStd::atomic<Rollback*> Rollback::s_instance;

    Rollback* Rollback::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    StateSnapshotHandle Rollback::CaptureBodyState(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).CaptureBodyState(worldHandle, bodyHandle);
    }

    Operation<StateSnapshotHandle> Rollback::CaptureBodyStateAsync(
        WorldHandle worldHandle,
        BodyHandle bodyHandle)
    {
        return GetRuntimeImplementation(*this).CaptureBodyStateAsync(worldHandle, bodyHandle);
    }

    bool Rollback::CaptureBodyState(
        WorldHandle worldHandle,
        BodyHandle bodyHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).CaptureBodyState(worldHandle, bodyHandle, snapshotHandle);
    }

    StateRestoreResult Rollback::RestoreBodyState(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).RestoreBodyState(worldHandle, snapshotHandle);
    }

    Operation<StateRestoreResult> Rollback::RestoreBodyStateAsync(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).RestoreBodyStateAsync(worldHandle, snapshotHandle);
    }

    StateSnapshotHandle Rollback::CaptureWorldState(WorldHandle worldHandle)
    {
        return GetRuntimeImplementation(*this).CaptureWorldState(worldHandle);
    }

    Operation<StateSnapshotHandle> Rollback::CaptureWorldStateAsync(WorldHandle worldHandle)
    {
        return GetRuntimeImplementation(*this).CaptureWorldStateAsync(worldHandle);
    }

    bool Rollback::CaptureWorldState(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).CaptureWorldState(worldHandle, snapshotHandle);
    }

    StateSnapshotHandle Rollback::CaptureWorldState(
        WorldHandle worldHandle,
        const StateSnapshotConfiguration& configuration,
        AZStd::span<const BodyHandle> bodyHandles)
    {
        return GetRuntimeImplementation(*this).CaptureWorldState(worldHandle, configuration, bodyHandles);
    }

    bool Rollback::CaptureWorldState(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle,
        const StateSnapshotConfiguration& configuration,
        AZStd::span<const BodyHandle> bodyHandles)
    {
        return GetRuntimeImplementation(*this).CaptureWorldState(worldHandle, snapshotHandle, configuration, bodyHandles);
    }

    bool Rollback::CaptureWorldStateParts(
        WorldHandle worldHandle,
        const StateSnapshotConfiguration& configuration,
        AZStd::span<const BodyHandle> bodyHandles,
        AZStd::span<const AZ::u32> partitionBodyCounts,
        AZStd::span<StateSnapshotHandle> snapshotHandles)
    {
        return GetRuntimeImplementation(*this).CaptureWorldStateParts(worldHandle, configuration, bodyHandles, partitionBodyCounts, snapshotHandles);
    }

    bool Rollback::ExportWorldStateArchive(
        WorldHandle worldHandle,
        AZStd::span<const StateSnapshotHandle> snapshotHandles,
        StateSnapshotArchive& archive)
    {
        return GetRuntimeImplementation(*this).ExportWorldStateArchive(worldHandle, snapshotHandles, archive);
    }

    bool Rollback::ImportWorldStateArchive(
        WorldHandle worldHandle,
        const StateSnapshotArchive& archive,
        AZStd::span<StateSnapshotHandle> snapshotHandles)
    {
        return GetRuntimeImplementation(*this).ImportWorldStateArchive(worldHandle, archive, snapshotHandles);
    }

    bool Rollback::DestroyStateSnapshot(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).DestroyStateSnapshot(worldHandle, snapshotHandle);
    }

    bool Rollback::IsValid(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle) const
    {
        return GetRuntimeImplementation(*this).IsValid(worldHandle, snapshotHandle);
    }

    StateRestoreResult Rollback::RestoreWorldState(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).RestoreWorldState(worldHandle, snapshotHandle);
    }

    Operation<StateRestoreResult> Rollback::RestoreWorldStateAsync(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle)
    {
        return GetRuntimeImplementation(*this).RestoreWorldStateAsync(worldHandle, snapshotHandle);
    }

    StateRestoreResult Rollback::RestoreWorldStateParts(
        WorldHandle worldHandle,
        AZStd::span<const StateSnapshotHandle> snapshotHandles)
    {
        return GetRuntimeImplementation(*this).RestoreWorldStateParts(worldHandle, snapshotHandles);
    }

    bool Rollback::ValidateWorldState(
        WorldHandle worldHandle,
        StateSnapshotHandle snapshotHandle,
        StateValidationResult& result)
    {
        return GetRuntimeImplementation(*this).ValidateWorldState(worldHandle, snapshotHandle, result);
    }

    bool Rollback::GetWorldStateDigest(
        WorldHandle worldHandle,
        WorldStateDigest& digest) const
    {
        return GetRuntimeImplementation(*this).GetWorldStateDigest(worldHandle, digest);
    }

    AZStd::atomic<Diagnostics*> Diagnostics::s_instance;

    Diagnostics* Diagnostics::Get()
    {
        return s_instance.load(AZStd::memory_order_acquire);
    }

    bool Diagnostics::GetWorldStatistics(
        WorldHandle worldHandle,
        WorldStatistics& statistics) const
    {
        return GetRuntimeImplementation(*this).GetWorldStatistics(worldHandle, statistics);
    }

    bool Diagnostics::ConfigurePerformanceStatistics(
        WorldHandle worldHandle,
        PerformanceStatisticsFlags flags)
    {
        return GetRuntimeImplementation(*this).ConfigurePerformanceStatistics(worldHandle, flags);
    }

    bool Diagnostics::GetPerformanceStatistics(
        WorldHandle worldHandle,
        WorldPerformanceStatistics& statistics,
        bool reset)
    {
        return GetRuntimeImplementation(*this).GetPerformanceStatistics(worldHandle, statistics, reset);
    }

    DiagnosticStatisticsResult Diagnostics::GetBroadPhaseStatistics(
        WorldHandle worldHandle,
        AZStd::span<BroadPhaseStatistics> statistics,
        bool reset)
    {
        return GetRuntimeImplementation(*this).GetBroadPhaseStatistics(worldHandle, statistics, reset);
    }

    DiagnosticStatisticsResult Diagnostics::GetNarrowPhaseStatistics(
        AZStd::span<NarrowPhaseStatistics> statistics,
        bool reset)
    {
        return GetRuntimeImplementation(*this).GetNarrowPhaseStatistics(statistics, reset);
    }

    bool Diagnostics::DrawDebug(
        WorldHandle worldHandle,
        const DebugDrawSettings& settings,
        IDebugRenderer& renderer,
        const IDebugFilter* filter)
    {
        return GetRuntimeImplementation(*this).DrawDebug(worldHandle, settings, renderer, filter);
    }

    bool Diagnostics::ConfigureDebugCapture(
        WorldHandle worldHandle,
        const DebugCaptureConfiguration& configuration)
    {
        return GetRuntimeImplementation(*this).ConfigureDebugCapture(worldHandle, configuration);
    }

    bool Diagnostics::GetDebugCaptureStatistics(
        WorldHandle worldHandle,
        DebugCaptureStatistics& statistics) const
    {
        return GetRuntimeImplementation(*this).GetDebugCaptureStatistics(worldHandle, statistics);
    }
} // namespace Jolt
