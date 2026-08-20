/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Query.h>
#include <Jolt/Skeleton.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;
    struct SkeletalAnimationSource;
    struct SkeletonDefinitionSource;

    class JOLT_API Skeletons
    {
    public:
        [[nodiscard]]
        static Skeletons* Get();

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

    private:
        friend class Runtime;

        Skeletons() = default;
        ~Skeletons() = default;

        static AZStd::atomic<Skeletons*> s_instance;
    };
} // namespace Jolt
