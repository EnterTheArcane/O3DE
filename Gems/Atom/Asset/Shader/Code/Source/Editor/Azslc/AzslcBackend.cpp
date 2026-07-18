/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AzslcBackend.h"

#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <Atom/RHI.Edit/Utils.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <AzslCompiler.h>
#include <AzslData.h>
#include <CommonFiles/CommonTypes.h>
#include <CommonFiles/Preprocessor.h>
#include <Editor/Azslc/AzslcReflectionAdapter.h>
#include <Editor/ShaderBuilderUtility.h>
#include <SrgLayoutUtility.h>

namespace AZ::ShaderBuilder
{
    AZStd::string_view AzslcBackend::GetName() const
    {
        return "azslc";
    }

    AZStd::span<const AZStd::string_view> AzslcBackend::GetSourceExtensions() const
    {
        static constexpr AZStd::string_view sourceExtensions[] = {
            ".azsl",
        };
        return sourceExtensions;
    }

    bool AzslcBackend::CanCompileTarget([[maybe_unused]] const RHI::ShaderTargetDescriptor& targetDescriptor) const
    {
        // The legacy pipeline predates target descriptors: every RHI it runs on carries its own
        // compile path in ShaderPlatformInterface::CompilePlatformInternal, descriptor or not.
        return true;
    }

    //! The search will start in @currentFolderPath.
    //! if the file is not found then it searches in order of appearence in @includeDirectories.
    //! If the search yields no existing file it returns an empty string.
    static AZStd::string DiscoverFullPath(AZStd::string_view normalizedRelativePath, AZStd::string_view currentFolderPath, AZStd::span<const AZStd::string> includeDirectories)
    {
        AZStd::string fullPath;
        AzFramework::StringFunc::Path::Join(currentFolderPath.data(), normalizedRelativePath.data(), fullPath);
        if (AZ::IO::SystemFile::Exists(fullPath.c_str()))
        {
            return fullPath;
        }

        for (const AZStd::string& includeDir : includeDirectories)
        {
            AzFramework::StringFunc::Path::Join(includeDir.c_str(), normalizedRelativePath.data(), fullPath);
            if (AZ::IO::SystemFile::Exists(fullPath.c_str()))
            {
                return fullPath;
            }
        }

        return "";
    }

    // Appends to @includedFiles normalized paths of possible future locations of the file @normalizedRelativePath.
    // The future locations are each directory listed in @includeDirectories joined with @normalizedRelativePath.
    // This function is called when an included file doesn't exist but We need to declare source dependency so a .shader
    // asset is rebuilt when the missing file appears in the future.
    static void AppendListOfPossibleFutureLocations(AZStd::unordered_set<AZStd::string>& includedFiles, AZStd::string_view normalizedRelativePath, AZStd::string_view currentFolderPath, AZStd::span<const AZStd::string> includeDirectories)
    {
        AZStd::string fullPath;
        AzFramework::StringFunc::Path::Join(currentFolderPath.data(), normalizedRelativePath.data(), fullPath);
        includedFiles.insert(fullPath);
        for (const AZStd::string& includeDir : includeDirectories)
        {
            AzFramework::StringFunc::Path::Join(includeDir.c_str(), normalizedRelativePath.data(), fullPath);
            includedFiles.insert(fullPath);
        }
    }

    //! Parses, using depth-first recursive approach, azsl files. Looks for '#include <foo/bar/blah.h>' or '#include "foo/bar/blah.h"' lines
    //! and in turn parses the included files.
    //! The included files are searched in the directories listed in @includeDirectories. Basically it's a similar approach
    //! as how most C-preprocessors would find included files.
    static void GetListOfIncludedFiles(AZStd::string_view sourceFilePath, AZStd::span<const AZStd::string> includeDirectories,
        const ShaderBuilderUtility::IncludedFilesParser& includedFilesParser, AZStd::unordered_set<AZStd::string>& includedFiles)
    {
        auto outcome = includedFilesParser.ParseFileAndGetIncludedFiles(sourceFilePath);
        if (!outcome.IsSuccess())
        {
            AZ_Warning("AzslcBackend", false, outcome.GetError().c_str());
            return;
        }

        // Cache the path of the folder where @sourceFilePath is located.
        AZStd::string sourceFileFolderPath;
        {
            AZStd::string drive;
            AzFramework::StringFunc::Path::Split(sourceFilePath.data(), &drive, &sourceFileFolderPath);
            if (!drive.empty())
            {
                AzFramework::StringFunc::Path::Join(drive.c_str(), sourceFileFolderPath.c_str(), sourceFileFolderPath);
            }
        }

        auto listOfRelativePaths = outcome.TakeValue();
        for (const AZStd::string& relativePath : listOfRelativePaths)
        {
            auto fullPath = DiscoverFullPath(relativePath, sourceFileFolderPath, includeDirectories);
            if (fullPath.empty())
            {
                // The file doesn't exist in any of the includeDirectories. It doesn't exist in @sourceFileFolderPath either.
                // The file may appear in the future in one of those directories, We must build an exhaustive list
                // of full file paths where the file may appear in the future.
                AppendListOfPossibleFutureLocations(includedFiles, relativePath, sourceFileFolderPath, includeDirectories);
                continue;
            }

            // Add the file to the list and keep parsing recursively.
            if (includedFiles.contains(fullPath))
            {
                continue;
            }
            includedFiles.insert(fullPath);
            GetListOfIncludedFiles(fullPath, includeDirectories, includedFilesParser, includedFiles);
        }
    }

