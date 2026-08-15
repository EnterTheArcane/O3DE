/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SceneBuilder.h>

#include <Jolt/AssetBuilderSystem.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/System.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>

#include <AzFramework/StringFunc/StringFunc.h>

namespace Jolt::Editor
{
    SceneBuilder::~SceneBuilder() = default;

    void SceneBuilder::Register()
    {
        m_isShuttingDown.store(false, AZStd::memory_order_release);

        if (!AZ::Interface<ISystem>::Get())
        {
            m_ownedSystem = CreateAssetBuilderSystem();
            if (!m_ownedSystem)
            {
                AZ_Error("Jolt", false, "Failed to initialize the physics runtime for scene asset processing.");
            }
        }

        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Jolt Scene Builder";
        descriptor.m_patterns.emplace_back(
            "*.joltscene",
            AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard);
        descriptor.m_busId = azrtti_typeid<SceneBuilder>();
        descriptor.m_version = 2;
        descriptor.m_analysisFingerprint = AZStd::string::format(
            "JoltScene:%016llx",
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

    void SceneBuilder::ShutDown()
    {
        m_isShuttingDown.store(true, AZStd::memory_order_release);
    }

    void SceneBuilder::CreateJobs(
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
            descriptor.m_jobKey = "Jolt Scene";
            descriptor.m_critical = true;
            descriptor.SetPlatformIdentifier(platform.m_identifier.c_str());
            response.m_createJobOutputs.push_back(AZStd::move(descriptor));
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    void SceneBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest& request,
        AssetBuilderSDK::ProcessJobResponse& response) const
    {
        if (m_isShuttingDown.load(AZStd::memory_order_acquire))
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        SceneSourceData sourceData;
        const AZ::Outcome<void, AZStd::string> loadResult =
            AZ::JsonSerializationUtils::LoadObjectFromFile(sourceData, request.m_fullPath);
        if (!loadResult.IsSuccess())
        {
            AZ_Error(
                "Jolt",
                false,
                "Failed to load scene source '%s': %s",
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
            AZ_Error("Jolt", false, "Cannot compile scene source without an active physics system.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        SceneAsset sceneAsset;
        if (!system->BuildSceneAsset(sourceData, sceneAsset.m_data))
        {
            AZ_Error("Jolt", false, "Failed to compile scene source '%s'.", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
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
            || !serializeContext->FindClassData(azrtti_typeid<SceneAsset>()))
        {
            AZ_Error("Jolt", false, "Scene asset reflection is unavailable during product serialization.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        if (!AZ::Utils::SaveObjectToFile(
            productPath,
            AZ::DataStream::ST_BINARY,
            &sceneAsset,
            serializeContext))
        {
            AZ_Error("Jolt", false, "Failed to save scene product '%s'.", productPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AssetBuilderSDK::JobProduct product(productPath, SceneAssetTypeId, 0);
        product.m_dependenciesHandled = true;
        response.m_outputProducts.push_back(AZStd::move(product));
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }

    void BuilderComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<BuilderComponent, AZ::Component>()
                ->Attribute(
                    AZ::Edit::Attributes::SystemComponentTags,
                    AZStd::vector<AZ::Crc32>{AssetBuilderSDK::ComponentTags::AssetBuilder});
        }
    }

    void BuilderComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltBuilderService"));
    }

    void BuilderComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltBuilderService"));
    }

    void BuilderComponent::Activate()
    {
        m_builder.Register();
        m_skeletonBuilder.Register();
    }

    void BuilderComponent::Deactivate()
    {
        m_skeletonBuilder.ShutDown();
        m_builder.ShutDown();
        m_skeletonBuilder.BusDisconnect();
        m_builder.BusDisconnect();
    }
} // namespace Jolt::Editor
