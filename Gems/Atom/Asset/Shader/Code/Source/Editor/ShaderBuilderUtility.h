/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/Preprocessor/Enum.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>

#include <Atom/RPI.Edit/Shader/ShaderSourceData.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include "AzslData.h"

namespace AZ
{
    namespace ShaderBuilder
    {
        class AzslCompiler;
        struct BindingDependencies;
        struct RootConstantData;

        using MapOfStringToStageType = AZStd::unordered_map<AZStd::string, RPI::ShaderStageType>;

        namespace ShaderBuilderUtility
        {
            //! Collect AZSL include directories in source, project, scan-folder and engine lookup order.
            AZStd::vector<AZStd::string> BuildListOfIncludeDirectories(
                const char* builderName,
                const char* optionalIncludeFolder = nullptr);

            //! Resolve an RHI header relative to the executable directory used for header staging.
            AZStd::string ResolveAzslHeaderPath(const char* header);

            //! Resolve configured -I/--include paths against the project without changing argument boundaries.
            AZStd::vector<AZStd::string> NormalizeAzslArguments(const AZStd::vector<AZStd::string>& arguments);

            //! Build one ordered compiler environment for dependency discovery and compilation.
            AZStd::vector<AZStd::string> BuildAzslArguments(
                const RHI::ShaderBuildArguments& arguments,
                const AZStd::vector<AZStd::string>& includePaths,
                const AZStd::string& platformHeader);

            Outcome<RPI::ShaderSourceData, AZStd::string> LoadShaderDataJson(const AZStd::string& fullPathToJsonFile, bool warningsAsErrors = true);

            void GetAbsolutePathToAzslFile(const AZStd::string& shaderSourceFileFullPath, AZStd::string specifiedShaderPathAndName, AZStd::string& absoluteShaderPath);

            //! Load the shader descriptor and resolve its AZSL source path.
            Outcome<AZStd::string, AZStd::string> PrepareSourceInput(
                const AZStd::string& shaderSourceFileFullPath,
                RPI::ShaderSourceData& sourceAsset);

            namespace AzslSubProducts
            {
                AZ_ENUM(SuffixList, ia, om, srg, options, bindingdep, hlsl);

                using SubId = RPI::ShaderAssetSubId;
                // product sub id enumerators:
                static constexpr SubId SubList[] = {SubId::IaJson,
                                                    SubId::OmJson,
                                                    SubId::SrgJson,
                                                    SubId::OptionsJson,
                                                    SubId::BindingdepJson,
                                                    SubId::GeneratedHlslSource};
                // in the same order, their file name suffix (they replicate what's in AzslcMain.cpp. and hlsl corresponds to what's in AzslBuilder.cpp)
                
                // a type to declare variables holding the full paths of their files
                using Paths = AZStd::fixed_vector<AZStd::string, AZ_ARRAY_SIZE(SubList)>;
            };

            //! Collects all the JSON files generated during AZSL compilation and loads the data as objects.
            //! @azslData must have paths correctly set.
            //! @azslData, @srgLayoutList, @shaderOptionGroupLayout, @bindingDependencies and @rootConstantData get the output data.
            AssetBuilderSDK::ProcessJobResultCode PopulateAzslDataFromJsonFiles(
                const char* builderName,
                const AzslSubProducts::Paths& pathOfJsonFiles,
                AzslData& azslData,
                RPI::ShaderResourceGroupLayoutList& srgLayoutList,
                RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout,
                BindingDependencies& bindingDependencies,
                RootConstantData& rootConstantData,
                const AZStd::string& tempFolder,
                bool& useSpecializationConstants);


            RHI::ShaderHardwareStage ToAssetBuilderShaderType(RPI::ShaderStageType stageType);

            //! Must be called before shaderPlatformInterface->CompilePlatformInternal()
            //! This function will prune non entry functions from BindingDependencies and use the
            //! rest of input data to create a pipeline layout descriptor.
            //! The pipeline layout descriptor is returned, but the same data will also be set into the @shaderPlatformInterface
            //! object, which is why it is important to call this method before calling shaderPlatformInterface->CompilePlatformInternal().
            RHI::Ptr<RHI::PipelineLayoutDescriptor> BuildPipelineLayoutDescriptorForApi(
                const char* builderName,
                const RPI::ShaderResourceGroupLayoutList& srgLayoutList,
                const MapOfStringToStageType& shaderEntryPoints,
                const RHI::ShaderBuildArguments& shaderBuildArguments,
                const RootConstantData& rootConstantData,
                RHI::ShaderPlatformInterface* shaderPlatformInterface,
                BindingDependencies& bindingDependencies /*inout*/);


            bool CreateShaderInputAndOutputContracts(
                const AzslData& azslData, const MapOfStringToStageType& shaderEntryPoints,
                const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout, const AZStd::string& pathToOmJson,
                const AZStd::string& pathToIaJson, RPI::ShaderInputContract& shaderInputContract,
                RPI::ShaderOutputContract& shaderOutputContract, size_t& colorAttachmentCount,
                const AZStd::string& tempFolder);

            //! "d:/p/f.e" -> "f"
            AZStd::string ExtractStemName(const char* path);

            AZStd::vector<RHI::ShaderPlatformInterface*> DiscoverValidShaderPlatformInterfaces(const AssetBuilderSDK::PlatformInfo& info);
            AZStd::vector<RHI::ShaderPlatformInterface*> DiscoverEnabledShaderPlatformInterfaces(
                const AssetBuilderSDK::PlatformInfo& info, const RPI::ShaderSourceData& shaderSourceData);

            // The idea is that the "Supervariants" json property is optional in .shader files,
            // For cases when it is not specified, this function will return a vector with one item, the default, nameless, supervariant.
            // If "Supervariants" is not empty, then this function will make sure the first supervariant in the list
            // is the default, nameless, supervariant.
            AZStd::vector<RPI::ShaderSourceData::SupervariantInfo> GetSupervariantListFromShaderSourceData(
                const RPI::ShaderSourceData& shaderSourceData);

            void LogProfilingData(const char* builderName, AZStd::string_view shaderPath);

            //! Returns the asset path of a product artifact produced by ShaderAssetBuilder.
            Outcome<AZStd::string, AZStd::string> ObtainBuildArtifactPathFromShaderAssetBuilder(
                const uint32_t rhiUniqueIndex, const AZStd::string& platformIdentifier, const AZStd::string& shaderJsonPath,
                const uint32_t supervariantIndex, RPI::ShaderAssetSubId shaderAssetSubId);

            AZStd::string GetPlatformNameFromPlatformInfo(const AssetBuilderSDK::PlatformInfo& platformInfo);

        }  // ShaderBuilderUtility namespace
    } // ShaderBuilder namespace
} // AZ
