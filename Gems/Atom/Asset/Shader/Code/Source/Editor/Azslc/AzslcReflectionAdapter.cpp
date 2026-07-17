/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AzslcReflectionAdapter.h"

#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/optional.h>

#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <AzslCompiler.h>

namespace AZ::ShaderBuilder::AzslcReflectionAdapter
{
    static RHI::ShaderInputImageType ToShaderInputImageType(TextureType textureType)
    {
        switch (textureType)
        {
        case TextureType::Texture1D:
            return RHI::ShaderInputImageType::Image1D;
        case TextureType::Texture1DArray:
            return RHI::ShaderInputImageType::Image1DArray;
        case TextureType::Texture2D:
            return RHI::ShaderInputImageType::Image2D;
        case TextureType::Texture2DArray:
            return RHI::ShaderInputImageType::Image2DArray;
        case TextureType::Texture2DMS:
            return RHI::ShaderInputImageType::Image2DMultisample;
        case TextureType::Texture2DMSArray:
            return RHI::ShaderInputImageType::Image2DMultisampleArray;
        case TextureType::Texture3D:
            return RHI::ShaderInputImageType::Image3D;
        case TextureType::TextureCube:
            return RHI::ShaderInputImageType::ImageCube;
        case TextureType::RwTexture1D:
            return RHI::ShaderInputImageType::Image1D;
        case TextureType::RwTexture1DArray:
            return RHI::ShaderInputImageType::Image1DArray;
        case TextureType::RwTexture2D:
            return RHI::ShaderInputImageType::Image2D;
        case TextureType::RwTexture2DArray:
            return RHI::ShaderInputImageType::Image2DArray;
        case TextureType::RwTexture3D:
            return RHI::ShaderInputImageType::Image3D;
        case TextureType::RasterizerOrderedTexture1D:
            return RHI::ShaderInputImageType::Image1D;
        case TextureType::RasterizerOrderedTexture1DArray:
            return RHI::ShaderInputImageType::Image1DArray;
        case TextureType::RasterizerOrderedTexture2D:
            return RHI::ShaderInputImageType::Image2D;
        case TextureType::RasterizerOrderedTexture2DArray:
            return RHI::ShaderInputImageType::Image2DArray;
        case TextureType::RasterizerOrderedTexture3D:
            return RHI::ShaderInputImageType::Image3D;
        case TextureType::SubpassInput:
            return RHI::ShaderInputImageType::SubpassInput;
        default:
            AZ_Assert(false, "Unhandled TextureType");
            return RHI::ShaderInputImageType::Unknown;
        }
    }

    static RHI::ShaderInputBufferType ToShaderInputBufferType(BufferType bufferType)
    {
        switch (bufferType)
        {
        case BufferType::Buffer:
        case BufferType::RwBuffer:
        case BufferType::RasterizerOrderedBuffer:
            return RHI::ShaderInputBufferType::Typed;
        case BufferType::AppendStructuredBuffer:
        case BufferType::ConsumeStructuredBuffer:
        case BufferType::RasterizerOrderedStructuredBuffer:
        case BufferType::RwStructuredBuffer:
        case BufferType::StructuredBuffer:
            return RHI::ShaderInputBufferType::Structured;
        case BufferType::RasterizerOrderedByteAddressBuffer:
        case BufferType::ByteAddressBuffer:
        case BufferType::RwByteAddressBuffer:
            return RHI::ShaderInputBufferType::Raw;
        case BufferType::RaytracingAccelerationStructure:
            return RHI::ShaderInputBufferType::AccelerationStructure;
        default:
            AZ_Assert(false, "Unhandled BufferType");
            return RHI::ShaderInputBufferType::Unknown;
        }
    }

