/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/AssetBuilder.h>

#include <Jolt/AssetBuilderSystem.h>
#include <Jolt/AssetProduct.h>
#include <Jolt/Capabilities.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/SkeletonAsset.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <AzFramework/StringFunc/StringFunc.h>

namespace Jolt::Editor
{
    namespace
    {
        enum class SourceDocumentKind : AZ::u8
        {
            Scene = 1,
            Skeleton,
        };

        struct SourceDocumentType final
        {
            SourceDocumentKind m_kind;
            AZStd::string_view m_className;
            AZStd::string_view m_jobKey;
        };

        constexpr SourceDocumentType SourceDocumentTypes[] = {
            {
                .m_kind = SourceDocumentKind::Scene,
                .m_className = "SceneSourceData",
                .m_jobKey = "Jolt Scene",
            },
            {
                .m_kind = SourceDocumentKind::Skeleton,
                .m_className = "SkeletonSourceData",
                .m_jobKey = "Jolt Skeleton",
            },
        };
        constexpr AZ::u32 CompiledProductSubId = 1;
        constexpr AZStd::string_view CompiledProductExtension = "jolt";

        AZ::Outcome<const SourceDocumentType*, AZStd::string> ReadSourceDocumentType(
            AZStd::string_view filePath)
        {
            AZ::Outcome<rapidjson::Document, AZStd::string> documentResult =
                AZ::JsonSerializationUtils::ReadJsonFile(filePath);
            if (!documentResult.IsSuccess())
            {
                return AZ::Failure(documentResult.TakeError());
            }

            const rapidjson::Document& document = documentResult.GetValue();
            if (!document.IsObject())
            {
                return AZ::Failure(AZStd::string("The root must be a JSON object."));
            }

            const auto type = document.FindMember("Type");
            if (type == document.MemberEnd()
                || !type->value.IsString()
                || AZStd::string_view(type->value.GetString(), type->value.GetStringLength()) != "JsonSerialization")
            {
                return AZ::Failure(AZStd::string("The root Type must be 'JsonSerialization'."));
            }

            const auto version = document.FindMember("Version");
            if (version == document.MemberEnd()
                || !version->value.IsUint()
                || version->value.GetUint() != 1)
            {
                return AZ::Failure(AZStd::string("The root Version must be 1."));
            }

            const auto classNameMember = document.FindMember("ClassName");
            if (classNameMember == document.MemberEnd()
                || !classNameMember->value.IsString())
            {
                return AZ::Failure(AZStd::string("The root ClassName must name a supported Jolt source document."));
            }

            const AZStd::string_view className(
                classNameMember->value.GetString(),
                classNameMember->value.GetStringLength());
            for (const SourceDocumentType& sourceType : SourceDocumentTypes)
            {
                if (className == sourceType.m_className)
                {
                    return AZ::Success(&sourceType);
                }
            }

            return AZ::Failure(AZStd::string::format(
                "Unsupported Jolt source document ClassName '%.*s'.",
                AZ_STRING_ARG(className)));
        }

        template<class AssetType>
        bool SaveProduct(
            const AssetBuilderSDK::ProcessJobRequest& request,
            const AssetType& asset,
            const AZ::Data::AssetType& assetType,
            AssetBuilderSDK::ProcessJobResponse& response,
            const AZStd::span<const CustomShapeDependency> sourceDependencies = {})
        {
            AZStd::string fileName;
            AzFramework::StringFunc::Path::GetFullFileName(
                request.m_sourceFile.c_str(),
                fileName);
            AzFramework::StringFunc::Path::ReplaceExtension(fileName, "");
            AzFramework::StringFunc::Path::ReplaceExtension(
                fileName,
                CompiledProductExtension.data());

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
                || !serializeContext->FindClassData(azrtti_typeid<AssetType>()))
            {
                AZ_Error(
                    "Jolt",
                    false,
                    "Asset reflection is unavailable while compiling '%s'.",
                    request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return false;
            }

            if (!SaveAssetProduct(
                productPath,
                &asset,
                assetType,
                *serializeContext))
            {
                AZ_Error(
                    "Jolt",
                    false,
                    "Failed to save product '%s'.",
                    productPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return false;
            }

            AssetBuilderSDK::JobProduct product(productPath, assetType, CompiledProductSubId);
            for (const CustomShapeDependency& dependency : sourceDependencies)
            {
                product.m_pathDependencies.emplace(
                    dependency.m_path,
                    AssetBuilderSDK::ProductPathDependencyType::SourceFile);
            }
            product.m_dependenciesHandled = true;
            response.m_outputProducts.push_back(AZStd::move(product));
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            return true;
        }
    } // namespace

