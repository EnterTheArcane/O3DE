/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Skeleton.h>

#include <AzCore/EBus/EBus.h>

namespace Jolt
{
    class ISkeletonRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual SkeletonDefinitionHandle CreateSkeletonDefinition(
            const SkeletonDefinitionConfiguration& configuration) = 0;

        virtual bool DestroySkeletonDefinition(SkeletonDefinitionHandle skeletonHandle) = 0;

        [[nodiscard]]
        virtual bool IsSkeletonDefinitionValid(SkeletonDefinitionHandle skeletonHandle) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SkeletonJoint> CopySkeletonJoints(
            SkeletonDefinitionHandle skeletonHandle) const = 0;

        [[nodiscard]]
        virtual AZ::s32 FindSkeletonJoint(
            SkeletonDefinitionHandle skeletonHandle,
            AZ::Name jointName) const = 0;

        virtual SkeletalAnimationHandle CreateSkeletalAnimation(
            const SkeletalAnimationConfiguration& configuration) = 0;

        virtual bool UpdateSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            const SkeletalAnimationConfiguration& configuration) = 0;

        virtual bool DestroySkeletalAnimation(SkeletalAnimationHandle animationHandle) = 0;

        [[nodiscard]]
        virtual bool IsSkeletalAnimationValid(SkeletalAnimationHandle animationHandle) const = 0;

        virtual bool GetSkeletalAnimationState(
            SkeletalAnimationHandle animationHandle,
            SkeletalAnimationState& state) const = 0;

        [[nodiscard]]
        virtual AZ::Name GetSkeletalAnimatedJointName(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SkeletalAnimationKeyframe> CopySkeletalAnimationKeyframes(
            SkeletalAnimationHandle animationHandle,
            AZ::u32 jointIndex) const = 0;

        virtual bool SetSkeletalAnimationLooping(
            SkeletalAnimationHandle animationHandle,
            bool isLooping) = 0;

        virtual bool ScaleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            float scale) = 0;

        virtual SkeletonPoseHandle CreateSkeletonPose(SkeletonDefinitionHandle skeletonHandle) = 0;

        virtual bool DestroySkeletonPose(SkeletonPoseHandle poseHandle) = 0;

        [[nodiscard]]
        virtual bool IsSkeletonPoseValid(SkeletonPoseHandle poseHandle) const = 0;

        virtual bool GetSkeletonPoseState(
            SkeletonPoseHandle poseHandle,
            SkeletonPoseState& state) const = 0;

        virtual bool SetSkeletonPoseRootOffset(
            SkeletonPoseHandle poseHandle,
            const WorldPosition& rootOffset) = 0;

        virtual bool SetSkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle,
            const AZStd::vector<AZ::Transform>& localTransforms) = 0;

        virtual bool SetSkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle,
            const AZStd::vector<AZ::Transform>& modelTransforms) = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Transform> CopySkeletonPoseLocalTransforms(
            SkeletonPoseHandle poseHandle) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Transform> CopySkeletonPoseModelTransforms(
            SkeletonPoseHandle poseHandle) const = 0;

        virtual bool SampleSkeletalAnimation(
            SkeletalAnimationHandle animationHandle,
            SkeletonPoseHandle poseHandle,
            float time) = 0;

        virtual SkeletonMapperHandle CreateSkeletonMapper(
            const SkeletonMapperConfiguration& configuration) = 0;

        virtual bool DestroySkeletonMapper(SkeletonMapperHandle mapperHandle) = 0;

        [[nodiscard]]
        virtual bool IsSkeletonMapperValid(SkeletonMapperHandle mapperHandle) const = 0;

        virtual bool GetSkeletonMapperState(
            SkeletonMapperHandle mapperHandle,
            SkeletonMapperState& state) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SkeletonMapperMappingState> CopySkeletonMapperMappings(
            SkeletonMapperHandle mapperHandle) const = 0;

        virtual bool GetSkeletonMapperChainState(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex,
            SkeletonMapperChainState& state) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::u32> CopySkeletonMapperSourceChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::u32> CopySkeletonMapperTargetChain(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 chainIndex) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SkeletonMapperUnmappedJoint> CopySkeletonMapperUnmappedJoints(
            SkeletonMapperHandle mapperHandle) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SkeletonMapperLockedTranslation> CopySkeletonMapperLockedTranslations(
            SkeletonMapperHandle mapperHandle) const = 0;

        [[nodiscard]]
        virtual AZ::s32 FindMappedSkeletonJoint(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 sourceJointIndex) const = 0;

        [[nodiscard]]
        virtual bool IsSkeletonJointTranslationLocked(
            SkeletonMapperHandle mapperHandle,
            AZ::u32 targetJointIndex) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Transform> MapSkeletonPose(
            SkeletonMapperHandle mapperHandle,
            const AZStd::vector<AZ::Transform>& sourceModelTransforms,
            const AZStd::vector<AZ::Transform>& targetLocalTransforms) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Transform> MapSkeletonPoseReverse(
            SkeletonMapperHandle mapperHandle,
            const AZStd::vector<AZ::Transform>& targetModelTransforms) const = 0;
    };

    using SkeletonRequestBus = AZ::EBus<ISkeletonRequests>;
} // namespace Jolt