    bool ConvertSrgDataToReflection(
        AZStd::string_view builderName,
        const SrgDataContainer& srgDataContainer,
        AZStd::vector<ShaderResourceGroupReflection>& srgReflections)
    {
        const AZStd::string builderNameString(builderName);
        for (const SrgData& srgData : srgDataContainer)
        {
            ShaderResourceGroupReflection srgReflection;
            srgReflection.m_name = Name{srgData.m_name.c_str()};
            srgReflection.m_uniqueId = srgData.m_containingFileName;
            srgReflection.m_bindingSlot = srgData.m_bindingSlot.m_index;

            // Samplers
            for (const SamplerSrgData& samplerData : srgData.m_samplers)
            {
                if (samplerData.m_isDynamic)
                {
                    srgReflection.m_samplers.push_back({
                        samplerData.m_nameId,
                        samplerData.m_count,
                        samplerData.m_registerId,
                        samplerData.m_spaceId});
                }
                else
                {
                    srgReflection.m_staticSamplers.push_back({
                        samplerData.m_nameId,
                        samplerData.m_descriptor,
                        samplerData.m_registerId,
                        samplerData.m_spaceId});
                }
            }

            // Images
            for (const TextureSrgData& textureData : srgData.m_textures)
            {
                const RHI::ShaderInputImageAccess imageAccess =
                    textureData.m_isReadOnlyType ? RHI::ShaderInputImageAccess::Read : RHI::ShaderInputImageAccess::ReadWrite;

                const RHI::ShaderInputImageType imageType = ToShaderInputImageType(textureData.m_type);
                if (imageType == RHI::ShaderInputImageType::Unknown)
                {
                    AZ_Error(
                        builderNameString.c_str(), false, "Failed to build Shader Resource Group reflection: Image %s has an unknown type",
                        textureData.m_nameId.GetCStr());
                    return false;
                }

                if (textureData.m_count != aznumeric_cast<uint32_t>(-1))
                {
                    srgReflection.m_images.push_back(RHI::ShaderInputImageDescriptor{
                        textureData.m_nameId, imageAccess, imageType, textureData.m_count,
                        textureData.m_registerId, textureData.m_spaceId});
                }
                else
                {
                    // unbounded array
                    srgReflection.m_imageUnboundedArrays.push_back(RHI::ShaderInputImageUnboundedArrayDescriptor{
                        textureData.m_nameId, imageAccess, imageType,
                        textureData.m_registerId, textureData.m_spaceId});
                }
            }

            // Constant buffers and buffers share the buffer input category; keeping the constant
            // buffers first preserves the legacy layout-construction order.
            for (const ConstantBufferData& constantBufferData : srgData.m_constantBuffers)
            {
                srgReflection.m_buffers.push_back(RHI::ShaderInputBufferDescriptor{
                    constantBufferData.m_nameId, RHI::ShaderInputBufferAccess::Constant, RHI::ShaderInputBufferType::Constant,
                    constantBufferData.m_count, constantBufferData.m_strideSize, constantBufferData.m_registerId, constantBufferData.m_spaceId});
            }

            for (const BufferSrgData& bufferData : srgData.m_buffers)
            {
                const RHI::ShaderInputBufferAccess bufferAccess =
                    bufferData.m_isReadOnlyType ? RHI::ShaderInputBufferAccess::Read : RHI::ShaderInputBufferAccess::ReadWrite;

                const RHI::ShaderInputBufferType bufferType = ToShaderInputBufferType(bufferData.m_type);
                if (bufferType == RHI::ShaderInputBufferType::Unknown)
                {
                    AZ_Error(
                        builderNameString.c_str(), false, "Failed to build Shader Resource Group reflection: Buffer %s has an unknown type",
                        bufferData.m_nameId.GetCStr());
                    return false;
                }

                if (bufferData.m_count != aznumeric_cast<uint32_t>(-1))
                {
                    srgReflection.m_buffers.push_back(RHI::ShaderInputBufferDescriptor{
                        bufferData.m_nameId, bufferAccess, bufferType, bufferData.m_count, bufferData.m_strideSize,
                        bufferData.m_registerId, bufferData.m_spaceId});
                }
                else
                {
                    // unbounded array
                    srgReflection.m_bufferUnboundedArrays.push_back(RHI::ShaderInputBufferUnboundedArrayDescriptor{
                        bufferData.m_nameId, bufferAccess, bufferType, bufferData.m_strideSize,
                        bufferData.m_registerId, bufferData.m_spaceId});
                }
            }

            // SRG Constants
            for (const SrgConstantData& srgConstants : srgData.m_srgConstantData)
            {
                srgReflection.m_constants.push_back({
                    srgConstants.m_nameId,
                    srgConstants.m_constantByteOffset,
                    srgConstants.m_constantByteSize,
                    srgData.m_srgConstantDataRegisterId,
                    srgData.m_srgConstantDataSpaceId});
            }

            // Shader Variant Key fallback
            srgReflection.m_shaderVariantKeyFallbackName = srgData.m_fallbackName;
            srgReflection.m_shaderVariantKeyFallbackSize = srgData.m_fallbackSize;

            srgReflections.push_back(AZStd::move(srgReflection));
        }

        return true;
    }

    static ResourceBindingReflection ConvertResourceBinding(const BindingDependencies::Resource& resource)
    {
        ResourceBindingReflection bindingReflection;
        bindingReflection.m_name = Name{resource.m_selfName.c_str()};
        bindingReflection.m_registerId = resource.m_registerId;
        bindingReflection.m_registerSpace = resource.m_registerSpace;
        bindingReflection.m_dependentFunctions = resource.m_dependentFunctions;
        return bindingReflection;
    }

