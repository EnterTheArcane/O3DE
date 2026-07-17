/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "ShaderReflectionData.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <Atom/RHI.Reflect/ConstantsLayout.h>
#include <Atom/RHI.Reflect/PipelineLayoutDescriptor.h>

#include <Atom/RHI.Edit/ShaderPlatformInterface.h>
#include <Atom/RHI.Edit/Utils.h>

#include "ShaderBuilderUtility.h"

namespace AZ::ShaderBuilder
{
    void ShaderReflectionData::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<ShaderFunctionAttributeReflection>()
                ->Version(1)
                ->Field("name", &ShaderFunctionAttributeReflection::m_name)
                ->Field("arguments", &ShaderFunctionAttributeReflection::m_arguments)
                ;

            serializeContext->Class<ShaderFunctionReflection>()
                ->Version(1)
                ->Field("name", &ShaderFunctionReflection::m_name)
                ->Field("attributes", &ShaderFunctionReflection::m_attributes)
                ;

            serializeContext->Class<ShaderStageInputReflection>()
                ->Version(1)
                ->Field("name", &ShaderStageInputReflection::m_name)
                ->Field("semanticText", &ShaderStageInputReflection::m_semanticText)
                ->Field("semanticIndex", &ShaderStageInputReflection::m_semanticIndex)
                ->Field("componentCount", &ShaderStageInputReflection::m_componentCount)
                ;

            serializeContext->Class<ShaderStageOutputReflection>()
                ->Version(1)
                ->Field("semanticText", &ShaderStageOutputReflection::m_semanticText)
                ->Field("componentCount", &ShaderStageOutputReflection::m_componentCount)
                ;

            serializeContext->Class<ResourceBindingReflection>()
                ->Version(1)
                ->Field("name", &ResourceBindingReflection::m_name)
                ->Field("registerId", &ResourceBindingReflection::m_registerId)
                ->Field("registerSpace", &ResourceBindingReflection::m_registerSpace)
                ->Field("dependentFunctions", &ResourceBindingReflection::m_dependentFunctions)
                ;

            serializeContext->Class<ShaderResourceGroupBindingReflection>()
                ->Version(1)
                ->Field("constantDataBinding", &ShaderResourceGroupBindingReflection::m_constantDataBinding)
                ->Field("resourceBindings", &ShaderResourceGroupBindingReflection::m_resourceBindings)
                ;

            serializeContext->Class<ShaderResourceGroupReflection>()
                ->Version(1)
                ->Field("name", &ShaderResourceGroupReflection::m_name)
                ->Field("uniqueId", &ShaderResourceGroupReflection::m_uniqueId)
                ->Field("bindingSlot", &ShaderResourceGroupReflection::m_bindingSlot)
                ->Field("shaderVariantKeyFallbackName", &ShaderResourceGroupReflection::m_shaderVariantKeyFallbackName)
                ->Field("shaderVariantKeyFallbackSize", &ShaderResourceGroupReflection::m_shaderVariantKeyFallbackSize)
                ->Field("staticSamplers", &ShaderResourceGroupReflection::m_staticSamplers)
                ->Field("samplers", &ShaderResourceGroupReflection::m_samplers)
                ->Field("images", &ShaderResourceGroupReflection::m_images)
                ->Field("imageUnboundedArrays", &ShaderResourceGroupReflection::m_imageUnboundedArrays)
                ->Field("buffers", &ShaderResourceGroupReflection::m_buffers)
                ->Field("bufferUnboundedArrays", &ShaderResourceGroupReflection::m_bufferUnboundedArrays)
                ->Field("constants", &ShaderResourceGroupReflection::m_constants)
                ;

            serializeContext->Class<RootConstantsReflection::Constant>()
                ->Version(1)
                ->Field("name", &RootConstantsReflection::Constant::m_name)
                ->Field("byteOffset", &RootConstantsReflection::Constant::m_byteOffset)
                ->Field("byteSize", &RootConstantsReflection::Constant::m_byteSize)
                ;

            serializeContext->Class<RootConstantsReflection>()
                ->Version(1)
                ->Field("bufferName", &RootConstantsReflection::m_bufferName)
                ->Field("sizeInBytes", &RootConstantsReflection::m_sizeInBytes)
                ->Field("registerId", &RootConstantsReflection::m_registerId)
                ->Field("registerSpace", &RootConstantsReflection::m_registerSpace)
                ->Field("constants", &RootConstantsReflection::m_constants)
                ;

