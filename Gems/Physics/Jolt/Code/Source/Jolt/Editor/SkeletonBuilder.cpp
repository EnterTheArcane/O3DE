/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SkeletonBuilder.h>

#include <Jolt/AssetBuilderSystem.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/SkeletonAsset.h>
#include <Jolt/System.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/algorithm.h>

#include <AzFramework/StringFunc/StringFunc.h>

namespace Jolt::Editor
{
    SkeletonBuilder::~SkeletonBuilder() = default;

    void SkeletonBuilder::Register()
    {
        m_isShuttingDown.store(false, AZStd::memory_order_release);

        if (!AZ::Interface<ISystem>::Get())
        {
            m_ownedSystem = CreateAssetBuilderSystem();
            if (!m_ownedSystem)
            {
                AZ_Error("Jolt", false, "Failed to initialize the physics runtime for skeleton asset processing.");
            }
        }

        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Jolt Skeleton Builder";
        descriptor.m_patterns.emplace_back(
            "*.joltskeleton",
            AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard);
        descriptor.m_busId = azrtti_typeid<SkeletonBuilder>();
        descriptor.m_version = 2;
        descriptor.m_analysisFingerprint = AZStd::string::format(
            "JoltSkeleton:%016llx",
            static_cast<unsigned long long>(GetNativeBuildFingerprint()));
        descriptor.m_createJobFunction =
            [this](const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
            {
                CreateJobs(request, response);
            };
        descriptor.m_processJobFunction =
            [this](const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
            {
                ProcessJob(request, response);
            };
        descriptor.m_flags = AssetBuilderSDK::AssetBuilderDesc::BF_EmitsNoDependencies;

        BusConnect(descriptor.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(
            &AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation,
            descriptor);
    }

    void SkeletonBuilder::ShutDown()
    {
        m_isShuttingDown.store(true, AZStd::memory_order_release);
    }

    void SkeletonBuilder::CreateJobs(
        const AssetBuilderSDK::CreateJobsRequest& request,
        AssetBuilderSDK::CreateJobsResponse& response) const
    {
        if (m_isShuttingDown.load(AZStd::memory_order_acquire))
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        for (const AssetBuilderSDK::PlatformInfo& platform : request.m_enabledPlatforms)
        {
            AssetBuilderSDK::JobDescriptor descriptor;
            descriptor.m_jobKey = "Jolt Skeleton";
            descriptor.m_critical = true;
            descriptor.SetPlatformIdentifier(platform.m_identifier.c_str());
            response.m_createJobOutputs.push_back(AZStd::move(descriptor));
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    void SkeletonBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest& request,
        AssetBuilderSDK::ProcessJobResponse& response) const
    {
        if (m_isShuttingDown.load(AZStd::memory_order_acquire))
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        SkeletonSourceData sourceData;
        const AZ::Outcome<void, AZStd::string> loadResult =
            AZ::JsonSerializationUtils::LoadObjectFromFile(sourceData, request.m_fullPath);
        if (!loadResult.IsSuccess())
        {
            AZ_Error(
                "Jolt",
                false,
                "Failed to load skeleton source '%s': %s",
                request.m_fullPath.c_str(),
                loadResult.GetError().c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        ISystem* system = m_ownedSystem.get();
        if (!system)
        {
            system = AZ::Interface<ISystem>::Get();
        }
        if (!system)
        {
            AZ_Error("Jolt", false, "Cannot compile skeleton source without an active physics system.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        SkeletonAsset skeletonAsset;
        skeletonAsset.m_data.m_name = AZ::Name(sourceData.m_name);

        SkeletonDefinitionConfiguration skeletonConfiguration;
        skeletonConfiguration.m_joints.reserve(sourceData.m_skeleton.m_joints.size());
        for (const SkeletonJointSource& sourceJoint : sourceData.m_skeleton.m_joints)
        {
            skeletonConfiguration.m_joints.push_back({
                .m_name = AZ::Name(sourceJoint.m_name),
                .m_parentIndex = sourceJoint.m_parentIndex,
            });
        }

        const SkeletonDefinitionHandle skeletonHandle = system->CreateSkeletonDefinition(skeletonConfiguration);
        if (!skeletonHandle)
        {
            AZ_Error("Jolt", false, "Failed to compile skeleton definition from '%s'.", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const bool skeletonExported = system->ExportSkeletonDefinition(
            skeletonHandle,
            skeletonAsset.m_data.m_skeleton);
        system->DestroySkeletonDefinition(skeletonHandle);
        if (!skeletonExported)
        {
            AZ_Error("Jolt", false, "Failed to export skeleton definition from '%s'.", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        skeletonAsset.m_data.m_animations.reserve(sourceData.m_animations.size());
        for (const NamedSkeletalAnimationSource& sourceAnimation : sourceData.m_animations)
        {
            const AZ::Name animationName(sourceAnimation.m_name);
            const auto hasName = [&animationName](const NamedSkeletalAnimationAsset& animation)
            {
                return animation.m_name == animationName;
            };
            if (sourceAnimation.m_name.empty()
                || AZStd::find_if(
                    skeletonAsset.m_data.m_animations.begin(),
                    skeletonAsset.m_data.m_animations.end(),
                    hasName) != skeletonAsset.m_data.m_animations.end())
            {
                AZ_Error("Jolt", false, "Skeleton source '%s' has an empty or duplicate animation name.", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            SkeletalAnimationConfiguration animationConfiguration;
            animationConfiguration.m_isLooping = sourceAnimation.m_configuration.m_isLooping;
            animationConfiguration.m_joints.reserve(sourceAnimation.m_configuration.m_joints.size());
            for (const SkeletalAnimatedJointSource& sourceJoint : sourceAnimation.m_configuration.m_joints)
            {
                animationConfiguration.m_joints.push_back({
                    .m_name = AZ::Name(sourceJoint.m_name),
                    .m_keyframes = sourceJoint.m_keyframes,
                });
            }

            const SkeletalAnimationHandle animationHandle = system->CreateSkeletalAnimation(animationConfiguration);
            if (!animationHandle)
            {
                AZ_Error(
                    "Jolt",
                    false,
                    "Failed to compile animation '%s' from '%s'.",
                    sourceAnimation.m_name.c_str(),
                    request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            NamedSkeletalAnimationAsset& productAnimation = skeletonAsset.m_data.m_animations.emplace_back();
            productAnimation.m_name = animationName;
            const bool animationExported = system->ExportSkeletalAnimation(
                animationHandle,
                productAnimation.m_archive);
            system->DestroySkeletalAnimation(animationHandle);
            if (!animationExported)
            {
                AZ_Error(
                    "Jolt",
                    false,
                    "Failed to export animation '%s' from '%s'.",
                    sourceAnimation.m_name.c_str(),
                    request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }
        }

        AZStd::string fileName;
        AzFramework::StringFunc::Path::GetFullFileName(request.m_sourceFile.c_str(), fileName);
        AZStd::string productPath;
        AzFramework::StringFunc::Path::ConstructFull(
            request.m_tempDirPath.c_str(),
            fileName.c_str(),
            productPath,
            true);

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(
            serializeContext,
            &AZ::ComponentApplicationBus::Events::GetSerializeContext);
        if (!serializeContext
            || !serializeContext->FindClassData(azrtti_typeid<SkeletonAsset>()))
        {
            AZ_Error("Jolt", false, "Skeleton asset reflection is unavailable during product serialization.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        if (!AZ::Utils::SaveObjectToFile(
            productPath,
            AZ::DataStream::ST_BINARY,
            &skeletonAsset,
            serializeContext))
        {
            AZ_Error("Jolt", false, "Failed to save skeleton product '%s'.", productPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AssetBuilderSDK::JobProduct product(productPath, SkeletonAssetTypeId, 0);
        product.m_dependenciesHandled = true;
        response.m_outputProducts.push_back(AZStd::move(product));
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }
} // namespace Jolt::Editor