    void ConvertBindingDependenciesToReflection(
        const BindingDependencies& bindingDependencies,
        AZStd::unordered_map<AZStd::string, ShaderResourceGroupBindingReflection>& srgBindings)
    {
        for (const auto& [srgName, srgIndex] : bindingDependencies.m_srgNameToVectorIndex)
        {
            const BindingDependencies::SrgResources& srgResources = bindingDependencies.m_orderedSrgs[srgIndex];

            ShaderResourceGroupBindingReflection srgBindingReflection;
            srgBindingReflection.m_constantDataBinding = ConvertResourceBinding(srgResources.m_srgConstantsDependencies.m_binding);
            for (const auto& [resourceName, resource] : srgResources.m_resources)
            {
                srgBindingReflection.m_resourceBindings.push_back(ConvertResourceBinding(resource));
            }

            srgBindings.emplace(srgName, AZStd::move(srgBindingReflection));
        }
    }

    RootConstantsReflection ConvertRootConstantDataToReflection(const RootConstantData& rootConstantData)
    {
        RootConstantsReflection rootConstantsReflection;
        rootConstantsReflection.m_bufferName = rootConstantData.m_bindingInfo.m_nameId;
        rootConstantsReflection.m_sizeInBytes = rootConstantData.m_bindingInfo.m_sizeInBytes;
        rootConstantsReflection.m_registerId = rootConstantData.m_bindingInfo.m_registerId;
        rootConstantsReflection.m_registerSpace = rootConstantData.m_bindingInfo.m_space;
        for (const SrgConstantData& constant : rootConstantData.m_constants)
        {
            rootConstantsReflection.m_constants.push_back({constant.m_nameId, constant.m_constantByteOffset, constant.m_constantByteSize});
        }
        return rootConstantsReflection;
    }

    static ShaderFunctionAttributeArgument ConvertAttributeArgument(const AZStd::any& argument)
    {
        if (const bool* boolValue = AZStd::any_cast<bool>(&argument))
        {
            return *boolValue;
        }
        if (const int32_t* intValue = AZStd::any_cast<int32_t>(&argument))
        {
            return *intValue;
        }
        if (const double* doubleValue = AZStd::any_cast<double>(&argument))
        {
            return *doubleValue;
        }
        // AZSLC captures string arguments as raw character pointers into the parsed JSON; they are
        // stored by value here so the reflection outlives the parse
        if (const char* const* stringValue = AZStd::any_cast<const char*>(&argument))
        {
            return AZStd::string(*stringValue);
        }
        if (const AZStd::string* azStringValue = AZStd::any_cast<AZStd::string>(&argument))
        {
            return *azStringValue;
        }
        return AZStd::monostate{};
    }

    AZStd::vector<ShaderFunctionReflection> ConvertFunctionsToReflection(const AzslFunctions& functions)
    {
        AZStd::vector<ShaderFunctionReflection> functionReflections;
        for (const FunctionData& function : functions)
        {
            ShaderFunctionReflection functionReflection;
            functionReflection.m_name = function.m_name;
            for (const auto& [attributeName, attributeArguments] : function.attributesList)
            {
                ShaderFunctionAttributeReflection attributeReflection;
                attributeReflection.m_name = attributeName;
                for (const AZStd::any& argument : attributeArguments)
                {
                    attributeReflection.m_arguments.push_back(ConvertAttributeArgument(argument));
                }
                functionReflection.m_attributes.push_back(AZStd::move(attributeReflection));
            }
            functionReflections.push_back(AZStd::move(functionReflection));
        }
        return functionReflections;
    }

    AZStd::vector<RPI::ShaderOptionDescriptor> ConvertShaderOptionsToReflection(const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout)
    {
        const auto& shaderOptions = shaderOptionGroupLayout.GetShaderOptions();
        return AZStd::vector<RPI::ShaderOptionDescriptor>(shaderOptions.begin(), shaderOptions.end());
    }

