/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ShaderVariantAssetBuilder.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderVariantAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderVariantTreeAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>

#include <Atom/RPI.Edit/Shader/ShaderVariantAssetCreator.h>
#include <Atom/RPI.Edit/Shader/ShaderVariantTreeAssetCreator.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <AzCore/Serialization/Json/JsonUtils.h>
#include <Atom/RPI.Reflect/Shader/ShaderVariantKey.h>

#include <Atom/RHI.Edit/Utils.h>
#include <Atom/RHI.Edit/ShaderPlatformInterface.h>
#include <Atom/RPI.Edit/Common/JsonReportingHelper.h>
#include <Atom/RPI.Edit/Common/AssetUtils.h>
#include <Atom/RHI.Reflect/ConstantsLayout.h>
#include <Atom/RHI.Reflect/PipelineLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ShaderStageFunction.h>
#include <Atom/RHI/RHIUtils.h>

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/Debug/TraceContext.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Platform/PlatformDefaults.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/JSON/document.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/IOUtils.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/time.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/StringFunc/StringFunc.h>

#include "HashedVariantListSourceData.h"
#include "ShaderAssetBuilder.h"
#include "ShaderBuilderUtility.h"
#include "ShaderCompilerBackend.h"
#include "ShaderCompilerBackendBus.h"
#include <CommonFiles/Preprocessor.h>
#include <ShaderPlatformInterfaceRequest.h>
#include "ShaderBuildArgumentsManager.h"


namespace AZ
{
    namespace ShaderBuilder
    {
        static constexpr char ShaderVariantAssetBuilderName[] = "ShaderVariantAssetBuilder";


        AZStd::string ShaderVariantAssetBuilder::GetShaderVariantTreeAssetJobKey()
        {
            return AZStd::string::format("%s_varianttree", ShaderVariantAssetBuilderJobKeyPrefix);
        }


        AZStd::string ShaderVariantAssetBuilder::GetShaderVariantAssetJobKey()
        {
            return AZStd::string::format("%s_variantbatch", ShaderVariantAssetBuilderJobKeyPrefix);
        }

        void ShaderVariantAssetBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const
        {
            // Please see comments in the header file for the core principles of this builder.

            // Is this a *.hashedvariantlist? if so We need to create the ShaderVariantTreeAsset
            AZStd::string fileExtension;
            AzFramework::StringFunc::Path::GetExtension(request.m_sourceFile.c_str(), fileExtension, false /*includeDot*/);
            if (fileExtension == HashedVariantListSourceData::Extension)
            {
                CreateShaderVariantTreeJobs(request, response);
                return;
            }
            else if (fileExtension == HashedVariantInfoSourceData::Extension)
            {
                CreateShaderVariantJobs(request, response);
                return;
            }

            AZ_Error(ShaderVariantAssetBuilderName, false, "Unsupported file extension: %s", fileExtension.c_str());
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
        }  // CreateJobs


        void ShaderVariantAssetBuilder::CreateShaderVariantTreeJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const
        {
            AZStd::string variantListRelativePath(request.m_sourceFile.data());
            AZStd::string hashedVariantListFullPath;
            AZ::StringFunc::Path::ConstructFull(request.m_watchFolder.data(), request.m_sourceFile.data(), hashedVariantListFullPath, true);

            AZ_TracePrintf(ShaderVariantAssetBuilderName, "CreateShaderVariantTreeJob for Hashed Shader Variant List \"%s\"\n", hashedVariantListFullPath.data());

            HashedVariantListSourceData hashedVariantListDescriptor;
            if (!RPI::JsonUtils::LoadObjectFromFile(hashedVariantListFullPath, hashedVariantListDescriptor, AZStd::numeric_limits<size_t>::max()))
            {
                AZ_Assert(false, "Failed to parse Hashed Variant List Descriptor JSON [%s]", hashedVariantListFullPath.c_str());
                response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
                return;
            }

            for (const AssetBuilderSDK::PlatformInfo& info : request.m_enabledPlatforms)
            {
                AZ_TraceContext("For platform", info.m_identifier.data());

                AssetBuilderSDK::JobDescriptor jobDescriptor;

                // The ShaderVariantTreeAsset is high priority, but must be generated after the ShaderAsset 
                jobDescriptor.m_priority = 1;
                jobDescriptor.m_critical = false;

                jobDescriptor.m_jobKey = GetShaderVariantTreeAssetJobKey();
                jobDescriptor.SetPlatformIdentifier(info.m_identifier.data());

                // Declare job dependency on the .azshader, this way the ShaderAsset must be built before
                // the ShaderVariantTreeAsset.
                AssetBuilderSDK::JobDependency jobDependency;
                jobDependency.m_jobKey = ShaderAssetBuilder::ShaderAssetBuilderJobKey;
                jobDependency.m_platformIdentifier = info.m_identifier;
                jobDependency.m_type = AssetBuilderSDK::JobDependencyType::Order;
                jobDependency.m_sourceFile.m_sourceFileDependencyPath = hashedVariantListDescriptor.m_shaderPath;
                jobDescriptor.m_jobDependencyList.push_back(jobDependency);

                response.m_createJobOutputs.push_back(jobDescriptor);

            }
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
        }


        // For a file with the following name: <shaderName>_<BatchId>.hashedvariantbatch
        // returns the absolute path that looks like: <shaderName>.hashedvariantlist
        static AZStd::string GetHashedVariantListPathFromVariantInfoPath(const AZStd::string& hashedVariantBatchParentPath, const AZStd::string& hashedVariantBatchRelativePath)
        {
            size_t charPos = AZ::StringFunc::Find(hashedVariantBatchRelativePath, "_", 0, true /* reverse*/);
            AZStd::string pathBefore_ = hashedVariantBatchRelativePath.substr(0, charPos);
            return AZStd::string::format(
                "%s%s%s.%s",
                hashedVariantBatchParentPath.c_str(),
                AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING,
                pathBefore_.c_str(),
                HashedVariantListSourceData::Extension);
        }