    void AzslcBackend::EnumerateSourceDependencies(
        AZStd::string_view shaderSourceFullPath,
        AZStd::span<const AZStd::string> includePaths,
        AZStd::unordered_set<AZStd::string>& sourceDependencies) const
    {
        const ShaderBuilderUtility::IncludedFilesParser includedFilesParser;
        GetListOfIncludedFiles(shaderSourceFullPath, includePaths, includedFilesParser, sourceDependencies);
    }

    AZ::Outcome<FrontendResult, AZStd::string> AzslcBackend::CompileFrontend(const FrontendInput& input)
    {
        const AZStd::string builderName(input.m_builderName);
        const AZStd::string apiName(input.m_shaderPlatformInterface->GetAPIName().GetCStr());
        const AZStd::string tempDirPath(input.m_tempDirPath);
        const AZStd::string sourceFullPath(input.m_shaderSourceFullPath);
        const AZStd::string stemName(input.m_stemName);

        // Each ShaderPlatformInterface has its own azsli header that needs to be prepended to
        // the AZSL file before preprocessing; the combination goes to a new temporary file.
        RHI::PrependArguments prependArguments;
        prependArguments.m_sourceFile = sourceFullPath.c_str();
        prependArguments.m_prependFile = input.m_shaderPlatformInterface->GetAzslHeader(*input.m_platformInfo);
        prependArguments.m_addSuffixToFileName = apiName.c_str();
        prependArguments.m_destinationFolder = tempDirPath.c_str();

        const AZStd::string prependedAzslFilePath = RHI::PrependFile(prependArguments);
        if (prependedAzslFilePath == sourceFullPath)
        {
            // The specific error is already reported by RHI::PrependFile().
            return AZ::Failure(AZStd::string::format("Failed to prepend the %s platform header to %s", apiName.c_str(), sourceFullPath.c_str()));
        }

        // Run the preprocessor.
        PreprocessorData preprocessorData;
        const AZStd::vector<AZStd::string> preprocessorArguments = AppendIncludePathsToArgumentList(
            input.m_buildArguments->m_preprocessorArguments,
            AZStd::vector<AZStd::string>(input.m_includePaths.begin(), input.m_includePaths.end()));
        const bool preprocessorSuccess = PreprocessFile(prependedAzslFilePath, preprocessorData, preprocessorArguments, true);
        if (RHI::ReportMessages(builderName.c_str(), preprocessorData.diagnostics, !preprocessorSuccess))
        {
            return AZ::Failure(AZStd::string::format("Failed to preprocess %s", prependedAzslFilePath.c_str()));
        }

        // Dump the preprocessed string as a flat AZSL file with extension .azslin, which will be
        // given to AZSLC to generate the HLSL file.
        const AZStd::string azslinFullPath =
            ShaderBuilderUtility::DumpPreprocessedCode(builderName.c_str(), preprocessorData.code, tempDirPath, stemName, apiName);
        if (azslinFullPath.empty())
        {
            return AZ::Failure(AZStd::string::format("Failed to write the preprocessed azslin file for %s", stemName.c_str()));
        }
        AZ_TracePrintf(builderName.c_str(), "Preprocessed AZSL File: %s \n", prependedAzslFilePath.c_str());

        // Ready to transpile the azslin file into HLSL.
        AzslCompiler azslc(azslinFullPath, tempDirPath);
        AZStd::string hlslFullPath = AZStd::string::format("%s_%s.hlsl", stemName.c_str(), apiName.c_str());
        AzFramework::StringFunc::Path::Join(tempDirPath.c_str(), hlslFullPath.c_str(), hlslFullPath, true);
        auto emitFullOutcome = azslc.EmitFullData(input.m_buildArguments->m_azslcArguments, hlslFullPath);
        if (!emitFullOutcome.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format("AZSLC failed to transpile %s", azslinFullPath.c_str()));
        }

