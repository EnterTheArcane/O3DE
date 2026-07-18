/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ShaderBuilderSystemComponent.h>

#include <AzCore/std/algorithm.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <Editor/ShaderCompilerBackend.h>
#include <Editor/ShaderReflectionData.h>
#include <Slang/SlangModuleClosure.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include <Atom/RHI.Edit/ShaderPlatformInterface.h>
#include <Atom/RHI.Edit/Utils.h>
#include <Atom/RHI.Edit/ShaderBuildOptions.h>

#include <Atom/RPI.Reflect/Asset/AssetHandler.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <Atom/RPI.Edit/Shader/ShaderSourceData.h>
#include <Atom/RPI.Edit/Shader/ShaderVariantListSourceData.h>

#include <AzToolsFramework/ToolsComponents/ToolsAssetCatalogBus.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Settings/SettingsRegistry.h>

#include <CommonFiles/Preprocessor.h>

#include "HashedVariantListSourceData.h"

namespace AZ
{
    namespace ShaderBuilder
    {
        void ShaderBuilderSystemComponent::Reflect(ReflectContext* context)
        {
            if (SerializeContext* serialize = azrtti_cast<SerializeContext*>(context))
            {
                serialize->Class<ShaderBuilderSystemComponent, Component>()
                    ->Version(0)
                    ->Attribute(Edit::Attributes::SystemComponentTags, AZStd::vector<Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }))
                    ;
            }

