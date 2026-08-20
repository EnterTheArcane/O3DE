/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Skeleton.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct SkeletonJointSource final
    {
        AZ_TYPE_INFO(SkeletonJointSource, SkeletonJointSourceTypeId);

        AZStd::string m_name;
        AZ::s32 m_parentIndex = -1;
    };

    struct SkeletonDefinitionSource final
    {
        AZ_TYPE_INFO(SkeletonDefinitionSource, SkeletonDefinitionSourceTypeId);

        AZStd::vector<SkeletonJointSource> m_joints;
    };

    struct SkeletalAnimatedJointSource final
    {
        AZ_TYPE_INFO(SkeletalAnimatedJointSource, SkeletalAnimatedJointSourceTypeId);

        AZStd::vector<SkeletalAnimationKeyframe> m_keyframes;
        AZStd::string m_name;
    };

    struct SkeletalAnimationSource final
    {
        AZ_TYPE_INFO(SkeletalAnimationSource, SkeletalAnimationSourceTypeId);

        AZStd::vector<SkeletalAnimatedJointSource> m_joints;
        bool m_isLooping = true;
    };

    struct NamedSkeletalAnimationSource final
    {
        AZ_TYPE_INFO(NamedSkeletalAnimationSource, NamedSkeletalAnimationSourceTypeId);

        SkeletalAnimationSource m_configuration;
        AZStd::string m_name;
    };

    struct NamedSkeletalAnimationAsset final
    {
        AZ_TYPE_INFO(NamedSkeletalAnimationAsset, NamedSkeletalAnimationAssetTypeId);

        SkeletalAnimationSource m_source;

        SkeletalAnimationArchive m_archive;
        AZ::Name m_name;
    };

    struct SkeletonSourceData final
    {
        AZ_TYPE_INFO(SkeletonSourceData, SkeletonSourceDataTypeId);

        SkeletonDefinitionSource m_skeleton;
        AZStd::vector<NamedSkeletalAnimationSource> m_animations;
        AZStd::string m_name;
    };

    struct SkeletonAssetData final
    {
        AZ_TYPE_INFO(SkeletonAssetData, SkeletonAssetDataTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        SkeletonDefinitionSource m_sourceSkeleton;
        AZStd::vector<NamedSkeletalAnimationAsset> m_animations;
        AZ::Name m_name;

        SkeletonDefinitionArchive m_skeleton;
        AZStd::string m_nativeCachePlatform;
        AZ::u64 m_nativeCacheBuildFingerprint = 0;
    };

    class JOLT_API SkeletonAsset final
        : public AZ::Data::AssetData
    {
    public:
        AZ_CLASS_ALLOCATOR(SkeletonAsset, AZ::SystemAllocator);
        AZ_RTTI(SkeletonAsset, SkeletonAssetTypeId, AZ::Data::AssetData);

        explicit SkeletonAsset(
            const AZ::Data::AssetId& assetId = {},
            AZ::Data::AssetData::AssetStatus status = AZ::Data::AssetData::AssetStatus::NotLoaded);

        static void Reflect(AZ::ReflectContext* context);

        SkeletonAssetData m_data;
    };
} // namespace Jolt
