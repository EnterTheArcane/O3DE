/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct SkeletonJoint final
    {
        AZ_TYPE_INFO(SkeletonJoint, SkeletonJointTypeId);

        AZ::Name m_name;
        AZ::s32 m_parentIndex = -1;
    };

    struct SkeletonDefinitionConfiguration final
    {
        AZ_TYPE_INFO(SkeletonDefinitionConfiguration, SkeletonDefinitionConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<SkeletonJoint> m_joints;
    };

    struct SkeletonDefinitionArchive final
    {
        AZ_TYPE_INFO(SkeletonDefinitionArchive, SkeletonDefinitionArchiveTypeId);

        AZStd::vector<AZ::u8> m_binaryState;
        AZ::u64 m_buildFingerprint = 0;
        AZ::u64 m_contentHash = 0;
        AZ::u32 m_formatVersion = 0;
        AZ::u32 m_jointCount = 0;
    };

    struct SkeletonJointMapping final
    {
        AZ_TYPE_INFO(SkeletonJointMapping, SkeletonJointMappingTypeId);

        AZ::u32 m_sourceJoint = 0;
        AZ::u32 m_targetJoint = 0;
    };

    struct SkeletonMapperConfiguration final
    {
        AZ_TYPE_INFO(SkeletonMapperConfiguration, SkeletonMapperConfigurationTypeId);

        SkeletonDefinitionHandle m_sourceSkeletonHandle;
        SkeletonDefinitionHandle m_targetSkeletonHandle;
        AZStd::vector<AZ::Transform> m_sourceNeutralModelTransforms;
        AZStd::vector<AZ::Transform> m_targetNeutralModelTransforms;
        AZStd::vector<SkeletonJointMapping> m_jointMappings;
        AZStd::vector<AZ::u8> m_lockedTargetTranslations;
        bool m_lockAllTargetTranslations = false;
    };

    struct SkeletonMapperState final
    {
        AZ_TYPE_INFO(SkeletonMapperState, SkeletonMapperStateTypeId);

        SkeletonDefinitionHandle m_sourceSkeletonHandle;
        SkeletonDefinitionHandle m_targetSkeletonHandle;
        AZ::u32 m_chainCount = 0;
        AZ::u32 m_lockedTranslationCount = 0;
        AZ::u32 m_mappingCount = 0;
        AZ::u32 m_sourceJointCount = 0;
        AZ::u32 m_targetJointCount = 0;
        AZ::u32 m_unmappedJointCount = 0;
    };

    struct SkeletonMapperMappingState final
    {
        AZ_TYPE_INFO(SkeletonMapperMappingState, SkeletonMapperMappingStateTypeId);

        AZ::Transform m_sourceToTarget = AZ::Transform::CreateIdentity();
        AZ::Transform m_targetToSource = AZ::Transform::CreateIdentity();
        AZ::u32 m_sourceJointIndex = 0;
        AZ::u32 m_targetJointIndex = 0;
    };

    struct SkeletonMapperChainState final
    {
        AZ_TYPE_INFO(SkeletonMapperChainState, SkeletonMapperChainStateTypeId);

        AZ::u32 m_sourceJointCount = 0;
        AZ::u32 m_targetJointCount = 0;
    };

    struct SkeletonMapperUnmappedJoint final
    {
        AZ_TYPE_INFO(SkeletonMapperUnmappedJoint, SkeletonMapperUnmappedJointTypeId);

        AZ::s32 m_jointIndex = -1;
        AZ::s32 m_parentJointIndex = -1;
    };

    struct SkeletonMapperLockedTranslation final
    {
        AZ_TYPE_INFO(SkeletonMapperLockedTranslation, SkeletonMapperLockedTranslationTypeId);

        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        AZ::s32 m_jointIndex = -1;
        AZ::s32 m_parentJointIndex = -1;
    };

    struct SkeletalAnimationKeyframe final
    {
        AZ_TYPE_INFO(SkeletalAnimationKeyframe, SkeletalAnimationKeyframeTypeId);

        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        float m_time = 0.0f;
    };

    struct SkeletalAnimatedJoint final
    {
        AZ_TYPE_INFO(SkeletalAnimatedJoint, SkeletalAnimatedJointTypeId);

        AZ::Name m_name;
        AZStd::vector<SkeletalAnimationKeyframe> m_keyframes;
    };

    struct SkeletalAnimationConfiguration final
    {
        AZ_TYPE_INFO(SkeletalAnimationConfiguration, SkeletalAnimationConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<SkeletalAnimatedJoint> m_joints;
        bool m_isLooping = true;
    };

    struct SkeletalAnimationArchive final
    {
        AZ_TYPE_INFO(SkeletalAnimationArchive, SkeletalAnimationArchiveTypeId);

        AZStd::vector<AZ::u8> m_binaryState;
        AZ::u64 m_buildFingerprint = 0;
        AZ::u64 m_contentHash = 0;
        AZ::u32 m_formatVersion = 0;
        AZ::u32 m_jointCount = 0;
    };

    struct SkeletalAnimationState final
    {
        AZ_TYPE_INFO(SkeletalAnimationState, SkeletalAnimationStateTypeId);

        float m_duration = 0.0f;
        AZ::u32 m_jointCount = 0;
        bool m_isLooping = false;
    };

    struct SkeletonPoseState final
    {
        AZ_TYPE_INFO(SkeletonPoseState, SkeletonPoseStateTypeId);

        WorldPosition m_rootOffset;
        SkeletonDefinitionHandle m_skeletonHandle;
        AZ::u32 m_jointCount = 0;
    };
} // namespace Jolt