        void ShaderVariantAssetBuilder::CreateShaderVariantJobs([[maybe_unused]] const AssetBuilderSDK::CreateJobsRequest& request,
                                                               [[maybe_unused]] AssetBuilderSDK::CreateJobsResponse& response) const
        {

            AZStd::string hashedVariantBatchRelativePath(request.m_sourceFile.data());
            AZStd::string hashedVariantBatchFullPath;
            AZ::StringFunc::Path::ConstructFull(
                request.m_watchFolder.data(), request.m_sourceFile.data(), hashedVariantBatchFullPath, true);
            
            AZ_TracePrintf(
                ShaderVariantAssetBuilderName,
                "CreateShaderVariantJobs for Hashed Variant Batch [%s]\n",
                hashedVariantBatchFullPath.data());
            
            HashedVariantListSourceData hashedVariantBatchDescriptor;
            if (!RPI::JsonUtils::LoadObjectFromFile(
                    hashedVariantBatchFullPath, hashedVariantBatchDescriptor, AZStd::numeric_limits<size_t>::max()))
            {
                AZ_Assert(false, "Failed to parse Hashed Variant Info Descriptor JSON [%s]", hashedVariantBatchFullPath.c_str());
                response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
                return;
            }

            AZStd::string hashedVariantBatchDescriptorString;
            RPI::JsonUtils::SaveObjectToJsonString(hashedVariantBatchDescriptor, hashedVariantBatchDescriptorString);

            AZStd::string hashedVariantBatchParentPath(request.m_watchFolder.data());
            AZStd::string hashedVariantListFullPath =
                GetHashedVariantListPathFromVariantInfoPath(hashedVariantBatchParentPath, hashedVariantBatchRelativePath);
            
            for (const AssetBuilderSDK::PlatformInfo& info : request.m_enabledPlatforms)
            {
                AZ_TraceContext("For platform", info.m_identifier.data());
            
                AssetBuilderSDK::JobDescriptor jobDescriptor;
            
                // There can be tens/hundreds of thousands of shader variants. By default each shader will get
                // a root variant that can be used at runtime. In order to prevent the AssetProcessor from
                // being overwhelmed by shader variant compilation We mark all non-root shader variant generation
                // as non critical and very low priority.
                jobDescriptor.m_priority = -5000;
                jobDescriptor.m_critical = false;
            
                jobDescriptor.m_jobKey = GetShaderVariantAssetJobKey();
                jobDescriptor.SetPlatformIdentifier(info.m_identifier.data());

                // Add the content of the hashedVariantBatch file as a parameter to avoid reading it again.
                jobDescriptor.m_jobParameters.emplace(ShaderVariantBatchJobParam, hashedVariantBatchDescriptorString);
            
                // The ShaderVariantAssets should be built AFTER the ShaderVariantTreeAsset.
                // With "OrderOnly" dependency, We make sure ShaderVariantTreeAsset completes before ShaderVariantAsset runs,
                // but don't re-run ShaderVariantAsset just because ShaderVariantTreeAsset ran.
                //
                AssetBuilderSDK::JobDependency variantTreeJobDependency;
                variantTreeJobDependency.m_jobKey = GetShaderVariantTreeAssetJobKey();
                variantTreeJobDependency.m_platformIdentifier = info.m_identifier;
                variantTreeJobDependency.m_sourceFile.m_sourceFileDependencyPath = hashedVariantListFullPath;
                variantTreeJobDependency.m_type = AssetBuilderSDK::JobDependencyType::OrderOnly;
                jobDescriptor.m_jobDependencyList.emplace_back(variantTreeJobDependency);

                // If the *.shader file changes, all the variants need to be rebuilt.
                AssetBuilderSDK::JobDependency shaderAssetJobDependency;
                shaderAssetJobDependency.m_jobKey = ShaderAssetBuilder::ShaderAssetBuilderJobKey;
                shaderAssetJobDependency.m_platformIdentifier = info.m_identifier;
                shaderAssetJobDependency.m_sourceFile.m_sourceFileDependencyPath = hashedVariantBatchDescriptor.m_shaderPath;
                shaderAssetJobDependency.m_type = AssetBuilderSDK::JobDependencyType::Order;
                jobDescriptor.m_jobDependencyList.emplace_back(shaderAssetJobDependency);
            
                response.m_createJobOutputs.push_back(jobDescriptor);
            
            }
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;

        }


        void ShaderVariantAssetBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
        {
            if (request.m_jobDescription.m_jobKey == GetShaderVariantTreeAssetJobKey())
            {
                ProcessShaderVariantTreeJob(request, response);
            }
            else
            {
                ProcessShaderVariantJob(request, response);
            }
        }


        //! The shader language backend that owns the .shader descriptor's source extension —
        //! the loader of frontend products and the compiler of variant stages.
        static IShaderCompilerBackend* FindBackendForShaderSource(const RPI::ShaderSourceData& shaderSourceDescriptor)
        {
            AZStd::string sourceExtension;
            AzFramework::StringFunc::Path::GetExtension(shaderSourceDescriptor.m_source.c_str(), sourceExtension, true);
            IShaderCompilerBackend* compilerBackend = nullptr;
            ShaderCompilerBackendBus::BroadcastResult(
                compilerBackend, &ShaderCompilerBackendBus::Events::FindShaderCompilerBackendForSourceExtension, sourceExtension);
            AZ_Error(
                ShaderVariantAssetBuilderName, compilerBackend,
                "No shader compiler backend is registered for source extension '%s'", sourceExtension.c_str());
            return compilerBackend;
        }


        void ShaderVariantAssetBuilder::ProcessShaderVariantTreeJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
        {
            AssetBuilderSDK::JobCancelListener jobCancelListener(request.m_jobId);
            if (jobCancelListener.IsCancelled())
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                return;
            }