    bool PopulateStageInterfacesFromJsonFiles(
        AZStd::string_view builderName,
        const AZStd::string& preprocessedSourcePath,
        const AZStd::string& tempDirPath,
        const AZStd::string& iaJsonPath,
        const AZStd::string& omJsonPath,
        const MapOfStringToStageType& shaderEntryPoints,
        ShaderReflectionData& reflectionData)
    {
        const AZStd::string builderNameString(builderName);
        const AzslCompiler azslc(preprocessedSourcePath, tempDirPath);

        AZStd::optional<rapidjson::Document> iaDocument;
        AZStd::optional<rapidjson::Document> omDocument;
        for (const auto& [shaderEntryName, shaderStageType] : shaderEntryPoints)
        {
            if (shaderStageType == RPI::ShaderStageType::Vertex)
            {
                if (!iaDocument)
                {
                    auto jsonOutcome = JsonSerializationUtils::ReadJsonFile(iaJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
                    if (!jsonOutcome.IsSuccess())
                    {
                        AZ_Error(builderNameString.c_str(), false, "%s", jsonOutcome.GetError().c_str());
                        return false;
                    }
                    iaDocument.emplace(jsonOutcome.TakeValue());
                }

                StructData inputStruct;
                inputStruct.m_id = "";
                if (!azslc.ParseIaPopulateStructData(*iaDocument, shaderEntryName, inputStruct))
                {
                    AZ_Error(builderNameString.c_str(), false, "Failed to parse input layout");
                    return false;
                }
                if (inputStruct.m_id.empty())
                {
                    AZ_Error(builderNameString.c_str(), false, "Failed to find the input struct for vertex shader %s", shaderEntryName.c_str());
                    return false;
                }

                AZStd::vector<ShaderStageInputReflection>& inputMembers = reflectionData.m_vertexEntryInputs[shaderEntryName];
                for (const StructParameter& member : inputStruct.m_members)
                {
                    ShaderStageInputReflection inputReflection;
                    inputReflection.m_name = member.m_variable.m_name;
                    inputReflection.m_semanticText = member.m_semanticText;
                    inputReflection.m_semanticIndex = member.m_semanticIndex;
                    if (member.m_variable.m_typeModifier == MatrixMajor::ColumnMajor)
                    {
                        inputReflection.m_componentCount = member.m_variable.m_cols;
                    }
                    else
                    {
                        inputReflection.m_componentCount = member.m_variable.m_rows;
                    }
                    inputMembers.push_back(AZStd::move(inputReflection));
                }
            }

            if (shaderStageType == RPI::ShaderStageType::Fragment)
            {
                if (!omDocument)
                {
                    auto jsonOutcome = JsonSerializationUtils::ReadJsonFile(omJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
                    if (!jsonOutcome.IsSuccess())
                    {
                        AZ_Error(builderNameString.c_str(), false, "%s", jsonOutcome.GetError().c_str());
                        return false;
                    }
                    omDocument.emplace(jsonOutcome.TakeValue());
                }

                StructData outputStruct;
                outputStruct.m_id = "";
                if (!azslc.ParseOmPopulateStructData(*omDocument, shaderEntryName, outputStruct))
                {
                    AZ_Error(builderNameString.c_str(), false, "Failed to parse output layout");
                    return false;
                }

                AZStd::vector<ShaderStageOutputReflection>& outputMembers = reflectionData.m_fragmentEntryOutputs[shaderEntryName];
                for (const StructParameter& member : outputStruct.m_members)
                {
                    ShaderStageOutputReflection outputReflection;
                    outputReflection.m_semanticText = member.m_semanticText;
                    // Render targets only support 1-D vector types and those are always column-major (per DXC)
                    outputReflection.m_componentCount = member.m_variable.m_cols;
                    outputMembers.push_back(AZStd::move(outputReflection));
                }
            }
        }

        return true;
    }

    AZ::Outcome<ShaderReflectionData, AZStd::string> BuildReflectionData(
        AZStd::string_view builderName,
        const AzslData& azslData,
        const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout,
        bool usesSpecializationConstants,
        const BindingDependencies& bindingDependencies,
        const RootConstantData& rootConstantData,
        const AZStd::string& iaJsonPath,
        const AZStd::string& omJsonPath,
        const MapOfStringToStageType& shaderEntryPoints,
        const AZStd::string& tempDirPath)
    {
        ShaderReflectionData reflectionData;

        if (!ConvertSrgDataToReflection(builderName, azslData.m_srgData, reflectionData.m_shaderResourceGroups))
        {
            return AZ::Failure(AZStd::string("Failed to convert Shader Resource Group reflection"));
        }

        ConvertBindingDependenciesToReflection(bindingDependencies, reflectionData.m_shaderResourceGroupBindings);
        reflectionData.m_rootConstants = ConvertRootConstantDataToReflection(rootConstantData);
        reflectionData.m_functions = ConvertFunctionsToReflection(azslData.m_functions);
        reflectionData.m_shaderOptions = ConvertShaderOptionsToReflection(shaderOptionGroupLayout);
        reflectionData.m_usesSpecializationConstants = usesSpecializationConstants;

        if (!PopulateStageInterfacesFromJsonFiles(
                builderName,
                azslData.m_preprocessedFullPath,
                tempDirPath,
                iaJsonPath,
                omJsonPath,
                shaderEntryPoints,
                reflectionData))
        {
            return AZ::Failure(AZStd::string("Failed to build the per-entry stage interface reflection"));
        }

        return AZ::Success(AZStd::move(reflectionData));
    }
} // namespace AZ::ShaderBuilder::AzslcReflectionAdapter