            serializeContext->Class<ShaderReflectionData>()
                ->Version(1)
                ->Field("schemaVersion", &ShaderReflectionData::m_schemaVersion)
                ->Field("shaderResourceGroups", &ShaderReflectionData::m_shaderResourceGroups)
                ->Field("shaderResourceGroupBindings", &ShaderReflectionData::m_shaderResourceGroupBindings)
                ->Field("shaderOptions", &ShaderReflectionData::m_shaderOptions)
                ->Field("usesSpecializationConstants", &ShaderReflectionData::m_usesSpecializationConstants)
                ->Field("rootConstants", &ShaderReflectionData::m_rootConstants)
                ->Field("functions", &ShaderReflectionData::m_functions)
                ->Field("vertexEntryInputs", &ShaderReflectionData::m_vertexEntryInputs)
                ->Field("fragmentEntryOutputs", &ShaderReflectionData::m_fragmentEntryOutputs)
                ;
        }
    }

    AZ::Outcome<RPI::ShaderResourceGroupLayoutList, AZStd::string> BuildShaderResourceGroupLayouts(const ShaderReflectionData& reflectionData)
    {
        RPI::ShaderResourceGroupLayoutList srgLayoutList;
        for (const ShaderResourceGroupReflection& srgReflection : reflectionData.m_shaderResourceGroups)
        {
            RHI::Ptr<RHI::ShaderResourceGroupLayout> srgLayout = RHI::ShaderResourceGroupLayout::Create();
            srgLayout->SetName(srgReflection.m_name);
            srgLayout->SetUniqueId(srgReflection.m_uniqueId);
            srgLayout->SetBindingSlot(srgReflection.m_bindingSlot);

            for (const RHI::ShaderInputStaticSamplerDescriptor& staticSampler : srgReflection.m_staticSamplers)
            {
                srgLayout->AddStaticSampler(staticSampler);
            }
            for (const RHI::ShaderInputSamplerDescriptor& sampler : srgReflection.m_samplers)
            {
                srgLayout->AddShaderInput(sampler);
            }
            for (const RHI::ShaderInputImageDescriptor& image : srgReflection.m_images)
            {
                srgLayout->AddShaderInput(image);
            }
            for (const RHI::ShaderInputImageUnboundedArrayDescriptor& imageUnboundedArray : srgReflection.m_imageUnboundedArrays)
            {
                srgLayout->AddShaderInput(imageUnboundedArray);
            }
            for (const RHI::ShaderInputBufferDescriptor& buffer : srgReflection.m_buffers)
            {
                srgLayout->AddShaderInput(buffer);
            }
            for (const RHI::ShaderInputBufferUnboundedArrayDescriptor& bufferUnboundedArray : srgReflection.m_bufferUnboundedArrays)
            {
                srgLayout->AddShaderInput(bufferUnboundedArray);
            }
            for (const RHI::ShaderInputConstantDescriptor& constant : srgReflection.m_constants)
            {
                srgLayout->AddShaderInput(constant);
            }

            if (srgReflection.m_shaderVariantKeyFallbackSize > 0)
            {
                srgLayout->SetShaderVariantKeyFallback(srgReflection.m_shaderVariantKeyFallbackName, srgReflection.m_shaderVariantKeyFallbackSize);
            }

            srgLayoutList.push_back(srgLayout);
        }

        return AZ::Success(AZStd::move(srgLayoutList));
    }

    RPI::Ptr<RPI::ShaderOptionGroupLayout> BuildShaderOptionGroupLayout(const ShaderReflectionData& reflectionData)
    {
        RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout = RPI::ShaderOptionGroupLayout::Create();
        for (const RPI::ShaderOptionDescriptor& shaderOption : reflectionData.m_shaderOptions)
        {
            if (!shaderOptionGroupLayout->AddShaderOption(shaderOption))
            {
                // AddShaderOption reports the error details
                return nullptr;
            }
        }
        shaderOptionGroupLayout->Finalize();
        return shaderOptionGroupLayout;
    }

    RHI::ShaderStageAttributeArguments BuildShaderStageAttributeArguments(AZStd::span<const ShaderFunctionAttributeArgument> attributeArguments)
    {
        RHI::ShaderStageAttributeArguments arguments;
        for (const ShaderFunctionAttributeArgument& argument : attributeArguments)
        {
            if (const bool* boolValue = AZStd::get_if<bool>(&argument))
            {
                arguments.push_back(AZStd::any{*boolValue});
            }
            else if (const int32_t* intValue = AZStd::get_if<int32_t>(&argument))
            {
                arguments.push_back(AZStd::any{*intValue});
            }
            else if (const double* doubleValue = AZStd::get_if<double>(&argument))
            {
                arguments.push_back(AZStd::any{*doubleValue});
            }
            else if (const AZStd::string* stringValue = AZStd::get_if<AZStd::string>(&argument))
            {
                arguments.push_back(AZStd::any{*stringValue});
            }
            else
            {
                arguments.push_back(AZStd::any{});
            }
        }
        return arguments;
    }

    RHI::Ptr<RHI::PipelineLayoutDescriptor> BuildPipelineLayoutDescriptor(
        AZStd::string_view builderName,
        const ShaderReflectionData& reflectionData,
        const RPI::ShaderResourceGroupLayoutList& srgLayoutList,
        const MapOfStringToStageType& shaderEntryPoints,
        const RHI::ShaderBuildArguments& shaderBuildArguments,
        RHI::ShaderPlatformInterface* shaderPlatformInterface)
    {
        const AZStd::string builderNameString(builderName);

        // Translates from a list of function names that use a resource to a shader stage mask.
        auto getRHIShaderStageMask = [&shaderEntryPoints](const AZStd::vector<AZStd::string>& dependentFunctions)
        {
            RHI::ShaderStageMask mask = RHI::ShaderStageMask::None;
            for (const AZStd::string& functionName : dependentFunctions)
            {
                const auto findId = shaderEntryPoints.find(functionName);
                if (findId != shaderEntryPoints.end())
                {
                    const RHI::ShaderHardwareStage hardwareStage = ShaderBuilderUtility::ToAssetBuilderShaderType(findId->second);
                    mask |= static_cast<RHI::ShaderStageMask>(AZ_BIT(static_cast<uint32_t>(RHI::ToRHIShaderStage(hardwareStage))));
                }
            }
            return mask;
        };

        // Build general PipelineLayoutDescriptor data that is provided for all platforms
        RHI::Ptr<RHI::PipelineLayoutDescriptor> pipelineLayoutDescriptor = shaderPlatformInterface->CreatePipelineLayoutDescriptor();
        RHI::ShaderPlatformInterface::ShaderResourceGroupInfoList srgInfos;
        for (const RHI::Ptr<RHI::ShaderResourceGroupLayout>& srgLayout : srgLayoutList)
        {
            const AZStd::string srgName(srgLayout->GetName().GetStringView());
            const auto findBinding = reflectionData.m_shaderResourceGroupBindings.find(srgName);
            if (findBinding == reflectionData.m_shaderResourceGroupBindings.end())
            {
                AZ_Error(builderNameString.c_str(), false, "SRG %s not found in the dependency dataset", srgName.c_str());
                return nullptr;
            }
            const ShaderResourceGroupBindingReflection& srgBindingReflection = findBinding->second;

            RHI::ShaderResourceGroupBindingInfo srgBindingInfo;
            // Calculate the binding of the constant data. All constant data share the same binding info.
            srgBindingInfo.m_constantDataBindingInfo = {
                getRHIShaderStageMask(srgBindingReflection.m_constantDataBinding.m_dependentFunctions),
                srgBindingReflection.m_constantDataBinding.m_registerId,
                srgBindingReflection.m_constantDataBinding.m_registerSpace
            };
            // Calculate the binding info for each resource of the Shader Resource Group.
            for (const ResourceBindingReflection& resourceBinding : srgBindingReflection.m_resourceBindings)
            {
                srgBindingInfo.m_resourcesRegisterMap.insert(
                    {resourceBinding.m_name,
                    RHI::ResourceBindingInfo(
                        getRHIShaderStageMask(resourceBinding.m_dependentFunctions),
                        resourceBinding.m_registerId,
                        resourceBinding.m_registerSpace)});
            }

            pipelineLayoutDescriptor->AddShaderResourceGroupLayoutInfo(*srgLayout.get(), srgBindingInfo);
            srgInfos.push_back(RHI::ShaderPlatformInterface::ShaderResourceGroupInfo{srgLayout.get(), srgBindingInfo});
        }

        RHI::Ptr<RHI::ConstantsLayout> rootConstantsLayout = RHI::ConstantsLayout::Create();
        for (const RootConstantsReflection::Constant& constant : reflectionData.m_rootConstants.m_constants)
        {
            const RHI::ShaderInputConstantDescriptor rootConstantDescriptor(
                constant.m_name,
                constant.m_byteOffset,
                constant.m_byteSize,
                reflectionData.m_rootConstants.m_registerId,
                reflectionData.m_rootConstants.m_registerSpace);
            rootConstantsLayout->AddShaderInput(rootConstantDescriptor);
        }

        if (!rootConstantsLayout->Finalize())
        {
            AZ_Error(builderNameString.c_str(), false, "Failed to finalize root constants layout");
            return nullptr;
        }

        pipelineLayoutDescriptor->SetRootConstantsLayout(*rootConstantsLayout);

        RHI::ShaderPlatformInterface::RootConstantsInfo rootConstantInfo;
        rootConstantInfo.m_spaceId = reflectionData.m_rootConstants.m_registerSpace;
        rootConstantInfo.m_registerId = reflectionData.m_rootConstants.m_registerId;
        rootConstantInfo.m_totalSizeInBytes = rootConstantsLayout->GetDataSize();

        // Build platform-specific PipelineLayoutDescriptor data, and finalize
        if (!shaderPlatformInterface->BuildPipelineLayoutDescriptor(pipelineLayoutDescriptor, srgInfos, rootConstantInfo, shaderBuildArguments))
        {
            AZ_Error(builderNameString.c_str(), false, "Failed to build pipeline layout descriptor");
            return nullptr;
        }

        return pipelineLayoutDescriptor;
    }

    static bool IsSystemValueSemantic(AZStd::string_view semantic)
    {
        // https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics#system-value-semantics
        return AzFramework::StringFunc::StartsWith(semantic, "sv_", false);
    }

    static bool BuildShaderInputContract(
        const AZStd::string& builderNameString,
        const ShaderReflectionData& reflectionData,
        const AZStd::string& vertexShaderName,
        const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout,
        RPI::ShaderInputContract& contract)
    {
        const auto findInputs = reflectionData.m_vertexEntryInputs.find(vertexShaderName);
        if (findInputs == reflectionData.m_vertexEntryInputs.end())
        {
            AZ_Error(builderNameString.c_str(), false, "Failed to find the input struct for vertex shader %s", vertexShaderName.c_str());
            return false;
        }

        for (const ShaderStageInputReflection& member : findInputs->second)
        {
            RHI::ShaderSemantic streamChannelSemantic{Name{member.m_semanticText}, aznumeric_cast<uint32_t>(member.m_semanticIndex)};

            // Semantics that represent a system-generated value do not map to an input stream
            if (IsSystemValueSemantic(streamChannelSemantic.m_name.GetStringView()))
            {
                continue;
            }

            contract.m_streamChannels.emplace_back().m_semantic = streamChannelSemantic;
            contract.m_streamChannels.back().m_componentCount = member.m_componentCount;

            // [GFX_TODO][ATOM-14475]: Come up with a more elegant way to mark optional channels and their corresponding shader option
            static constexpr char OptionalInputStreamPrefix[] = "m_optional_";
            if (AzFramework::StringFunc::StartsWith(member.m_name, OptionalInputStreamPrefix, true))
            {
                const AZStd::string expectedOptionName = AZStd::string::format(
                    "o_%s_isBound", member.m_name.substr(strlen(OptionalInputStreamPrefix)).c_str());

                RPI::ShaderOptionIndex shaderOptionIndex = shaderOptionGroupLayout.FindShaderOptionIndex(Name{expectedOptionName});
                if (!shaderOptionIndex.IsValid())
                {
                    AZ_Error(
                        builderNameString.c_str(), false, "Shader option '%s' not found for optional input stream '%s'",
                        expectedOptionName.c_str(), member.m_name.c_str());
                    return false;
                }

                const RPI::ShaderOptionDescriptor& option = shaderOptionGroupLayout.GetShaderOption(shaderOptionIndex);
                if (option.GetType() != RPI::ShaderOptionType::Boolean)
                {
                    AZ_Error(builderNameString.c_str(), false, "Shader option '%s' must be a bool", expectedOptionName.c_str());
                    return false;
                }

                if (option.GetDefaultValue().GetStringView() != "false")
                {
                    AZ_Error(builderNameString.c_str(), false, "Shader option '%s' must default to false", expectedOptionName.c_str());
                    return false;
                }

                contract.m_streamChannels.back().m_isOptional = true;
                contract.m_streamChannels.back().m_streamBoundIndicatorIndex = shaderOptionIndex;
            }
        }

        return true;
    }

    static bool BuildShaderOutputContract(
        const AZStd::string& builderNameString,
        const ShaderReflectionData& reflectionData,
        const AZStd::string& fragmentShaderName,
        RPI::ShaderOutputContract& contract)
    {
        const auto findOutputs = reflectionData.m_fragmentEntryOutputs.find(fragmentShaderName);
        if (findOutputs == reflectionData.m_fragmentEntryOutputs.end())
        {
            AZ_Error(builderNameString.c_str(), false, "Failed to find the output struct for fragment shader %s", fragmentShaderName.c_str());
            return false;
        }

        for (const ShaderStageOutputReflection& member : findOutputs->second)
        {
            const RHI::ShaderSemantic semantic = RHI::ShaderSemantic::Parse(member.m_semanticText);

            bool depthFound = false;

            if (semantic.m_name.GetStringView() == "SV_Target")
            {
                // Render targets only support 1-D vector types
                contract.m_requiredColorAttachments.emplace_back().m_componentCount = member.m_componentCount;
            }
            else if (
                semantic.m_name.GetStringView() == "SV_Depth" || semantic.m_name.GetStringView() == "SV_DepthGreaterEqual" ||
                semantic.m_name.GetStringView() == "SV_DepthLessEqual")
            {
                if (depthFound)
                {
                    AZ_Error(builderNameString.c_str(), false, "SV_Depth specified more than once in the fragment shader output structure");
                    return false;
                }
                depthFound = true;
            }
            else
            {
                AZ_Error(builderNameString.c_str(), false, "Unsupported shader output semantic '%s'", semantic.m_name.GetCStr());
                return false;
            }
        }

        return true;
    }

    bool BuildShaderInputAndOutputContracts(
        AZStd::string_view builderName,
        const ShaderReflectionData& reflectionData,
        const MapOfStringToStageType& shaderEntryPoints,
        const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout,
        RPI::ShaderInputContract& shaderInputContract,
        RPI::ShaderOutputContract& shaderOutputContract,
        size_t& colorAttachmentCount)
    {
        const AZStd::string builderNameString(builderName);
        bool success = true;
        for (const auto& [shaderEntryName, shaderStageType] : shaderEntryPoints)
        {
            if (shaderStageType == RPI::ShaderStageType::Vertex)
            {
                if (!BuildShaderInputContract(builderNameString, reflectionData, shaderEntryName, shaderOptionGroupLayout, shaderInputContract))
                {
                    success = false;
                    AZ_Error(
                        builderNameString.c_str(), false, "Could not create the input contract for the vertex function %s",
                        shaderEntryName.c_str());
                    continue; // Using continue to report all the errors found
                }
            }

            if (shaderStageType == RPI::ShaderStageType::Fragment)
            {
                if (!BuildShaderOutputContract(builderNameString, reflectionData, shaderEntryName, shaderOutputContract))
                {
                    success = false;
                    AZ_Error(
                        builderNameString.c_str(), false, "Could not create the output contract for the fragment function %s",
                        shaderEntryName.c_str());
                    continue; // Using continue to report all the errors found
                }

                colorAttachmentCount = shaderOutputContract.m_requiredColorAttachments.size();
            }
        }
        return success;
    }

    AZStd::string DumpReflectionData(
        AZStd::string_view builderName,
        const ShaderReflectionData& reflectionData,
        AZStd::string_view tempDirPath,
        AZStd::string_view stemName,
        AZStd::string_view apiName)
    {
        const AZStd::string fileName = AZStd::string::format(
            "%.*s.%.*s.reflection.xml",
            AZ_STRING_ARG(stemName),
            AZ_STRING_ARG(apiName));
        AZStd::string fullPath;
        AzFramework::StringFunc::Path::Join(AZStd::string(tempDirPath).c_str(), fileName.c_str(), fullPath);

        const AZStd::string builderNameString(builderName);
        if (!AZ::Utils::SaveObjectToFile(fullPath, AZ::DataStream::ST_XML, &reflectionData))
        {
            AZ_Warning(builderNameString.c_str(), false, "Failed to write reflection data dump to %s", fullPath.c_str());
            return "";
        }
        return fullPath;
    }
} // namespace AZ::ShaderBuilder