            AZStd::string hashedVariantListFullPath;
            AZ::StringFunc::Path::ConstructFull(request.m_watchFolder.data(), request.m_sourceFile.data(), hashedVariantListFullPath, true);

            HashedVariantListSourceData hashedVariantListDescriptor;
            if (!RPI::JsonUtils::LoadObjectFromFile(hashedVariantListFullPath, hashedVariantListDescriptor, AZStd::numeric_limits<size_t>::max()))
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "Failed to parse Hashed Variant List Descriptor JSON [%s]", hashedVariantListFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            const AZStd::string& shaderSourceFileFullPath = hashedVariantListDescriptor.m_shaderPath;

            AZStd::string shaderName;
            AZ::StringFunc::Path::GetFileName(shaderSourceFileFullPath.c_str(), shaderName);

            auto descriptorParseOutcome = ShaderBuilderUtility::LoadShaderDataJson(shaderSourceFileFullPath);
            if (!descriptorParseOutcome.IsSuccess())
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "Failed to parse shader file [%s]", shaderSourceFileFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            RPI::ShaderSourceData shaderSourceDescriptor = descriptorParseOutcome.TakeValue();
            RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout;

            // Request the list of valid shader platform interfaces for the target platform.
            AZStd::vector<RHI::ShaderPlatformInterface*> platformInterfaces =
                ShaderBuilderUtility::DiscoverEnabledShaderPlatformInterfaces(request.m_platformInfo, shaderSourceDescriptor);
            if (platformInterfaces.empty())
            {
                // No work to do. Exit gracefully.
                AZ_TracePrintf(
                    ShaderVariantAssetBuilderName,
                    "No azshadervarianttree is produced on behalf of %s because all valid RHI backends were disabled for this shader.\n",
                    shaderSourceFileFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
                return;
            }

            IShaderCompilerBackend* compilerBackend = FindBackendForShaderSource(shaderSourceDescriptor);
            if (!compilerBackend)
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            // The language source path, for backend error messages; no compiler runs on it here.
            AZStd::string azslFullPath;
            ShaderBuilderUtility::GetAbsolutePathToAzslFile(shaderSourceFileFullPath, shaderSourceDescriptor.m_source, azslFullPath);

            auto supervariantList = ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceDescriptor);

            AZStd::string previousLoopApiName;
            bool usesVariants = false;
            for (RHI::ShaderPlatformInterface* shaderPlatformInterface : platformInterfaces)
            {
                auto thisLoopApiName = shaderPlatformInterface->GetAPIName().GetStringView();
                for (uint32_t supervariantIndexCounter = 0; supervariantIndexCounter < supervariantList.size(); ++supervariantIndexCounter)
                {
                    VariantCompilationInputsRequest inputsRequest;
                    inputsRequest.m_builderName = ShaderVariantAssetBuilderName;
                    inputsRequest.m_platformInfo = &request.m_platformInfo;
                    inputsRequest.m_shaderPlatformInterface = shaderPlatformInterface;
                    inputsRequest.m_shaderDescriptorPath = shaderSourceFileFullPath;
                    inputsRequest.m_shaderSourceFullPath = azslFullPath;
                    inputsRequest.m_supervariantIndex = supervariantIndexCounter;
                    inputsRequest.m_tempDirPath = request.m_tempDirPath;
                    inputsRequest.m_loadStageInputs = false;
                    auto inputsOutcome = compilerBackend->LoadVariantCompilationInputs(inputsRequest);
                    if (!inputsOutcome.IsSuccess())
                    {
                        AZ_Error(ShaderVariantAssetBuilderName, false, "%s", inputsOutcome.GetError().c_str());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    const bool usesSpecialization = inputsOutcome.GetValue().m_useSpecializationConstants;
                    RPI::Ptr<RPI::ShaderOptionGroupLayout> loopLocal_ShaderOptionGroupLayout =
                        inputsOutcome.GetValue().m_shaderOptionGroupLayout;
                    if (shaderOptionGroupLayout && shaderOptionGroupLayout->GetHash() != loopLocal_ShaderOptionGroupLayout->GetHash())
                    {
                        AZ_Error(
                            ShaderVariantAssetBuilderName,
                            false,
                            "There was a discrepancy in shader options between %s and %s",
                            previousLoopApiName.c_str(),
                            thisLoopApiName.data());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }

                    // Check if there's a supervariant that needs to generate the variants
                    if (!usesSpecialization || !loopLocal_ShaderOptionGroupLayout->IsFullySpecialized())
                    {
                        usesVariants = true;
                    }
                    shaderOptionGroupLayout = loopLocal_ShaderOptionGroupLayout;
                }
                previousLoopApiName = thisLoopApiName;
            }

            if (!usesVariants)
            {
                // No need to create the variant tree since all supervariants are fully specialized. Exit gracefully.
                AZ_TracePrintf(
                    ShaderVariantAssetBuilderName,
                    "No azshadervarianttree is produced on behalf of %s because all valid RHI backends are using specialization constants for shader options.\n",
                    shaderSourceFileFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
                return;
            }

            AZStd::vector<RPI::ShaderVariantListSourceData::VariantInfo> variantInfos;
            variantInfos.reserve(hashedVariantListDescriptor.m_hashedVariants.size());
            for (const auto& hashedVariantInfo : hashedVariantListDescriptor.m_hashedVariants)
            {
                variantInfos.push_back(hashedVariantInfo.m_variantInfo);
            }
            
            RPI::ShaderVariantTreeAssetCreator shaderVariantTreeAssetCreator;
            shaderVariantTreeAssetCreator.Begin(Uuid::CreateRandom());
            shaderVariantTreeAssetCreator.SetShaderOptionGroupLayout(*shaderOptionGroupLayout);
            shaderVariantTreeAssetCreator.SetVariantInfos(variantInfos);
            Data::Asset<RPI::ShaderVariantTreeAsset> shaderVariantTreeAsset;
            if (!shaderVariantTreeAssetCreator.End(shaderVariantTreeAsset))
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "Failed to build Shader Variant Tree Asset");
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            AZStd::string filename = AZStd::string::format("%s.%s", shaderName.c_str(), RPI::ShaderVariantTreeAsset::Extension);
            AZStd::string assetPath;
            AZ::StringFunc::Path::ConstructFull(request.m_tempDirPath.c_str(), filename.c_str(), assetPath, true);
            if (!AZ::Utils::SaveObjectToFile(assetPath, AZ::DataStream::ST_BINARY, shaderVariantTreeAsset.Get()))
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "Failed to save Shader Variant Tree Asset to \"%s\"", assetPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            AssetBuilderSDK::JobProduct assetProduct;
            assetProduct.m_productSubID = RPI::ShaderVariantTreeAsset::ProductSubID;
            assetProduct.m_productFileName = assetPath;
            assetProduct.m_productAssetType = azrtti_typeid<RPI::ShaderVariantTreeAsset>();
            assetProduct.m_dependenciesHandled = true; // This builder has no dependencies to output
            response.m_outputProducts.push_back(assetProduct);

            AZ_TracePrintf(ShaderVariantAssetBuilderName, "Shader Variant Tree Asset [%s] compiled successfully.\n", assetPath.c_str());

            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success; 
        }


