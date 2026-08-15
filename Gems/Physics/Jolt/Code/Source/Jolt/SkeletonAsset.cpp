/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SkeletonAsset.h>

#include <Jolt/Reflection.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void SkeletonAssetData::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SkeletonAssetData>(*serializeContext))
            {
                return;
            }

            SkeletalAnimationConfiguration::Reflect(context);

            serializeContext
                ->Class<SkeletonJointSource>()
                ->Field("Name", &SkeletonJointSource::m_name)
                ->Field("ParentIndex", &SkeletonJointSource::m_parentIndex);

            serializeContext
                ->Class<SkeletonDefinitionSource>()
                ->Field("Joints", &SkeletonDefinitionSource::m_joints);

            serializeContext
                ->Class<SkeletalAnimatedJointSource>()
                ->Field("Keyframes", &SkeletalAnimatedJointSource::m_keyframes)
                ->Field("Name", &SkeletalAnimatedJointSource::m_name);

            serializeContext
                ->Class<SkeletalAnimationSource>()
                ->Field("Joints", &SkeletalAnimationSource::m_joints)
                ->Field("IsLooping", &SkeletalAnimationSource::m_isLooping);

            serializeContext
                ->Class<NamedSkeletalAnimationSource>()
                ->Field("Configuration", &NamedSkeletalAnimationSource::m_configuration)
                ->Field("Name", &NamedSkeletalAnimationSource::m_name);

            serializeContext
                ->Class<NamedSkeletalAnimationAsset>()
                ->Field("Archive", &NamedSkeletalAnimationAsset::m_archive)
                ->Field("Name", &NamedSkeletalAnimationAsset::m_name);

            serializeContext
                ->Class<SkeletonSourceData>()
                ->Field("Skeleton", &SkeletonSourceData::m_skeleton)
                ->Field("Animations", &SkeletonSourceData::m_animations)
                ->Field("Name", &SkeletonSourceData::m_name);

            serializeContext
                ->Class<SkeletonAssetData>()
                ->Field("Skeleton", &SkeletonAssetData::m_skeleton)
                ->Field("Animations", &SkeletonAssetData::m_animations)
                ->Field("Name", &SkeletonAssetData::m_name);
        }
    }

    SkeletonAsset::SkeletonAsset(
        const AZ::Data::AssetId& assetId,
        const AZ::Data::AssetData::AssetStatus status)
        : AZ::Data::AssetData(assetId, status)
    {
    }

    void SkeletonAsset::Reflect(
        AZ::ReflectContext* context)
    {
        SkeletonAssetData::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SkeletonAsset>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SkeletonAsset, AZ::Data::AssetData>()
                ->Field("Data", &SkeletonAsset::m_data);
        }
    }
} // namespace Jolt
