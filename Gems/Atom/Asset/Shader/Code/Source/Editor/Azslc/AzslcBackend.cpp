/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AzslcBackend.h"

#include <AzCore/Utils/Utils.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <Atom/RHI.Edit/Utils.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <AzslCompiler.h>
#include <CommonFiles/Preprocessor.h>
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

        FrontendResult result;
        result.m_subProductPaths = emitFullOutcome.TakeValue();
        result.m_azslData.m_preprocessedFullPath = azslinFullPath;

        const AssetBuilderSDK::ProcessJobResultCode azslJsonReadResult = ShaderBuilderUtility::PopulateAzslDataFromJsonFiles(
            builderName.c_str(),
            result.m_subProductPaths,
            result.m_azslData,
            result.m_srgLayoutList,
            result.m_shaderOptionGroupLayout,
            result.m_bindingDependencies,
            result.m_rootConstantData,
            tempDirPath,
            result.m_usesSpecializationConstants);
        if (azslJsonReadResult != AssetBuilderSDK::ProcessJobResult_Success)
        {
            return AZ::Failure(AZStd::string::format("Failed to load the AZSLC reflection data for %s", azslinFullPath.c_str()));
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