    AssetBuilder::~AssetBuilder() = default;

    void AssetBuilder::Register()
    {
        m_isShuttingDown.store(false, AZStd::memory_order_release);

        if (!RuntimeConfiguration::Get())
        {
            m_ownedSystem = CreateAssetBuilderSystem();
            if (!m_ownedSystem)
            {
                AZ_Error("Jolt", false, "Failed to initialize the physics runtime for asset processing.");
            }
        }

        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Jolt Asset Builder";
        descriptor.m_patterns.emplace_back(
            "*.jolt.json",
            AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard);
        descriptor.m_busId = azrtti_typeid<AssetBuilder>();
        descriptor.m_version = 8;
        descriptor.m_analysisFingerprint = AZStd::string::format(
            "JoltAssets:Portable2:%016llx",
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
        descriptor.m_flags = AssetBuilderSDK::AssetBuilderDesc::BF_None;

        BusConnect(descriptor.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(
            &AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation,
            descriptor);
    }

    void AssetBuilder::ShutDown()
    {
        m_isShuttingDown.store(true, AZStd::memory_order_release);
    }

    void AssetBuilder::CreateJobs(
        const AssetBuilderSDK::CreateJobsRequest& request,
        AssetBuilderSDK::CreateJobsResponse& response) const
    {
        if (m_isShuttingDown.load(AZStd::memory_order_acquire))
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        AZStd::string fullPath;
        AzFramework::StringFunc::Path::ConstructFull(
            request.m_watchFolder.c_str(),
            request.m_sourceFile.c_str(),
            fullPath,
            true);
        const AZ::Outcome<const SourceDocumentType*, AZStd::string> sourceTypeResult =
            ReadSourceDocumentType(fullPath);
        if (!sourceTypeResult.IsSuccess())
        {
            AZ_Error(
                "Jolt",
                false,
                "Failed to identify Jolt source document '%s': %s",
                fullPath.c_str(),
                sourceTypeResult.GetError().c_str());
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
            return;
        }

        const SourceDocumentType& sourceType = *sourceTypeResult.GetValue();
        for (const AssetBuilderSDK::PlatformInfo& platform : request.m_enabledPlatforms)
        {
            AssetBuilderSDK::JobDescriptor descriptor;
            descriptor.m_jobKey = sourceType.m_jobKey;
            descriptor.m_critical = true;
            descriptor.SetPlatformIdentifier(platform.m_identifier.c_str());
            response.m_createJobOutputs.push_back(AZStd::move(descriptor));
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    void AssetBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest& request,
        AssetBuilderSDK::ProcessJobResponse& response) const
    {
        if (m_isShuttingDown.load(AZStd::memory_order_acquire))
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }
        if (request.m_platformInfo.m_identifier.empty())
        {
            AZ_Error("Jolt", false, "Cannot compile a Jolt asset without a target platform identifier.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const AZ::Outcome<const SourceDocumentType*, AZStd::string> sourceTypeResult =
            ReadSourceDocumentType(request.m_fullPath);
        if (!sourceTypeResult.IsSuccess())
        {
            AZ_Error(
                "Jolt",
                false,
                "Failed to identify Jolt source document '%s': %s",
                request.m_fullPath.c_str(),
                sourceTypeResult.GetError().c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        Scenes* scenes = Scenes::Get();
        Skeletons* skeletons = Skeletons::Get();
        if (!scenes || !skeletons)
        {
            AZ_Error("Jolt", false, "Cannot compile a Jolt source document without an active physics system.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const SourceDocumentType& sourceType = *sourceTypeResult.GetValue();
        if (sourceType.m_kind == SourceDocumentKind::Scene)
        {
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

            SceneAsset sceneAsset;
            if (!scenes->BuildSceneAsset(sourceData, sceneAsset.m_data))
            {
                AZ_Error("Jolt", false, "Failed to compile scene source '%s'.", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            AZStd::vector<CustomShapeDependency> sourceDependencies;
            for (const SceneAssetShape& shape : sceneAsset.m_data.m_shapes)
            {
                sourceDependencies.insert(
                    sourceDependencies.end(),
                    shape.m_dependencies.begin(),
                    shape.m_dependencies.end());
            }

            if (request.m_platformInfo.m_identifier != GetNativeAssetPlatform())
            {
                sceneAsset.m_data.m_nativeCachePlatform.clear();
                sceneAsset.m_data.m_nativeCacheBuildFingerprint = 0;
                for (SceneAssetShape& shape : sceneAsset.m_data.m_shapes)
                {
                    shape.m_archive = {};
                    shape.m_materialIndices.clear();
                    shape.m_childShapeIndices.clear();
                }
                for (SceneAssetSoftBodyDefinition& definition : sceneAsset.m_data.m_softBodyDefinitions)
                {
                    definition.m_archive = {};
                    definition.m_materialIndices.clear();
                }
            }

            SaveProduct(
                request,
                sceneAsset,
                SceneAssetTypeId,
                response,
                sourceDependencies);
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

        SkeletonAsset skeletonAsset;
        skeletonAsset.m_data.m_name = AZ::Name(sourceData.m_name);
        skeletonAsset.m_data.m_sourceSkeleton = sourceData.m_skeleton;
        skeletonAsset.m_data.m_nativeCachePlatform = GetNativeAssetPlatform();
        skeletonAsset.m_data.m_nativeCacheBuildFingerprint = GetNativeBuildFingerprint();

        const SkeletonDefinitionHandle skeletonHandle = skeletons->CreateSkeletonDefinition(sourceData.m_skeleton);
        if (!skeletonHandle)
        {
            AZ_Error("Jolt", false, "Failed to compile skeleton definition from '%s'.", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const bool skeletonExported = skeletons->ExportSkeletonDefinition(
            skeletonHandle,
            skeletonAsset.m_data.m_skeleton);
        skeletons->DestroySkeletonDefinition(skeletonHandle);
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
                AZ_Error(
                    "Jolt",
                    false,
                    "Skeleton source '%s' has an empty or duplicate animation name.",
                    request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            const SkeletalAnimationHandle animationHandle = skeletons->CreateSkeletalAnimation(sourceAnimation.m_configuration);
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
            productAnimation.m_source = sourceAnimation.m_configuration;
            productAnimation.m_name = animationName;
            const bool animationExported = skeletons->ExportSkeletalAnimation(
                animationHandle,
                productAnimation.m_archive);
            skeletons->DestroySkeletalAnimation(animationHandle);
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

        if (request.m_platformInfo.m_identifier != GetNativeAssetPlatform())
        {
            skeletonAsset.m_data.m_skeleton = {};
            skeletonAsset.m_data.m_nativeCachePlatform.clear();
            skeletonAsset.m_data.m_nativeCacheBuildFingerprint = 0;
            for (NamedSkeletalAnimationAsset& animation : skeletonAsset.m_data.m_animations)
            {
                animation.m_archive = {};
            }
        }

        SaveProduct(
            request,
            skeletonAsset,
            SkeletonAssetTypeId,
            response);
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

    void BuilderComponent::GetDependentServices(
        AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC_CE("JoltService"));
    }

    void BuilderComponent::Activate()
    {
        m_builder.Register();
    }

    void BuilderComponent::Deactivate()
    {
        m_builder.ShutDown();
        m_builder.BusDisconnect();
    }
} // namespace Jolt::Editor