        void ShaderVariantAssetBuilder::ProcessShaderVariantJob(
            const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
        {
            AssetBuilderSDK::JobCancelListener jobCancelListener(request.m_jobId);

            AZStd::string hashedVariantBatchFullPath;
            AZ::StringFunc::Path::ConstructFull(
                request.m_watchFolder.data(), request.m_sourceFile.data(), hashedVariantBatchFullPath, true);


            AZStd::string hashedVariantBatchDescriptorString;
            if (!request.m_jobDescription.m_jobParameters.contains(ShaderVariantBatchJobParam))
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "Missing job Parameter: ShaderVariantBatchJobParam");
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }
            hashedVariantBatchDescriptorString = request.m_jobDescription.m_jobParameters.at(ShaderVariantBatchJobParam);

            HashedVariantListSourceData hashedVariantBatchDescriptor;
            if (!RPI::JsonUtils::LoadObjectFromJsonString(
                    hashedVariantBatchDescriptorString, hashedVariantBatchDescriptor))
            {
                AZ_Assert(false, "Failed to parse Hashed Variant Batch Descriptor JSON [%s]", hashedVariantBatchFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            const AZStd::string& shaderSourceFileFullPath = hashedVariantBatchDescriptor.m_shaderPath;
            AZStd::string shaderFileName;
            AZ::StringFunc::Path::GetFileName(shaderSourceFileFullPath.c_str(), shaderFileName);

            RPI::ShaderSourceData shaderSourceDescriptor;
            AZStd::shared_ptr<ShaderFiles> sources =
                ShaderBuilderUtility::PrepareSourceInput(ShaderVariantAssetBuilderName, shaderSourceFileFullPath, shaderSourceDescriptor);

            IShaderCompilerBackend* compilerBackend = FindBackendForShaderSource(shaderSourceDescriptor);
            if (!compilerBackend)
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            // Request the list of valid shader platform interfaces for the target platform.
            AZStd::vector<RHI::ShaderPlatformInterface*> platformInterfaces =
                ShaderBuilderUtility::DiscoverEnabledShaderPlatformInterfaces(request.m_platformInfo, shaderSourceDescriptor);
            if (platformInterfaces.empty())
            {
                // No work to do. Exit gracefully.
                AZ_TracePrintf(
                    ShaderVariantAssetBuilderName,
                    "No azshader is produced on behalf of %s because all valid RHI backends were disabled for this shader.\n",
                    shaderSourceFileFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
                return;
            }

            if (shaderSourceDescriptor.m_programSettings.m_entryPoints.empty())
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "ProgramSettings must specify entry points.");
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }
            MapOfStringToStageType shaderEntryPoints;
            for (const auto& entryPoint : shaderSourceDescriptor.m_programSettings.m_entryPoints)
            {
                shaderEntryPoints[entryPoint.m_name] = entryPoint.m_type;
            }

            auto supervariantList = ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceDescriptor);

            ShaderBuildArgumentsManager buildArgsManager;
            buildArgsManager.Init();
            // A job always runs on behalf of an Asset Processing platform (aka PlatformInfo).
            // Let's merge the shader build arguments of the current PlatformInfo with the global
            // set of arguments.
            const auto platformName = ShaderBuilderUtility::GetPlatformNameFromPlatformInfo(request.m_platformInfo);
            buildArgsManager.PushArgumentScope(platformName);

            //! The ShaderOptionGroupLayout is common across all RHIs & Supervariants
            RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout = nullptr;