        if (!input.m_entryPoints)
        {
            return AZ::Failure(AZStd::string::format("No entry points were provided for %s", sourceFullPath.c_str()));
        }

        const ShaderBuilderUtility::AzslSubProducts::Paths subProductPaths = emitFullOutcome.TakeValue();

        AzslData azslData(AZStd::make_shared<ShaderFiles>());
        azslData.m_preprocessedFullPath = azslinFullPath;
        RPI::ShaderResourceGroupLayoutList srgLayoutList;
        RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout = RPI::ShaderOptionGroupLayout::Create();
        BindingDependencies bindingDependencies;
        RootConstantData rootConstantData;
        bool usesSpecializationConstants = false;
        const AssetBuilderSDK::ProcessJobResultCode azslJsonReadResult = ShaderBuilderUtility::PopulateAzslDataFromJsonFiles(
            builderName.c_str(),
            subProductPaths,
            azslData,
            srgLayoutList,
            shaderOptionGroupLayout,
            bindingDependencies,
            rootConstantData,
            tempDirPath,
            usesSpecializationConstants);
        if (azslJsonReadResult != AssetBuilderSDK::ProcessJobResult_Success)
        {
            return AZ::Failure(AZStd::string::format("Failed to load the AZSLC reflection data for %s", azslinFullPath.c_str()));
        }

        auto reflectionOutcome = AzslcReflectionAdapter::BuildReflectionData(
            input.m_builderName,
            azslData,
            *shaderOptionGroupLayout,
            usesSpecializationConstants,
            bindingDependencies,
            rootConstantData,
            subProductPaths[ShaderBuilderUtility::AzslSubProducts::ia],
            subProductPaths[ShaderBuilderUtility::AzslSubProducts::om],
            *input.m_entryPoints,
            tempDirPath);
        if (!reflectionOutcome.IsSuccess())
        {
            return AZ::Failure(reflectionOutcome.TakeError());
        }

        FrontendResult result;
        result.m_reflection = reflectionOutcome.TakeValue();
        for (size_t i = 0; i < subProductPaths.size(); ++i)
        {
            result.m_subProducts.push_back({
                subProductPaths[i],
                aznumeric_cast<uint32_t>(ShaderBuilderUtility::AzslSubProducts::SubList[i])});
        }

        AZ::Outcome<AZStd::string, AZStd::string> hlslSourceCodeOutcome = AZ::Utils::ReadFile(hlslFullPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        if (!hlslSourceCodeOutcome.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format(
                "Failed to obtain shader source from %s. [%s]", hlslFullPath.c_str(), hlslSourceCodeOutcome.GetError().c_str()));
        }

