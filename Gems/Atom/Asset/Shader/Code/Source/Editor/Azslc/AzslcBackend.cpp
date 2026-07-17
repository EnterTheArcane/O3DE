/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AzslcBackend.h"

#include <AzCore/IO/SystemFile.h>
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

    AZ::Outcome<StageResult, AZStd::string> AzslcBackend::CompileStage(const StageInput& input)
    {
        StageResult result;
        const AZStd::string sourcePath(input.m_sourcePath);
        const AZStd::string entryPointName(input.m_entryPointName);
        const AZStd::string tempDirPath(input.m_tempDirPath);

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