            // Generate shaders for each of those ShaderPlatformInterfaces.
            for (RHI::ShaderPlatformInterface* shaderPlatformInterface : platformInterfaces)
            {
                AZStd::string apiName(shaderPlatformInterface->GetAPIName().GetCStr());
                AZ_TraceContext("Platform API", apiName);

                buildArgsManager.PushArgumentScope(apiName);
                buildArgsManager.PushArgumentScope(
                    shaderSourceDescriptor.m_removeBuildArguments,
                    shaderSourceDescriptor.m_addBuildArguments,
                    shaderSourceDescriptor.m_definitions);

                // Loop through all the Supervariants.
                for (uint32_t supervariantIndexCounter = 0;
                    supervariantIndexCounter < supervariantList.size();
                    ++supervariantIndexCounter)
                {
                    const auto& supervariantInfo = supervariantList[supervariantIndexCounter];
                    RPI::SupervariantIndex supervariantIndex(supervariantIndexCounter);

                    // Check if we were canceled before we do any heavy processing of
                    // the shader variant data.
                    if (jobCancelListener.IsCancelled())
                    {
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                        return;
                    }

                    buildArgsManager.PushArgumentScope(
                        supervariantInfo.m_removeBuildArguments, supervariantInfo.m_addBuildArguments, supervariantInfo.m_definitions);

                    AZStd::string shaderStemNamePrefix = shaderFileName;
                    if (supervariantIndex.GetIndex() > 0)
                    {
                        shaderStemNamePrefix += AZStd::string::format("-%s", supervariantInfo.m_name.GetCStr());
                    }

                    // Everything a variant compile needs from this shader's frontend products —
                    // option layout, specialization flag, stage source, module closure and any
                    // per-API pipeline layout — comes through the language backend.
                    VariantCompilationInputsRequest inputsRequest;
                    inputsRequest.m_builderName = ShaderVariantAssetBuilderName;
                    inputsRequest.m_platformInfo = &request.m_platformInfo;
                    inputsRequest.m_shaderPlatformInterface = shaderPlatformInterface;
                    inputsRequest.m_shaderDescriptorPath = shaderSourceFileFullPath;
                    inputsRequest.m_shaderSourceFullPath = sources->m_azslSourceFullPath;
                    inputsRequest.m_supervariantIndex = supervariantIndex.GetIndex();
                    inputsRequest.m_tempDirPath = request.m_tempDirPath;
                    inputsRequest.m_entryPoints = &shaderEntryPoints;
                    inputsRequest.m_buildArguments = &buildArgsManager.GetCurrentArguments();

                    // First pass loads just the option layout and specialization flag, so fully
                    // specialized supervariants skip without touching any stage inputs.
                    inputsRequest.m_loadStageInputs = false;
                    auto layoutOutcome = compilerBackend->LoadVariantCompilationInputs(inputsRequest);
                    if (!layoutOutcome.IsSuccess())
                    {
                        AZ_Error(ShaderVariantAssetBuilderName, false, "%s", layoutOutcome.GetError().c_str());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    shaderOptionGroupLayout = layoutOutcome.GetValue().m_shaderOptionGroupLayout;
                    const bool usesSpecializationConstants = layoutOutcome.GetValue().m_useSpecializationConstants;

                    if (usesSpecializationConstants && shaderOptionGroupLayout->IsFullySpecialized())
                    {
                        // No need to create the shader variants since all supervariants are fully specialized.
                        AZ_TracePrintf(
                            ShaderVariantAssetBuilderName,
                            "No azshaderVariant is produced on behalf of %s, super variant %s, because it's using specialization "
                            "constants "
                            "for shader options.\n",
                            shaderSourceFileFullPath.c_str(),
                            supervariantInfo.m_name.GetCStr());
                        buildArgsManager.PopArgumentScope();
                        continue;
                    }

                    inputsRequest.m_loadStageInputs = true;
                    auto inputsOutcome = compilerBackend->LoadVariantCompilationInputs(inputsRequest);
                    if (!inputsOutcome.IsSuccess())
                    {
                        AZ_Error(ShaderVariantAssetBuilderName, false, "%s", inputsOutcome.GetError().c_str());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    // Kept alive through the variant loop: the pipeline layout it may hold primes
                    // per-API state the stage compiles rely on.
                    const VariantCompilationInputs variantInputs = inputsOutcome.TakeValue();

                    // Setup the shader variant creation context:
                    ShaderVariantCreationContext shaderVariantCreationContext = { *shaderPlatformInterface,
                                                                                  request.m_platformInfo,
                                                                                  buildArgsManager.GetCurrentArguments(),
                                                                                  request.m_tempDirPath,
                                                                                  shaderSourceDescriptor,
                                                                                  *shaderOptionGroupLayout.get(),
                                                                                  shaderEntryPoints,
                                                                                  Uuid::CreateRandom(),
                                                                                  shaderStemNamePrefix,
                                                                                  variantInputs.m_stageSourcePath,
                                                                                  variantInputs.m_stageSourceCode,
                                                                                  variantInputs.m_moduleClosurePath,
                                                                                  usesSpecializationConstants };

                    // Preserve the Temp folder when shaders are compiled with debug symbols
                    // or because the ShaderSourceData has m_keepTempFolder set to true.
                    response.m_keepTempFolder |= shaderVariantCreationContext.m_shaderBuildArguments.m_generateDebugInfo ||
                        shaderSourceDescriptor.m_keepTempFolder || RHI::IsGraphicsDevModeEnabled();

                    for (const HashedVariantInfoSourceData& hashedVariantInfoDescriptor : hashedVariantBatchDescriptor.m_hashedVariants)
                    {
                        const RPI::ShaderVariantListSourceData::VariantInfo& variantInfo = hashedVariantInfoDescriptor.m_variantInfo;

                        AZStd::optional<RHI::ShaderPlatformInterface::ByProducts> outputByproducts;
                        auto shaderVariantAssetOutcome =
                            CreateShaderVariantAsset(variantInfo, shaderVariantCreationContext, outputByproducts);
                        if (!shaderVariantAssetOutcome.IsSuccess())
                        {
                            AZ_Error(ShaderVariantAssetBuilderName, false, "%s\n", shaderVariantAssetOutcome.GetError().c_str());
                            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                            return;
                        }
                        Data::Asset<RPI::ShaderVariantAsset> shaderVariantAsset = shaderVariantAssetOutcome.TakeValue();

                        // Time to save the asset in the tmp folder so it ends up in the Cache folder.
                        const uint32_t productSubID = RPI::ShaderVariantAsset::MakeAssetProductSubId(
                            shaderPlatformInterface->GetAPIUniqueIndex(), supervariantIndex.GetIndex(), shaderVariantAsset->GetStableId());
                        AssetBuilderSDK::JobProduct assetProduct;
                        if (!SerializeOutShaderVariantAsset(
                                shaderVariantAsset,
                                shaderStemNamePrefix,
                                request.m_tempDirPath,
                                *shaderPlatformInterface,
                                productSubID,
                                assetProduct))
                        {
                            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                            return;
                        }
                        response.m_outputProducts.push_back(assetProduct);

                        if (outputByproducts)
                        {
                            // add byproducts as job output products:
                            uint32_t subProductType = RPI::ShaderVariantAsset::ShaderVariantAssetSubProductType;
                            for (const AZStd::string& byproduct : outputByproducts.value().m_intermediatePaths)
                            {
                                AssetBuilderSDK::JobProduct jobProduct;
                                jobProduct.m_productFileName = byproduct;
                                jobProduct.m_productAssetType = Uuid::CreateName("DebugInfoByProduct-PdbOrDxilTxt");
                                jobProduct.m_productSubID = RPI::ShaderVariantAsset::MakeAssetProductSubId(
                                    shaderPlatformInterface->GetAPIUniqueIndex(),
                                    supervariantIndex.GetIndex(),
                                    shaderVariantAsset->GetStableId(),
                                    subProductType++);
                                response.m_outputProducts.push_back(AZStd::move(jobProduct));
                            }
                        }
                    }

                    buildArgsManager.PopArgumentScope(); // Pop the supervariant build arguments.
                } // End of supervariant for block

                buildArgsManager.PopArgumentScope(); // Pop the .shader build arguments.
                buildArgsManager.PopArgumentScope(); // Pop the RHI build arguments.
            }

            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        }


        bool ShaderVariantAssetBuilder::SerializeOutShaderVariantAsset(
            const Data::Asset<RPI::ShaderVariantAsset> shaderVariantAsset, const AZStd::string& shaderStemNamePrefix,
            const AZStd::string& tempDirPath,
            const RHI::ShaderPlatformInterface& shaderPlatformInterface, const uint32_t productSubID, AssetBuilderSDK::JobProduct& assetProduct)
        {
            AZStd::string filename = AZStd::string::format(
                "%s_%s_%u.%s", shaderStemNamePrefix.c_str(), shaderPlatformInterface.GetAPIName().GetCStr(),
                shaderVariantAsset->GetStableId().GetIndex(), RPI::ShaderVariantAsset::Extension);

            AZStd::string assetPath;
            AZ::StringFunc::Path::ConstructFull(tempDirPath.c_str(), filename.c_str(), assetPath, true);

            if (!AZ::Utils::SaveObjectToFile(assetPath, AZ::DataStream::ST_BINARY, shaderVariantAsset.Get()))
            {
                AZ_Error(ShaderVariantAssetBuilderName, false, "Failed to save Shader Variant Asset to \"%s\"", assetPath.c_str());
                return false;
            }

            assetProduct.m_productSubID = productSubID;
            assetProduct.m_productFileName = assetPath;
            assetProduct.m_productAssetType = azrtti_typeid<RPI::ShaderVariantAsset>();
            assetProduct.m_dependenciesHandled = true; // This builder has no dependencies to output

            AZ_TracePrintf(ShaderVariantAssetBuilderName, "Shader Variant Asset [%s] compiled successfully.\n", assetPath.c_str());
            return true;
        }


        AZ::Outcome<Data::Asset<RPI::ShaderVariantAsset>, AZStd::string> ShaderVariantAssetBuilder::CreateShaderVariantAsset(
            const RPI::ShaderVariantListSourceData::VariantInfo& shaderVariantInfo,
            ShaderVariantCreationContext& creationContext,
            AZStd::optional<RHI::ShaderPlatformInterface::ByProducts>& outputByproducts)
        {
            // Temporary structure used for sorting and caching intermediate results
            struct OptionCache
            {
                AZ::Name m_optionName;
                AZ::Name m_valueName;
                RPI::ShaderOptionIndex m_optionIndex; // Cached m_optionName
                RPI::ShaderOptionValue m_value; // Cached m_valueName
            };
            AZStd::vector<OptionCache> optionList;
            // We can not have more options than the number of options in the layout:
            optionList.reserve(creationContext.m_shaderOptionGroupLayout.GetShaderOptionCount());

            // This loop will validate and cache the indices for each option value:
            for (const auto& shaderOption : shaderVariantInfo.m_options)
            {
                Name optionName{shaderOption.first};
                Name optionValue{shaderOption.second};

                RPI::ShaderOptionIndex optionIndex = creationContext.m_shaderOptionGroupLayout.FindShaderOptionIndex(optionName);
                if (optionIndex.IsNull())
                {
                    return AZ::Failure(AZStd::string::format("Invalid shader option: %s", optionName.GetCStr()));
                }

                const RPI::ShaderOptionDescriptor& option = creationContext.m_shaderOptionGroupLayout.GetShaderOption(optionIndex);
                RPI::ShaderOptionValue value = option.FindValue(optionValue);
                if (value.IsNull())
                {
                    return AZ::Failure(
                        AZStd::string::format("Invalid value (%s) for shader option: %s", optionValue.GetCStr(), optionName.GetCStr()));
                }

                optionList.push_back(OptionCache{optionName, optionValue, optionIndex, value});
            }

            // Create one instance of the shader variant
            RPI::ShaderOptionGroup optionGroup(&creationContext.m_shaderOptionGroupLayout);

            //! The variant's option assignments in listing order. How they reach the compiled
            //! bytecode is the language backend's business (AZSL prepends #define macros onto
            //! the source, Slang links a generated option-values module).
            AZStd::vector<ShaderOptionAssignment> variantOptionAssignments;
            variantOptionAssignments.reserve(optionList.size());

            // We want to go over all options listed in the variant and set their respective values
            // This loop will populate the optionGroup and the assignments in order of the option priority
            for (const auto& optionCache : optionList)
            {
                const RPI::ShaderOptionDescriptor& option = creationContext.m_shaderOptionGroupLayout.GetShaderOption(optionCache.m_optionIndex);

                // Assign the option value specified in the variant:
                option.Set(optionGroup, optionCache.m_value);

                // We have already confirmed the names and values are valid.
                variantOptionAssignments.push_back({optionCache.m_optionName, optionCache.m_valueName});
            }

            AZ_TracePrintf(ShaderVariantAssetBuilderName, "Variant StableId: %u", shaderVariantInfo.m_stableId);
            AZ_TracePrintf(ShaderVariantAssetBuilderName, "Variant Shader Options: %s", optionGroup.ToString().c_str());

            const RPI::ShaderVariantStableId shaderVariantStableId{shaderVariantInfo.m_stableId};

            // By this time the optionGroup was populated with all option values for the variant and
            // the m_shaderCodePrefix contains all option related preprocessing macros
            // Let's add the requested variant:
            RPI::ShaderVariantAssetCreator variantCreator;
            RPI::ShaderOptionGroup shaderOptions{&creationContext.m_shaderOptionGroupLayout, optionGroup.GetShaderVariantId()};
            variantCreator.Begin(
                creationContext.m_shaderVariantAssetId, optionGroup.GetShaderVariantId(), shaderVariantStableId,
                shaderOptions.IsFullySpecified());

            // Search roots for backends that recompile from source at stage time; the source's
            // own directory is the highest-priority location.
            AZStd::string sourceFolderPath;
            AzFramework::StringFunc::Path::GetFolderPath(creationContext.m_stageSourcePath.c_str(), sourceFolderPath);
            const AZStd::vector<AZStd::string> stageIncludePaths =
                BuildListOfIncludeDirectories(ShaderVariantAssetBuilderName, sourceFolderPath.c_str());

            // Compile the target-language source to platform bytecode through the shader
            // language backend that owns this shader's source extension.
            IShaderCompilerBackend* compilerBackend = FindBackendForShaderSource(creationContext.m_shaderSourceDataDescriptor);
            if (!compilerBackend)
            {
                return AZ::Failure(AZStd::string("No shader compiler backend claims this shader's source extension"));
            }

            const AZStd::unordered_map<AZStd::string, RPI::ShaderStageType>& shaderEntryPoints = creationContext.m_shaderEntryPoints;
            for (const auto& shaderEntryPoint : shaderEntryPoints)
            {
                auto shaderEntryName = shaderEntryPoint.first;
                auto shaderStageType = shaderEntryPoint.second;

                AZ_TracePrintf(ShaderVariantAssetBuilderName, "Entry Point: %s", shaderEntryName.c_str());
                AZ_TracePrintf(ShaderVariantAssetBuilderName, "Begin compiling shader function \"%s\"", shaderEntryName.c_str());

                auto assetBuilderShaderType = ShaderBuilderUtility::ToAssetBuilderShaderType(shaderStageType);

                StageInput stageInput;
                stageInput.m_builderName = ShaderVariantAssetBuilderName;
                stageInput.m_platformInfo = &creationContext.m_platformInfo;
                stageInput.m_shaderPlatformInterface = &creationContext.m_shaderPlatformInterface;
                stageInput.m_sourcePath = creationContext.m_stageSourcePath;
                stageInput.m_sourceCode = creationContext.m_stageSourceContent;
                stageInput.m_entryPointName = shaderEntryName;
                stageInput.m_stage = assetBuilderShaderType;
                stageInput.m_includePaths = stageIncludePaths;
                stageInput.m_tempDirPath = creationContext.m_tempDirPath;
                stageInput.m_buildArguments = &creationContext.m_shaderBuildArguments;
                stageInput.m_useSpecializationConstants = creationContext.m_useSpecializationConstants;
                stageInput.m_variantOptionAssignments = variantOptionAssignments;
                stageInput.m_variantOptionValues = variantOptionAssignments.empty() ? nullptr : &optionGroup;
                stageInput.m_variantStableId = shaderVariantInfo.m_stableId;
                stageInput.m_stemNamePrefix = creationContext.m_shaderStemNamePrefix;
                stageInput.m_moduleClosurePath = creationContext.m_moduleClosurePath;

                AZ::Outcome<StageResult, AZStd::string> stageOutcome = compilerBackend->CompileStage(stageInput);
                if (!stageOutcome.IsSuccess())
                {
                    return AZ::Failure(stageOutcome.TakeError());
                }
                StageResult stageResult = stageOutcome.TakeValue();
                RHI::ShaderPlatformInterface::StageDescriptor descriptor = AZStd::move(stageResult.m_descriptor);
                // bubble up the byproducts to the caller by moving them to the context.
                outputByproducts.emplace(AZStd::move(descriptor.m_byProducts));

                RHI::Ptr<RHI::ShaderStageFunction> shaderStageFunction = creationContext.m_shaderPlatformInterface.CreateShaderStageFunction(descriptor);
                variantCreator.SetShaderFunction(ToRHIShaderStage(assetBuilderShaderType), shaderStageFunction);

                if (descriptor.m_byProducts.m_dynamicBranchCount != AZ::RHI::ShaderPlatformInterface::ByProducts::UnknownDynamicBranchCount)
                {
                    AZ_TracePrintf(
                        ShaderVariantAssetBuilderName, "Finished compiling shader function. Number of dynamic branches: %u",
                        descriptor.m_byProducts.m_dynamicBranchCount);
                }
                else
                {
                    AZ_TracePrintf(
                        ShaderVariantAssetBuilderName, "Finished compiling shader function. Number of dynamic branches: unknown");
                }
            }

            if (shaderVariantInfo.m_enableRegisterAnalysis)
            {
                if (creationContext.m_shaderPlatformInterface.GetAPIName().GetStringView() == "vulkan")
                {
                    AZ::IO::FixedMaxPath projectBuildPath = AZ::Utils::GetExecutableDirectory();
                    projectBuildPath = projectBuildPath.RemoveFilename(); // profile
                    projectBuildPath = projectBuildPath.RemoveFilename(); // bin

                    AZ::IO::FixedMaxPath spirvPath(AZStd::string_view(creationContext.m_tempDirPath));
                    spirvPath /= AZ::IO::FixedMaxPathString::format(
                        "%s_vulkan_%u.spirv.bin", creationContext.m_shaderStemNamePrefix.c_str(), shaderVariantInfo.m_stableId);

                    AZStd::string rgaCommand = AZStd::string::format(
                        "-s vk-spv-offline --isa ./disassem_%u.txt --livereg ./livereg_%u.txt --asic %s",
                        shaderVariantInfo.m_stableId,
                        shaderVariantInfo.m_stableId,
                        shaderVariantInfo.m_asic.c_str());

                    AZStd::string RgaPath;
                    if (creationContext.m_platformInfo.m_identifier == "pc")
                    {
                        RgaPath = "\\_deps\\rga-src\\rga.exe";
                    }
                    else
                    {
                        RgaPath = "/_deps/rga-src/rga";
                    }

                    AZStd::string command = AZStd::string::format(
                        "%s%s %s %s", projectBuildPath.c_str(), RgaPath.c_str(), rgaCommand.c_str(), spirvPath.c_str());
                    AZ_TracePrintf(ShaderVariantAssetBuilderName, "Rga command %s\n", command.c_str());

                    AZStd::vector<AZStd::string> fullCommand;
                    fullCommand.push_back(command);
                    AZStd::string failMessage;
                    if (LaunchRadeonGPUAnalyzer(fullCommand, creationContext.m_tempDirPath, failMessage))
                    {
                        // add rga output to the by product list
                        outputByproducts->m_intermediatePaths.insert(AZStd::string::format(
                            "./%s_disassem_%u_frag.txt", shaderVariantInfo.m_asic.c_str(), shaderVariantInfo.m_stableId));
                        outputByproducts->m_intermediatePaths.insert(AZStd::string::format(
                            "./%s_livereg_%u_frag.txt", shaderVariantInfo.m_asic.c_str(), shaderVariantInfo.m_stableId));
                    }
                    else
                    {
                        AZ_Warning(ShaderVariantAssetBuilderName, false, "%s", failMessage.c_str());
                    }
                }
                else
                {
                    AZ_Warning(
                        ShaderVariantAssetBuilderName,
                        false,
                        "Current platform is %s, register analysis is only available on Vulkan for now.",
                        creationContext.m_shaderPlatformInterface.GetAPIName().GetCStr());
                }
            }

            Data::Asset<RPI::ShaderVariantAsset> shaderVariantAsset;
            variantCreator.End(shaderVariantAsset);
            return AZ::Success(AZStd::move(shaderVariantAsset));
        }


        bool ShaderVariantAssetBuilder::LaunchRadeonGPUAnalyzer(AZStd::vector<AZStd::string> command, const AZStd::string& workingDirectory, AZStd::string& failMessage)
        {
            AzFramework::ProcessLauncher::ProcessLaunchInfo processLaunchInfo;
            processLaunchInfo.m_commandlineParameters.emplace<AZStd::vector<AZStd::string>>(AZStd::move(command));
            processLaunchInfo.m_workingDirectory = workingDirectory;
            processLaunchInfo.m_showWindow = false;
            AzFramework::ProcessWatcher* watcher =
                AzFramework::ProcessWatcher::LaunchProcess(processLaunchInfo, AzFramework::COMMUNICATOR_TYPE_STDINOUT);
            if (!watcher)
            {
                failMessage = AZStd::string("Rga executable can not be launched");
                return false;
            }

            AZStd::unique_ptr<AzFramework::ProcessWatcher> watcherPtr = AZStd::unique_ptr<AzFramework::ProcessWatcher>(watcher);

            AZStd::string errorMessages;
            AZStd::string outputMessages;
            auto pumpOuputStreams = [&watcherPtr, &errorMessages, &outputMessages]()
            {
                auto communicator = watcherPtr->GetCommunicator();

                // Instead of collecting all the output in a giant string, it would be better to report
                // the chunks of messages as they arrive, but this should be good enough for now.
                if (auto byteCount = communicator->PeekError())
                {
                    AZStd::string chunk;
                    chunk.resize(byteCount);
                    communicator->ReadError(chunk.data(), byteCount);
                    errorMessages += chunk;
                }

                if (auto byteCount = communicator->PeekOutput())
                {
                    AZStd::string chunk;
                    chunk.resize(byteCount);
                    communicator->ReadOutput(chunk.data(), byteCount);
                    outputMessages += chunk;
                }
            };

            uint32_t exitCode = 0;
            bool timedOut = false;

            const AZStd::sys_time_t maxWaitTimeSeconds = 5;
            const AZStd::sys_time_t startTimeSeconds = AZStd::GetTimeNowSecond();

            while (watcherPtr->IsProcessRunning(&exitCode))
            {
                const AZStd::sys_time_t currentTimeSeconds = AZStd::GetTimeNowSecond();
                if (currentTimeSeconds - startTimeSeconds > maxWaitTimeSeconds)
                {
                    timedOut = true;
                    static const uint32_t TimeOutExitCode = 121;
                    exitCode = TimeOutExitCode;
                    watcherPtr->TerminateProcess(TimeOutExitCode);
                    break;
                }
                else
                {
                    pumpOuputStreams();
                }
            }

            AZ_Assert(!watcherPtr->IsProcessRunning(), "Rga execution failed to terminate");

            // Pump one last time to make sure the streams have been flushed
            pumpOuputStreams();

            if (timedOut)
            {
                failMessage = AZStd::string("Rga execution timed out");
                return false;
            }

            if (exitCode != 0)
            {
                failMessage = AZStd::string::format("Rga process failed, exit code %u", exitCode);
                return false;
            }

            if (!errorMessages.empty())
            {
                failMessage = AZStd::string::format("Rga report error message %s", errorMessages.c_str());
                return false;
            }

            if (!outputMessages.empty() && outputMessages.contains("Error"))
            {
                failMessage = AZStd::string::format("Rga report error message %s", outputMessages.c_str());
                return false;
            }

            return true;
        }
    } // ShaderBuilder
} // AZ