            PreprocessorOptions::Reflect(context);
            RHI::ShaderCompilerProfiling::Reflect(context);
            RHI::ShaderBuildArguments::Reflect(context);
            RHI::ShaderBuildOptions::Reflect(context);
            HashedVariantListSourceData::Reflect(context);
            ShaderReflectionData::Reflect(context);
            SlangModuleClosureBundle::Reflect(context);
        }

        void ShaderBuilderSystemComponent::GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC_CE("ShaderBuilderService"));
        }

        void ShaderBuilderSystemComponent::GetIncompatibleServices(ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC_CE("ShaderBuilderService"));
        }

        void ShaderBuilderSystemComponent::GetRequiredServices(ComponentDescriptor::DependencyArrayType& required)
        {
            (void)required;
        }

        void ShaderBuilderSystemComponent::GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC_CE("AssetCatalogService"));
        }

        void ShaderBuilderSystemComponent::Init()
        {
        }

        void ShaderBuilderSystemComponent::Activate()
        {
            RHI::ShaderPlatformInterfaceRegisterBus::Handler::BusConnect();
            ShaderPlatformInterfaceRequestBus::Handler::BusConnect();
            ShaderCompilerBackendBus::Handler::BusConnect();

            // Register Shader Asset Builder
            AssetBuilderSDK::AssetBuilderDesc shaderAssetBuilderDescriptor;
            shaderAssetBuilderDescriptor.m_name = "Shader Asset Builder";
            shaderAssetBuilderDescriptor.m_version = 134; // Module-closure variant builds (bundle schema 2, root-variant relink)
            shaderAssetBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern( AZStd::string::format("*.%s", RPI::ShaderSourceData::Extension), AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
            shaderAssetBuilderDescriptor.m_busId = azrtti_typeid<ShaderAssetBuilder>();
            shaderAssetBuilderDescriptor.m_createJobFunction = AZStd::bind(&ShaderAssetBuilder::CreateJobs, &m_shaderAssetBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
            shaderAssetBuilderDescriptor.m_processJobFunction = AZStd::bind(&ShaderAssetBuilder::ProcessJob, &m_shaderAssetBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);

            m_shaderAssetBuilder.BusConnect(shaderAssetBuilderDescriptor.m_busId);
            AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Handler::RegisterBuilderInformation, shaderAssetBuilderDescriptor);

            // If, either the SettingsRegistry doesn't exist, or the property @EnableShaderVariantAssetBuilderRegistryKey is not found,
            // the default is to enable the ShaderVariantAssetBuilder.
            m_enableShaderVariantAssetBuilder = true;
            auto settingsRegistry = AZ::SettingsRegistry::Get();
            if (settingsRegistry)
            {
                settingsRegistry->Get(m_enableShaderVariantAssetBuilder, EnableShaderVariantAssetBuilderRegistryKey);
            }

            if (m_enableShaderVariantAssetBuilder)
            {
                // Register Shader Variant Asset Builder
                AssetBuilderSDK::AssetBuilderDesc shaderVariantAssetBuilderDescriptor;
                shaderVariantAssetBuilderDescriptor.m_name = "Shader Variant Asset Builder";
                // Both "Shader Variant Asset Builder" and "Shader Asset Builder" produce ShaderVariantAsset products. If you update
                // ShaderVariantAsset you will need to update BOTH version numbers, not just "Shader Variant Asset Builder".
                shaderVariantAssetBuilderDescriptor.m_version = 49; // Language-backend variant inputs seam, module-closure variant builds
                shaderVariantAssetBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern(AZStd::string::format("*.%s", HashedVariantListSourceData::Extension), AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
                shaderVariantAssetBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern(AZStd::string::format("*.%s", HashedVariantInfoSourceData::Extension), AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
                shaderVariantAssetBuilderDescriptor.m_busId = azrtti_typeid<ShaderVariantAssetBuilder>();
                shaderVariantAssetBuilderDescriptor.m_createJobFunction = AZStd::bind(&ShaderVariantAssetBuilder::CreateJobs, &m_shaderVariantAssetBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
                shaderVariantAssetBuilderDescriptor.m_processJobFunction = AZStd::bind(&ShaderVariantAssetBuilder::ProcessJob, &m_shaderVariantAssetBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);

                m_shaderVariantAssetBuilder.BusConnect(shaderVariantAssetBuilderDescriptor.m_busId);
                AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Handler::RegisterBuilderInformation, shaderVariantAssetBuilderDescriptor);

                // Register Shader Variant List Builder
                AssetBuilderSDK::AssetBuilderDesc shaderVariantListBuilderDescriptor;
                shaderVariantListBuilderDescriptor.m_name = "Shader Variant List Builder";
                shaderVariantListBuilderDescriptor.m_version = 4; // ShaderStageFunction allocator
                shaderVariantListBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern(AZStd::string::format("*.%s", RPI::ShaderVariantListSourceData::Extension), AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
                shaderVariantListBuilderDescriptor.m_busId = azrtti_typeid<ShaderVariantListBuilder>();
                shaderVariantListBuilderDescriptor.m_createJobFunction = AZStd::bind(&ShaderVariantListBuilder::CreateJobs, &m_shaderVariantListBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
                shaderVariantListBuilderDescriptor.m_processJobFunction = AZStd::bind(&ShaderVariantListBuilder::ProcessJob, &m_shaderVariantListBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);

                m_shaderVariantListBuilder.BusConnect(shaderVariantListBuilderDescriptor.m_busId);
                AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Handler::RegisterBuilderInformation, shaderVariantListBuilderDescriptor);

            }

            // Register Precompiled Shader Builder
            AssetBuilderSDK::AssetBuilderDesc precompiledShaderBuilderDescriptor;
            precompiledShaderBuilderDescriptor.m_name = "Precompiled Shader Builder";
            precompiledShaderBuilderDescriptor.m_version = 15; // ShaderStageFunction allocator
            precompiledShaderBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern(AZStd::string::format("*.%s", AZ::PrecompiledShaderBuilder::Extension), AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
            precompiledShaderBuilderDescriptor.m_busId = azrtti_typeid<PrecompiledShaderBuilder>();
            precompiledShaderBuilderDescriptor.m_createJobFunction = AZStd::bind(&PrecompiledShaderBuilder::CreateJobs, &m_precompiledShaderBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
            precompiledShaderBuilderDescriptor.m_processJobFunction = AZStd::bind(&PrecompiledShaderBuilder::ProcessJob, &m_precompiledShaderBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);

            m_precompiledShaderBuilder.BusConnect(precompiledShaderBuilderDescriptor.m_busId);
            AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Handler::RegisterBuilderInformation, precompiledShaderBuilderDescriptor);
        }

        void ShaderBuilderSystemComponent::Deactivate()
        {
            m_shaderAssetBuilder.BusDisconnect();
            if (m_enableShaderVariantAssetBuilder)
            {
                m_shaderVariantAssetBuilder.BusDisconnect();
                m_shaderVariantListBuilder.BusDisconnect();

            }
            m_precompiledShaderBuilder.BusDisconnect();

            ShaderCompilerBackendBus::Handler::BusDisconnect();
            RHI::ShaderPlatformInterfaceRegisterBus::Handler::BusDisconnect();
            ShaderPlatformInterfaceRequestBus::Handler::BusDisconnect();
        }

        void ShaderBuilderSystemComponent::RegisterShaderCompilerBackend(IShaderCompilerBackend* backend)
        {
            if (!backend)
            {
                return;
            }

            // Duplicate extension claims are a deterministic hard error, never last-writer-wins.
            for (const AZStd::string_view extension : backend->GetSourceExtensions())
            {
                if (IShaderCompilerBackend* existingBackend = FindShaderCompilerBackendForSourceExtension(extension))
                {
                    AZ_Assert(
                        false,
                        "Shader compiler backend '%.*s' claims extension '%.*s' already claimed by '%.*s' - registration rejected",
                        AZ_STRING_ARG(backend->GetName()),
                        AZ_STRING_ARG(extension),
                        AZ_STRING_ARG(existingBackend->GetName()));
                    return;
                }
            }

            m_shaderCompilerBackends.push_back(backend);
        }

        void ShaderBuilderSystemComponent::UnregisterShaderCompilerBackend(IShaderCompilerBackend* backend)
        {
            m_shaderCompilerBackends.erase(
                AZStd::remove(m_shaderCompilerBackends.begin(), m_shaderCompilerBackends.end(), backend),
                m_shaderCompilerBackends.end());
        }

        IShaderCompilerBackend* ShaderBuilderSystemComponent::FindShaderCompilerBackendForSourceExtension(AZStd::string_view extensionWithDot)
        {
            for (IShaderCompilerBackend* backend : m_shaderCompilerBackends)
            {
                for (const AZStd::string_view claimedExtension : backend->GetSourceExtensions())
                {
                    if (AZ::StringFunc::Equal(claimedExtension, extensionWithDot))
                    {
                        return backend;
                    }
                }
            }
            return nullptr;
        }

        void ShaderBuilderSystemComponent::RegisterShaderPlatformHandler(RHI::ShaderPlatformInterface* shaderPlatformInterface)
        {
            m_shaderPlatformInterfaces[shaderPlatformInterface->GetAPIType()] = shaderPlatformInterface;
        }

        void ShaderBuilderSystemComponent::UnregisterShaderPlatformHandler(RHI::ShaderPlatformInterface* shaderPlatformInterface)
        {
            m_shaderPlatformInterfaces.erase(shaderPlatformInterface->GetAPIType());
        }

        AZStd::vector<RHI::ShaderPlatformInterface*> ShaderBuilderSystemComponent::GetShaderPlatformInterface(const AssetBuilderSDK::PlatformInfo& platformInfo)
        {
            //Used to validate that each ShaderPlatformInterface returns a unique index.
            AZStd::unordered_map<uint32_t, Name> apiUniqueIndexToName;


            AZStd::vector<RHI::ShaderPlatformInterface*> results;
            results.reserve(platformInfo.m_tags.size());
            for (const AZStd::string& tag : platformInfo.m_tags)
            {
                // Use the platform tags to find the proper ShaderPlatformInterface
                auto findIt = m_shaderPlatformInterfaces.find(RHI::APIType(tag.c_str()));
                if (findIt != m_shaderPlatformInterfaces.end())
                {
                    RHI::ShaderPlatformInterface* rhiApi = findIt->second;
                    const uint32_t uniqueIndex = rhiApi->GetAPIUniqueIndex();
                    auto uniqueIt = apiUniqueIndexToName.find(uniqueIndex);
                    if (uniqueIt != apiUniqueIndexToName.end())
                    {
                        AZ_Assert(false, "The ShaderPlatformInterface with name [%s] is providing a unique api index [%u] which was already provided by the ShaderPlatformInterface [%s]",
                            rhiApi->GetAPIName().GetCStr(), uniqueIndex, uniqueIt->second.GetCStr());
                        continue;
                    }
                    AZ_Assert(uniqueIndex <= RHI::Limits::APIType::PerPlatformApiUniqueIndexMax, "The api index [%u] from ShaderPlatformInterface [%s] is invalid", uniqueIndex, rhiApi->GetAPIName().GetCStr());
                    apiUniqueIndexToName[uniqueIndex] = rhiApi->GetAPIName();

                    results.push_back(rhiApi);
                }
            }
            return results;
        }

    } // namespace ShaderBuilder
} // namespace AZ