        result.m_targetSourcePath = AZStd::move(hlslFullPath);
        result.m_targetSourceCode = hlslSourceCodeOutcome.TakeValue();
        return AZ::Success(AZStd::move(result));
    }

    //! Loads the option layout the frontend's options.json product declares, plus whether the
    //! supervariant compiles its options as specialization constants.
    static RPI::Ptr<RPI::ShaderOptionGroupLayout> LoadShaderOptionsGroupLayoutFromShaderAssetBuilder(
        const char* builderName,
        const RHI::ShaderPlatformInterface* shaderPlatformInterface,
        const AssetBuilderSDK::PlatformInfo& platformInfo,
        const AzslCompiler& azslCompiler,
        const AZStd::string& shaderSourceFileFullPath,
        const RPI::SupervariantIndex supervariantIndex,
        bool& useSpecializationConstants)
    {
        auto optionsGroupPathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            shaderPlatformInterface->GetAPIUniqueIndex(), platformInfo.m_identifier, shaderSourceFileFullPath, supervariantIndex.GetIndex(),
            AZ::RPI::ShaderAssetSubId::OptionsJson);
        if (!optionsGroupPathOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", optionsGroupPathOutcome.GetError().c_str());
            return nullptr;
        }
        auto optionsGroupJsonPath = optionsGroupPathOutcome.TakeValue();
        RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout = RPI::ShaderOptionGroupLayout::Create();
        // The shader options define what options are available, what are the allowed values/range
        // for each option and what is its default value.
        auto jsonOutcome = JsonSerializationUtils::ReadJsonFile(optionsGroupJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        if (!jsonOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", jsonOutcome.GetError().c_str());
            return nullptr;
        }
        if (!azslCompiler.ParseOptionsPopulateOptionGroupLayout(
                jsonOutcome.GetValue(), shaderOptionGroupLayout, useSpecializationConstants))
        {
            AZ_Error(builderName, false, "Failed to find a valid list of shader options!");
            return nullptr;
        }

        return shaderOptionGroupLayout;
    }

    static void LoadShaderFunctionsFromShaderAssetBuilder(
        const char* builderName,
        const RHI::ShaderPlatformInterface* shaderPlatformInterface, const AssetBuilderSDK::PlatformInfo& platformInfo,
        const AzslCompiler& azslCompiler, const AZStd::string& shaderSourceFileFullPath,
        const RPI::SupervariantIndex supervariantIndex,
        AzslFunctions& functions)
    {
        auto functionsJsonPathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            shaderPlatformInterface->GetAPIUniqueIndex(), platformInfo.m_identifier, shaderSourceFileFullPath, supervariantIndex.GetIndex(),
            AZ::RPI::ShaderAssetSubId::IaJson);
        if (!functionsJsonPathOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", functionsJsonPathOutcome.GetError().c_str());
            return;
        }

        auto functionsJsonPath = functionsJsonPathOutcome.TakeValue();
        auto jsonOutcome = JsonSerializationUtils::ReadJsonFile(functionsJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        if (!jsonOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", jsonOutcome.GetError().c_str());
            return;
        }
        if (!azslCompiler.ParseIaPopulateFunctionData(jsonOutcome.GetValue(), functions))
        {
            functions.clear();
            AZ_Error(builderName, false, "Failed to find shader functions.");
            return;
        }
    }

    static bool LoadSrgLayoutListFromShaderAssetBuilder(
        const char* builderName,
        const RHI::ShaderPlatformInterface* shaderPlatformInterface,
        const AssetBuilderSDK::PlatformInfo& platformInfo,
        const AzslCompiler& azslCompiler, const AZStd::string& shaderSourceFileFullPath,
        const RPI::SupervariantIndex supervariantIndex,
        RPI::ShaderResourceGroupLayoutList& srgLayoutList,
        RootConstantData& rootConstantData)
    {
        auto srgJsonPathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            shaderPlatformInterface->GetAPIUniqueIndex(), platformInfo.m_identifier, shaderSourceFileFullPath, supervariantIndex.GetIndex(), AZ::RPI::ShaderAssetSubId::SrgJson);
        if (!srgJsonPathOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", srgJsonPathOutcome.GetError().c_str());
            return false;
        }

        auto srgJsonPath = srgJsonPathOutcome.TakeValue();
        auto jsonOutcome = JsonSerializationUtils::ReadJsonFile(srgJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        if (!jsonOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", jsonOutcome.GetError().c_str());
            return false;
        }
        SrgDataContainer srgData;
        if (!azslCompiler.ParseSrgPopulateSrgData(jsonOutcome.GetValue(), srgData))
        {
            AZ_Error(builderName, false, "Failed to parse srg data");
            return false;
        }
        // Add all Shader Resource Group Assets that were defined in the shader code to the shader asset
        if (!SrgLayoutUtility::LoadShaderResourceGroupLayouts(builderName, srgData, srgLayoutList))
        {
            AZ_Error(builderName, false, "Failed to load ShaderResourceGroupLayouts");
            return false;
        }

        for (auto srgLayout : srgLayoutList)
        {
            if (!srgLayout->Finalize())
            {
                AZ_Error(builderName, false,
                    "Failed to finalize SrgLayout %s", srgLayout->GetName().GetCStr());
                return false;
            }
        }

        // Access the root constants reflection
        if (!azslCompiler.ParseSrgPopulateRootConstantData(
                jsonOutcome.GetValue(),
                rootConstantData)) // consuming data from --srg ("RootConstantBuffer" subjson section)
        {
            AZ_Error(builderName, false, "Failed to obtain root constant data reflection");
            return false;
        }

        return true;
    }

    static bool LoadBindingDependenciesFromShaderAssetBuilder(
        const char* builderName,
        const RHI::ShaderPlatformInterface* shaderPlatformInterface,
        const AssetBuilderSDK::PlatformInfo& platformInfo,
        const AzslCompiler& azslCompiler, const AZStd::string& shaderSourceFileFullPath,
        const RPI::SupervariantIndex supervariantIndex,
        BindingDependencies& bindingDependencies)
    {
        auto bindingsJsonPathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            shaderPlatformInterface->GetAPIUniqueIndex(), platformInfo.m_identifier, shaderSourceFileFullPath,     supervariantIndex.GetIndex(), AZ::RPI::ShaderAssetSubId::BindingdepJson);
        if (!bindingsJsonPathOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", bindingsJsonPathOutcome.GetError().c_str());
            return false;
        }

        auto bindingsJsonPath = bindingsJsonPathOutcome.TakeValue();
        auto jsonOutcome = JsonSerializationUtils::ReadJsonFile(bindingsJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        if (!jsonOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", jsonOutcome.GetError().c_str());
            return false;
        }
        if (!azslCompiler.ParseBindingdepPopulateBindingDependencies(jsonOutcome.GetValue(), bindingDependencies))
        {
            AZ_Error(builderName, false, "Failed to parse binding dependencies data");
            return false;
        }

        return true;
    }

    // Returns the content of the hlsl file for the given supervariant as produced by ShaderAsssetBuilder.
    // In addition to the content it also returns the full path of the hlsl file in @hlslSourcePath.
    static AZStd::string LoadHlslFileFromShaderAssetBuilder(
        const char* builderName,
        const RHI::ShaderPlatformInterface* shaderPlatformInterface, const AssetBuilderSDK::PlatformInfo& platformInfo,
        const AZStd::string& shaderSourceFileFullPath, const RPI::SupervariantIndex supervariantIndex, AZStd::string& hlslSourcePath)
    {
        auto hlslSourcePathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            shaderPlatformInterface->GetAPIUniqueIndex(), platformInfo.m_identifier, shaderSourceFileFullPath, supervariantIndex.GetIndex(),
            AZ::RPI::ShaderAssetSubId::GeneratedHlslSource);
        if (!hlslSourcePathOutcome.IsSuccess())
        {
            AZ_Error(builderName, false, "%s", hlslSourcePathOutcome.GetError().c_str());
            return "";
        }

        hlslSourcePath = hlslSourcePathOutcome.TakeValue();
        Outcome<AZStd::string, AZStd::string> hlslSourceOutcome = Utils::ReadFile(hlslSourcePath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        if (!hlslSourceOutcome.IsSuccess())
        {
            AZ_Error(
                builderName, false, "Failed to obtain shader source from %s. [%s]", hlslSourcePath.c_str(),
                hlslSourceOutcome.TakeError().c_str());
            return "";
        }
        return hlslSourceOutcome.TakeValue();
    }

    AZ::Outcome<VariantCompilationInputs, AZStd::string> AzslcBackend::LoadVariantCompilationInputs(
        const VariantCompilationInputsRequest& request)
    {
        const AZStd::string builderName(request.m_builderName);
        const AZStd::string shaderDescriptorPath(request.m_shaderDescriptorPath);
        const AZStd::string sourceFullPath(request.m_shaderSourceFullPath);
        const RPI::SupervariantIndex supervariantIndex(request.m_supervariantIndex);

        // Set the input file for eventual error messages; the compiler won't be called on it.
        const AzslCompiler azslc(sourceFullPath, AZStd::string(request.m_tempDirPath));

        VariantCompilationInputs inputs;
        inputs.m_shaderOptionGroupLayout = LoadShaderOptionsGroupLayoutFromShaderAssetBuilder(
            builderName.c_str(), request.m_shaderPlatformInterface, *request.m_platformInfo, azslc,
            shaderDescriptorPath, supervariantIndex, inputs.m_useSpecializationConstants);
        if (!inputs.m_shaderOptionGroupLayout)
        {
            return AZ::Failure(AZStd::string::format(
                "Failed to load the shader option group layout for %s", shaderDescriptorPath.c_str()));
        }

        if (!request.m_loadStageInputs)
        {
            return AZ::Success(AZStd::move(inputs));
        }

        AzslFunctions azslFunctions;
        LoadShaderFunctionsFromShaderAssetBuilder(
            builderName.c_str(), request.m_shaderPlatformInterface, *request.m_platformInfo, azslc,
            shaderDescriptorPath, supervariantIndex, azslFunctions);
        if (azslFunctions.empty())
        {
            return AZ::Failure(AZStd::string::format("Failed to load the shader functions for %s", shaderDescriptorPath.c_str()));
        }

        inputs.m_stageSourceCode = LoadHlslFileFromShaderAssetBuilder(
            builderName.c_str(), request.m_shaderPlatformInterface, *request.m_platformInfo,
            shaderDescriptorPath, supervariantIndex, inputs.m_stageSourcePath);
        if (inputs.m_stageSourceCode.empty() || inputs.m_stageSourcePath.empty())
        {
            return AZ::Failure(AZStd::string::format("Failed to load the generated HLSL for %s", shaderDescriptorPath.c_str()));
        }

        if (request.m_shaderPlatformInterface->VariantCompilationRequiresSrgLayoutData())
        {
            RPI::ShaderResourceGroupLayoutList srgLayoutList;
            RootConstantData rootConstantData;
            if (!LoadSrgLayoutListFromShaderAssetBuilder(
                    builderName.c_str(), request.m_shaderPlatformInterface, *request.m_platformInfo, azslc,
                    shaderDescriptorPath, supervariantIndex, srgLayoutList, rootConstantData))
            {
                return AZ::Failure(AZStd::string::format(
                    "Failed to load the ShaderResourceGroup layouts for %s", shaderDescriptorPath.c_str()));
            }

            BindingDependencies bindingDependencies;
            if (!LoadBindingDependenciesFromShaderAssetBuilder(
                    builderName.c_str(), request.m_shaderPlatformInterface, *request.m_platformInfo, azslc,
                    shaderDescriptorPath, supervariantIndex, bindingDependencies))
            {
                return AZ::Failure(AZStd::string::format(
                    "Failed to load the binding dependencies for %s", shaderDescriptorPath.c_str()));
            }

            inputs.m_pipelineLayoutDescriptor = ShaderBuilderUtility::BuildPipelineLayoutDescriptorForApi(
                builderName.c_str(),
                srgLayoutList,
                *request.m_entryPoints,
                *request.m_buildArguments,
                rootConstantData,
                request.m_shaderPlatformInterface,
                bindingDependencies);
            if (!inputs.m_pipelineLayoutDescriptor)
            {
                return AZ::Failure(AZStd::string::format(
                    "Failed to build pipeline layout descriptor for api=[%s]",
                    request.m_shaderPlatformInterface->GetAPIName().GetCStr()));
            }
        }

        return AZ::Success(AZStd::move(inputs));
    }

    AZ::Outcome<StageResult, AZStd::string> AzslcBackend::CompileStage(const StageInput& input)
    {
        StageResult result;
        AZStd::string sourcePath(input.m_sourcePath);
        const AZStd::string entryPointName(input.m_entryPointName);
        const AZStd::string tempDirPath(input.m_tempDirPath);

        if (!input.m_variantOptionAssignments.empty())
        {
            // The variant's option values lower as #define macros prepended onto the generated
            // HLSL (the AZSLC-emitted option getters read them), materialized under a
            // stable-id-qualified name so variants never collide inside one job's temp folder.
            AZStd::string variantSourceContent;
            for (const ShaderOptionAssignment& assignment : input.m_variantOptionAssignments)
            {
                variantSourceContent += AZStd::string::format(
                    "#define %s_OPTION_DEF %s\n", assignment.m_option.GetCStr(), assignment.m_value.GetCStr());
            }
            variantSourceContent += input.m_sourceCode;

            const AZStd::string variantSourceName = AZStd::string::format(
                "%.*s_%s_%u.hlsl", AZ_STRING_ARG(input.m_stemNamePrefix),
                input.m_shaderPlatformInterface->GetAPIName().GetCStr(), input.m_variantStableId);
            AZStd::string variantSourcePath;
            AZ::StringFunc::Path::Join(tempDirPath.c_str(), variantSourceName.c_str(), variantSourcePath, true, true);

            auto writeOutcome = Utils::WriteFile(variantSourceContent, variantSourcePath);
            if (!writeOutcome.IsSuccess())
            {
                return AZ::Failure(AZStd::string::format("Failed to create file %s", variantSourcePath.c_str()));
            }
            sourcePath = variantSourcePath;
        }

        const bool compiled = input.m_shaderPlatformInterface->CompilePlatformInternal(
            *input.m_platformInfo,
            sourcePath,
            entryPointName,
            input.m_stage,
            tempDirPath,
            result.m_descriptor,
            *input.m_buildArguments,
            input.m_useSpecializationConstants);
        if (!compiled)
        {
            return AZ::Failure(AZStd::string::format("Could not compile the shader function %s", entryPointName.c_str()));
        }
        return AZ::Success(AZStd::move(result));
    }
} // namespace AZ::ShaderBuilder
