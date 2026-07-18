/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangReflectionWalker.h"

#include <AzCore/StringFunc/StringFunc.h>

namespace AZ::ShaderBuilder::SlangReflectionWalker
{
    static constexpr char WalkerName[] = "SlangReflectionWalker";

    static slang::UserAttribute* FindUserAttribute(slang::VariableReflection* variable, const char* attributeName)
    {
        for (unsigned index = 0; index < variable->getUserAttributeCount(); ++index)
        {
            slang::UserAttribute* attribute = variable->getUserAttributeByIndex(index);
            if (azstricmp(attribute->getName(), attributeName) == 0)
            {
                return attribute;
            }
        }
        return nullptr;
    }

    static slang::UserAttribute* FindUserAttribute(slang::TypeReflection* type, const char* attributeName)
    {
        for (unsigned index = 0; index < type->getUserAttributeCount(); ++index)
        {
            slang::UserAttribute* attribute = type->getUserAttributeByIndex(index);
            if (azstricmp(attribute->getName(), attributeName) == 0)
            {
                return attribute;
            }
        }
        return nullptr;
    }

    static AZStd::string GetAttributeStringArgument(slang::UserAttribute* attribute, unsigned argumentIndex)
    {
        size_t length = 0;
        if (const char* text = attribute->getArgumentValueString(argumentIndex, &length); text && length > 0)
        {
            return AZStd::string(text, length);
        }
        return {};
    }

    //! The reflection category that carries a resource binding for the target: SPIR-V exposes
    //! every resource as a descriptor-table slot; DXIL splits bindings by register class.
    static SlangParameterCategory GetBindingCategory(
        RHI::ShaderTargetFormat targetFormat,
        slang::TypeReflection::Kind kind,
        SlangResourceAccess access)
    {
        if (targetFormat == RHI::ShaderTargetFormat::Spirv)
        {
            return SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT;
        }

        switch (kind)
        {
        case slang::TypeReflection::Kind::SamplerState:
            return SLANG_PARAMETER_CATEGORY_SAMPLER_STATE;
        case slang::TypeReflection::Kind::ConstantBuffer:
            return SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER;
        case slang::TypeReflection::Kind::Resource:
        default:
            if (access == SLANG_RESOURCE_ACCESS_READ)
            {
                return SLANG_PARAMETER_CATEGORY_SHADER_RESOURCE;
            }
            return SLANG_PARAMETER_CATEGORY_UNORDERED_ACCESS;
        }
    }

    static bool ToImageType(SlangResourceShape shape, RHI::ShaderInputImageType& imageType)
    {
        const SlangResourceShape baseShape = static_cast<SlangResourceShape>(shape & SLANG_RESOURCE_BASE_SHAPE_MASK);
        const bool isArray = (shape & SLANG_TEXTURE_ARRAY_FLAG) != 0;
        const bool isMultisample = (shape & SLANG_TEXTURE_MULTISAMPLE_FLAG) != 0;
        switch (baseShape)
        {
        case SLANG_TEXTURE_1D:
            imageType = isArray ? RHI::ShaderInputImageType::Image1DArray : RHI::ShaderInputImageType::Image1D;
            return true;
        case SLANG_TEXTURE_2D:
            if (isMultisample)
            {
                imageType = isArray ? RHI::ShaderInputImageType::Image2DMultisampleArray : RHI::ShaderInputImageType::Image2DMultisample;
            }
            else
            {
                imageType = isArray ? RHI::ShaderInputImageType::Image2DArray : RHI::ShaderInputImageType::Image2D;
            }
            return true;
        case SLANG_TEXTURE_3D:
            imageType = RHI::ShaderInputImageType::Image3D;
            return true;
        case SLANG_TEXTURE_CUBE:
            imageType = RHI::ShaderInputImageType::ImageCube;
            return true;
        case SLANG_TEXTURE_SUBPASS:
            imageType = RHI::ShaderInputImageType::SubpassInput;
            return true;
        default:
            return false;
        }
    }

    static bool ToBufferType(SlangResourceShape shape, RHI::ShaderInputBufferType& bufferType)
    {
        switch (shape & SLANG_RESOURCE_BASE_SHAPE_MASK)
        {
        case SLANG_TEXTURE_BUFFER:
            bufferType = RHI::ShaderInputBufferType::Typed;
            return true;
        case SLANG_STRUCTURED_BUFFER:
            bufferType = RHI::ShaderInputBufferType::Structured;
            return true;
        case SLANG_BYTE_ADDRESS_BUFFER:
            bufferType = RHI::ShaderInputBufferType::Raw;
            return true;
        case SLANG_ACCELERATION_STRUCTURE:
            bufferType = RHI::ShaderInputBufferType::AccelerationStructure;
            return true;
        default:
            return false;
        }
    }

    static RHI::SamplerState BuildStaticSamplerState(slang::UserAttribute* attribute)
    {
        // Argument order matches AtomStaticSamplerAttribute:
        // (maxAnisotropy, addressMode, filterMode, comparisonFunc, borderColor)
        int maxAnisotropy = 0;
        attribute->getArgumentValueInt(0, &maxAnisotropy);
        const AZStd::string addressMode = GetAttributeStringArgument(attribute, 1);
        const AZStd::string filterMode = GetAttributeStringArgument(attribute, 2);
        const AZStd::string comparisonFunc = GetAttributeStringArgument(attribute, 3);
        const AZStd::string borderColor = GetAttributeStringArgument(attribute, 4);

        auto parseAddressMode = [](const AZStd::string& text)
        {
            if (AZ::StringFunc::Equal(text, "Mirror")) { return RHI::AddressMode::Mirror; }
            if (AZ::StringFunc::Equal(text, "Clamp")) { return RHI::AddressMode::Clamp; }
            if (AZ::StringFunc::Equal(text, "Border")) { return RHI::AddressMode::Border; }
            if (AZ::StringFunc::Equal(text, "MirrorOnce")) { return RHI::AddressMode::MirrorOnce; }
            return RHI::AddressMode::Wrap;
        };
        auto parseFilterMode = [](const AZStd::string& text)
        {
            if (AZ::StringFunc::Equal(text, "Point")) { return RHI::FilterMode::Point; }
            return RHI::FilterMode::Linear;
        };
        auto parseBorderColor = [](const AZStd::string& text)
        {
            if (AZ::StringFunc::Equal(text, "OpaqueBlack")) { return RHI::BorderColor::OpaqueBlack; }
            if (AZ::StringFunc::Equal(text, "OpaqueWhite")) { return RHI::BorderColor::OpaqueWhite; }
            return RHI::BorderColor::TransparentBlack;
        };
        auto parseComparisonFunc = [](const AZStd::string& text)
        {
            if (AZ::StringFunc::Equal(text, "Less")) { return RHI::ComparisonFunc::Less; }
            if (AZ::StringFunc::Equal(text, "Equal")) { return RHI::ComparisonFunc::Equal; }
            if (AZ::StringFunc::Equal(text, "LessEqual")) { return RHI::ComparisonFunc::LessEqual; }
            if (AZ::StringFunc::Equal(text, "Greater")) { return RHI::ComparisonFunc::Greater; }
            if (AZ::StringFunc::Equal(text, "NotEqual")) { return RHI::ComparisonFunc::NotEqual; }
            if (AZ::StringFunc::Equal(text, "GreaterEqual")) { return RHI::ComparisonFunc::GreaterEqual; }
            if (AZ::StringFunc::Equal(text, "Always")) { return RHI::ComparisonFunc::Always; }
            return RHI::ComparisonFunc::Never;
        };

        RHI::SamplerState samplerState;
        if (maxAnisotropy > 1)
        {
            samplerState = RHI::SamplerState::CreateAnisotropic(maxAnisotropy, parseAddressMode(addressMode));
        }
        else
        {
            samplerState = RHI::SamplerState::Create(
                parseFilterMode(filterMode), parseFilterMode(filterMode), parseAddressMode(addressMode), parseBorderColor(borderColor));
        }
        samplerState.m_comparisonFunc = parseComparisonFunc(comparisonFunc);
        return samplerState;
    }

    //! One resolved member binding: the register index in the target's binding category and the
    //! register space/descriptor set it lives in.
    struct MemberBinding
    {
        uint32_t m_registerId = 0;
        uint32_t m_spaceId = 0;
    };

    //! Answers "which entry points actually use the resource at this binding location" from the
    //! per-entry IMetadata of the linked program. Metadata pointers are held raw with manual
    //! release (AZStd containers cannot hold Slang::ComPtr).
    class EntryPointUsageOracle final
    {
    public:
        EntryPointUsageOracle(slang::IComponentType* linkedProgram, AZStd::span<const AZStd::string> entryPointNamesInOrder)
        {
            for (size_t entryPointIndex = 0; entryPointIndex < entryPointNamesInOrder.size(); ++entryPointIndex)
            {
                slang::IMetadata* metadata = nullptr;
                linkedProgram->getEntryPointMetadata(aznumeric_cast<SlangInt>(entryPointIndex), 0, &metadata);
                m_entryPointNames.push_back(entryPointNamesInOrder[entryPointIndex]);
                m_metadata.push_back(metadata);
            }
        }

        ~EntryPointUsageOracle()
        {
            for (slang::IMetadata* metadata : m_metadata)
            {
                if (metadata)
                {
                    metadata->release();
                }
            }
        }

        //! Returns the entry points whose generated code uses the binding; falls back to every
        //! entry point when metadata is unavailable.
        AZStd::vector<AZStd::string> DependentFunctions(SlangParameterCategory category, const MemberBinding& binding) const
        {
            AZStd::vector<AZStd::string> dependentFunctions;
            for (size_t entryPointIndex = 0; entryPointIndex < m_metadata.size(); ++entryPointIndex)
            {
                bool used = true;
                if (!m_metadata[entryPointIndex]
                    || SLANG_FAILED(m_metadata[entryPointIndex]->isParameterLocationUsed(category, binding.m_spaceId, binding.m_registerId, used)))
                {
                    used = true;
                }
                if (used)
                {
                    dependentFunctions.push_back(m_entryPointNames[entryPointIndex]);
                }
            }
            return dependentFunctions;
        }

    private:
        AZStd::vector<AZStd::string> m_entryPointNames;
        AZStd::vector<slang::IMetadata*> m_metadata;
    };

    static MemberBinding GetMemberBinding(
        SlangParameterCategory category,
        slang::VariableLayoutReflection* memberLayout,
        uint32_t blockSpace)
    {
        MemberBinding binding;
        binding.m_registerId = aznumeric_cast<uint32_t>(memberLayout->getOffset(category));
        binding.m_spaceId = blockSpace + aznumeric_cast<uint32_t>(memberLayout->getBindingSpace(category));
        return binding;
    }

    //! Appends one member (of a ParameterBlock element or an attribute-grouped global) to the
    //! SRG entry. @arrayCount -1 marks unbounded arrays.
    static bool AppendMember(
        ShaderResourceGroupReflection& srgReflection,
        ShaderResourceGroupBindingReflection& srgBindings,
        RHI::ShaderTargetFormat targetFormat,
        slang::VariableLayoutReflection* memberLayout,
        AZStd::string_view memberName,
        slang::TypeLayoutReflection* memberTypeLayout,
        int64_t arrayCount,
        uint32_t blockSpace,
        const MemberBinding& constantsBinding,
        const EntryPointUsageOracle& usageOracle)
    {
        const slang::TypeReflection::Kind kind = memberTypeLayout->getKind();
        const Name memberNameId{memberName};

        // Arrays of resources unwrap to their element type; unbounded arrays keep count -1.
        if (kind == slang::TypeReflection::Kind::Array)
        {
            const size_t elementCount = memberTypeLayout->getElementCount();
            return AppendMember(
                srgReflection, srgBindings, targetFormat, memberLayout, memberName,
                memberTypeLayout->getElementTypeLayout(),
                elementCount == 0 ? -1 : aznumeric_cast<int64_t>(elementCount),
                blockSpace, constantsBinding, usageOracle);
        }

        auto appendBindingDependency = [&](SlangParameterCategory category, const MemberBinding& binding)
        {
            ResourceBindingReflection bindingReflection;
            bindingReflection.m_name = memberNameId;
            bindingReflection.m_registerId = binding.m_registerId;
            bindingReflection.m_registerSpace = binding.m_spaceId;
            bindingReflection.m_dependentFunctions = usageOracle.DependentFunctions(category, binding);
            srgBindings.m_resourceBindings.push_back(AZStd::move(bindingReflection));
        };

        const uint32_t count = arrayCount < 0 ? 1 : aznumeric_cast<uint32_t>(arrayCount);

        switch (kind)
        {
        case slang::TypeReflection::Kind::SamplerState:
        {
            const SlangParameterCategory category = GetBindingCategory(targetFormat, kind, SLANG_RESOURCE_ACCESS_READ);
            const MemberBinding binding = GetMemberBinding(category, memberLayout, blockSpace);
            if (slang::UserAttribute* staticSampler = FindUserAttribute(memberLayout->getVariable(), "AtomStaticSampler"))
            {
                srgReflection.m_staticSamplers.push_back({memberNameId, BuildStaticSamplerState(staticSampler), binding.m_registerId, binding.m_spaceId});
            }
            else
            {
                srgReflection.m_samplers.push_back({memberNameId, count, binding.m_registerId, binding.m_spaceId});
            }
            appendBindingDependency(category, binding);
            return true;
        }
        case slang::TypeReflection::Kind::ConstantBuffer:
        {
            const SlangParameterCategory category = GetBindingCategory(targetFormat, kind, SLANG_RESOURCE_ACCESS_READ);
            const MemberBinding binding = GetMemberBinding(category, memberLayout, blockSpace);
            const uint32_t strideSize = aznumeric_cast<uint32_t>(
                memberTypeLayout->getElementTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
            srgReflection.m_buffers.push_back({memberNameId, RHI::ShaderInputBufferAccess::Constant, RHI::ShaderInputBufferType::Constant,
                count, strideSize, binding.m_registerId, binding.m_spaceId});
            appendBindingDependency(category, binding);
            return true;
        }
        case slang::TypeReflection::Kind::Resource:
        {
            slang::TypeReflection* type = memberTypeLayout->getType();
            const SlangResourceShape shape = type->getResourceShape();
            const SlangResourceAccess access = type->getResourceAccess();
            const SlangParameterCategory category = GetBindingCategory(targetFormat, kind, access);
            const MemberBinding binding = GetMemberBinding(category, memberLayout, blockSpace);
            const bool isReadOnly = access == SLANG_RESOURCE_ACCESS_READ;

            RHI::ShaderInputImageType imageType = RHI::ShaderInputImageType::Unknown;
            if (ToImageType(shape, imageType))
            {
                const RHI::ShaderInputImageAccess imageAccess = isReadOnly ? RHI::ShaderInputImageAccess::Read : RHI::ShaderInputImageAccess::ReadWrite;
                if (arrayCount < 0)
                {
                    srgReflection.m_imageUnboundedArrays.push_back({memberNameId, imageAccess, imageType, binding.m_registerId, binding.m_spaceId});
                }
                else
                {
                    srgReflection.m_images.push_back({memberNameId, imageAccess, imageType, count, binding.m_registerId, binding.m_spaceId});
                }
                appendBindingDependency(category, binding);
                return true;
            }

            RHI::ShaderInputBufferType bufferType = RHI::ShaderInputBufferType::Unknown;
            if (ToBufferType(shape, bufferType))
            {
                const RHI::ShaderInputBufferAccess bufferAccess = isReadOnly ? RHI::ShaderInputBufferAccess::Read : RHI::ShaderInputBufferAccess::ReadWrite;
                uint32_t strideSize = 0;
                if (slang::TypeLayoutReflection* elementLayout = memberTypeLayout->getElementTypeLayout())
                {
                    strideSize = aznumeric_cast<uint32_t>(elementLayout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
                }
                if (arrayCount < 0)
                {
                    srgReflection.m_bufferUnboundedArrays.push_back({memberNameId, bufferAccess, bufferType, strideSize, binding.m_registerId, binding.m_spaceId});
                }
                else
                {
                    srgReflection.m_buffers.push_back({memberNameId, bufferAccess, bufferType, count, strideSize, binding.m_registerId, binding.m_spaceId});
                }
                appendBindingDependency(category, binding);
                return true;
            }

            AZ_Error(WalkerName, false, "Resource %.*s has a shape the walker does not support yet", AZ_STRING_ARG(memberName));
            return false;
        }
        default:
            // Everything else is uniform data: a loose member of the implicit SRG-constants buffer
            const uint32_t byteOffset = aznumeric_cast<uint32_t>(memberLayout->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
            const uint32_t byteSize = aznumeric_cast<uint32_t>(memberTypeLayout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
            srgReflection.m_constants.push_back({memberNameId, byteOffset, byteSize, constantsBinding.m_registerId, constantsBinding.m_spaceId});
            return true;
        }
    }

    AZ::Outcome<ShaderReflectionData, AZStd::string> BuildReflectionData(
        slang::IComponentType* linkedProgram,
        RHI::ShaderTargetFormat targetFormat,
        AZStd::span<const AZStd::string> entryPointNamesInOrder)
    {
        slang::ShaderReflection* programLayout = linkedProgram ? linkedProgram->getLayout(0) : nullptr;
        if (!programLayout)
        {
            return AZ::Failure(AZStd::string("No reflection layout is available for the linked program"));
        }

        ShaderReflectionData reflectionData;
        const EntryPointUsageOracle usageOracle(linkedProgram, entryPointNamesInOrder);

        // Indexed by SRG name so ParameterBlocks and attribute-grouped globals can accumulate
        // into the same group; vector order preserves first-appearance order.
        auto findOrAddGroup = [&reflectionData](AZStd::string_view srgName, uint32_t bindingSlot) -> ShaderResourceGroupReflection&
        {
            for (ShaderResourceGroupReflection& existing : reflectionData.m_shaderResourceGroups)
            {
                if (existing.m_name.GetStringView() == srgName)
                {
                    return existing;
                }
            }
            ShaderResourceGroupReflection& added = reflectionData.m_shaderResourceGroups.emplace_back();
            added.m_name = Name{srgName};
            added.m_uniqueId = srgName;
            added.m_bindingSlot = bindingSlot;
            reflectionData.m_shaderResourceGroupBindings.emplace(AZStd::string(srgName), ShaderResourceGroupBindingReflection{});
            return added;
        };

        const unsigned parameterCount = programLayout->getParameterCount();
        for (unsigned parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            slang::VariableLayoutReflection* parameterLayout = programLayout->getParameterByIndex(parameterIndex);
            slang::VariableReflection* variable = parameterLayout->getVariable();
            slang::TypeLayoutReflection* typeLayout = parameterLayout->getTypeLayout();
            const char* parameterName = variable->getName();

            if (typeLayout->getKind() == slang::TypeReflection::Kind::ParameterBlock)
            {
                slang::TypeLayoutReflection* elementTypeLayout = typeLayout->getElementTypeLayout();
                slang::TypeReflection* elementType = elementTypeLayout->getType();

                slang::UserAttribute* srgAttribute = FindUserAttribute(elementType, "AtomShaderResourceGroup");
                if (!srgAttribute)
                {
                    return AZ::Failure(AZStd::string::format(
                        "ParameterBlock %s has no [AtomShaderResourceGroup(bindingSlot)] attribute on its element type", parameterName));
                }
                int bindingSlot = 0;
                srgAttribute->getArgumentValueInt(0, &bindingSlot);

                ShaderResourceGroupReflection& srgReflection = findOrAddGroup(parameterName, aznumeric_cast<uint32_t>(bindingSlot));
                ShaderResourceGroupBindingReflection& srgBindings = reflectionData.m_shaderResourceGroupBindings[parameterName];

                // The block's register space (DXIL) / descriptor set (SPIR-V)
                const uint32_t blockSpace = aznumeric_cast<uint32_t>(
                    parameterLayout->getOffset(SLANG_PARAMETER_CATEGORY_SUB_ELEMENT_REGISTER_SPACE)
                    + parameterLayout->getBindingSpace(SLANG_PARAMETER_CATEGORY_SUB_ELEMENT_REGISTER_SPACE));

                // The implicit constant buffer holding loose members, when any exist
                MemberBinding constantsBinding;
                const size_t uniformSize = elementTypeLayout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                if (uniformSize > 0)
                {
                    slang::VariableLayoutReflection* containerLayout = typeLayout->getContainerVarLayout();
                    const SlangParameterCategory containerCategory = targetFormat == RHI::ShaderTargetFormat::Spirv
                        ? SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT
                        : SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER;
                    constantsBinding.m_registerId = aznumeric_cast<uint32_t>(containerLayout->getOffset(containerCategory));
                    constantsBinding.m_spaceId = blockSpace;

                    const SlangParameterCategory containerBindingCategory = targetFormat == RHI::ShaderTargetFormat::Spirv
                        ? SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT
                        : SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER;
                    srgBindings.m_constantDataBinding.m_name = Name{AZStd::string::format("%s_SRGConstantBuffer", parameterName)};
                    srgBindings.m_constantDataBinding.m_registerId = constantsBinding.m_registerId;
                    srgBindings.m_constantDataBinding.m_registerSpace = constantsBinding.m_spaceId;
                    srgBindings.m_constantDataBinding.m_dependentFunctions = usageOracle.DependentFunctions(containerBindingCategory, constantsBinding);
                }

                const unsigned fieldCount = elementTypeLayout->getFieldCount();
                for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
                {
                    slang::VariableLayoutReflection* fieldLayout = elementTypeLayout->getFieldByIndex(fieldIndex);
                    if (!AppendMember(
                            srgReflection, srgBindings, targetFormat, fieldLayout, fieldLayout->getVariable()->getName(),
                            fieldLayout->getTypeLayout(), 1, blockSpace, constantsBinding, usageOracle))
                    {
                        return AZ::Failure(AZStd::string::format("Failed to reflect member %s of %s", fieldLayout->getVariable()->getName(), parameterName));
                    }
                }
                continue;
            }

            // Attribute-grouped globals (the Bindless group)
            if (slang::UserAttribute* memberAttribute = FindUserAttribute(variable, "AtomShaderResourceGroupMember"))
            {
                const AZStd::string srgName = GetAttributeStringArgument(memberAttribute, 0);
                int bindingSlot = 0;
                memberAttribute->getArgumentValueInt(1, &bindingSlot);
                if (srgName.empty())
                {
                    return AZ::Failure(AZStd::string::format("%s: [AtomShaderResourceGroupMember] needs a group name", parameterName));
                }

                ShaderResourceGroupReflection& srgReflection = findOrAddGroup(srgName, aznumeric_cast<uint32_t>(bindingSlot));
                ShaderResourceGroupBindingReflection& srgBindings = reflectionData.m_shaderResourceGroupBindings[srgName];

                // A grouped ConstantBuffer named <group>_SRGConstantBuffer is the pinned form of
                // the group's SRG-constants buffer (how shared-SRG declarations express what a
                // ParameterBlock holds implicitly): its element fields become the SRG constants.
                const AZStd::string expectedConstantBufferName = AZStd::string::format("%s_SRGConstantBuffer", srgName.c_str());
                if (typeLayout->getKind() == slang::TypeReflection::Kind::ConstantBuffer && expectedConstantBufferName == parameterName)
                {
                    const SlangParameterCategory category = GetBindingCategory(targetFormat, typeLayout->getKind(), SLANG_RESOURCE_ACCESS_READ);
                    const MemberBinding binding = GetMemberBinding(category, parameterLayout, 0);
                    slang::TypeLayoutReflection* elementTypeLayout = typeLayout->getElementTypeLayout();
                    for (unsigned fieldIndex = 0; fieldIndex < elementTypeLayout->getFieldCount(); ++fieldIndex)
                    {
                        slang::VariableLayoutReflection* fieldLayout = elementTypeLayout->getFieldByIndex(fieldIndex);
                        srgReflection.m_constants.push_back({
                            Name{fieldLayout->getVariable()->getName()},
                            aznumeric_cast<uint32_t>(fieldLayout->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM)),
                            aznumeric_cast<uint32_t>(fieldLayout->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM)),
                            binding.m_registerId,
                            binding.m_spaceId});
                    }
                    srgBindings.m_constantDataBinding.m_name = Name{expectedConstantBufferName};
                    srgBindings.m_constantDataBinding.m_registerId = binding.m_registerId;
                    srgBindings.m_constantDataBinding.m_registerSpace = binding.m_spaceId;
                    srgBindings.m_constantDataBinding.m_dependentFunctions = usageOracle.DependentFunctions(category, binding);
                    continue;
                }

                if (!AppendMember(srgReflection, srgBindings, targetFormat, parameterLayout, parameterName, typeLayout, 1, 0, MemberBinding{}, usageOracle))
                {
                    return AZ::Failure(AZStd::string::format("Failed to reflect grouped global %s", parameterName));
                }
                continue;
            }

            return AZ::Failure(AZStd::string::format(
                "Global shader parameter %s is neither a ShaderResourceGroup ParameterBlock nor an [AtomShaderResourceGroupMember]-grouped declaration",
                parameterName));
        }

        // Entry points: stage interfaces and attributes
        const SlangUInt entryPointCount = programLayout->getEntryPointCount();
        for (SlangUInt entryPointIndex = 0; entryPointIndex < entryPointCount; ++entryPointIndex)
        {
            slang::EntryPointReflection* entryPoint = programLayout->getEntryPointByIndex(entryPointIndex);
            const AZStd::string entryPointName = entryPoint->getName();

            ShaderFunctionReflection functionReflection;
            functionReflection.m_name = entryPointName;

            if (entryPoint->getStage() == SLANG_STAGE_COMPUTE)
            {
                SlangUInt threadGroupSize[3] = {1, 1, 1};
                entryPoint->getComputeThreadGroupSize(3, threadGroupSize);
                ShaderFunctionAttributeReflection numThreads;
                numThreads.m_name = Name{"numthreads"};
                for (const SlangUInt axisSize : threadGroupSize)
                {
                    numThreads.m_arguments.push_back(aznumeric_cast<int32_t>(axisSize));
                }
                functionReflection.m_attributes.push_back(AZStd::move(numThreads));
            }
            reflectionData.m_functions.push_back(AZStd::move(functionReflection));

            // Vertex inputs: parameters (and struct fields) with semantics
            if (entryPoint->getStage() == SLANG_STAGE_VERTEX)
            {
                AZStd::vector<ShaderStageInputReflection>& inputMembers = reflectionData.m_vertexEntryInputs[entryPointName];
                for (unsigned parameterIndexInEntry = 0; parameterIndexInEntry < entryPoint->getParameterCount(); ++parameterIndexInEntry)
                {
                    slang::VariableLayoutReflection* inputLayout = entryPoint->getParameterByIndex(parameterIndexInEntry);
                    auto appendInput = [&inputMembers](slang::VariableLayoutReflection* varLayout)
                    {
                        if (!varLayout->getSemanticName())
                        {
                            return;
                        }
                        ShaderStageInputReflection inputReflection;
                        inputReflection.m_name = varLayout->getVariable()->getName();
                        inputReflection.m_semanticText = varLayout->getSemanticName();
                        inputReflection.m_semanticIndex = aznumeric_cast<int32_t>(varLayout->getSemanticIndex());
                        slang::TypeReflection* inputType = varLayout->getTypeLayout()->getType();
                        inputReflection.m_componentCount = inputType->getKind() == slang::TypeReflection::Kind::Vector
                            ? aznumeric_cast<uint32_t>(inputType->getElementCount())
                            : 1;
                        inputMembers.push_back(AZStd::move(inputReflection));
                    };

                    slang::TypeLayoutReflection* inputTypeLayout = inputLayout->getTypeLayout();
                    if (inputTypeLayout->getKind() == slang::TypeReflection::Kind::Struct)
                    {
                        for (unsigned fieldIndex = 0; fieldIndex < inputTypeLayout->getFieldCount(); ++fieldIndex)
                        {
                            appendInput(inputTypeLayout->getFieldByIndex(fieldIndex));
                        }
                    }
                    else
                    {
                        appendInput(inputLayout);
                    }
                }
            }

            // Fragment outputs: the result (and struct fields) with semantics
            if (entryPoint->getStage() == SLANG_STAGE_FRAGMENT)
            {
                AZStd::vector<ShaderStageOutputReflection>& outputMembers = reflectionData.m_fragmentEntryOutputs[entryPointName];
                slang::VariableLayoutReflection* resultLayout = entryPoint->getResultVarLayout();
                auto appendOutput = [&outputMembers](slang::VariableLayoutReflection* varLayout, const char* fallbackSemantic)
                {
                    const char* semanticName = varLayout->getSemanticName() ? varLayout->getSemanticName() : fallbackSemantic;
                    if (!semanticName)
                    {
                        return;
                    }
                    ShaderStageOutputReflection outputReflection;
                    outputReflection.m_semanticText = AZStd::string::format(
                        "%s%zu", semanticName, varLayout->getSemanticName() ? varLayout->getSemanticIndex() : 0);
                    slang::TypeReflection* outputType = varLayout->getTypeLayout()->getType();
                    outputReflection.m_componentCount = outputType->getKind() == slang::TypeReflection::Kind::Vector
                        ? aznumeric_cast<uint32_t>(outputType->getElementCount())
                        : 1;
                    outputMembers.push_back(AZStd::move(outputReflection));
                };

                if (resultLayout)
                {
                    slang::TypeLayoutReflection* resultTypeLayout = resultLayout->getTypeLayout();
                    if (resultTypeLayout->getKind() == slang::TypeReflection::Kind::Struct)
                    {
                        for (unsigned fieldIndex = 0; fieldIndex < resultTypeLayout->getFieldCount(); ++fieldIndex)
                        {
                            appendOutput(resultTypeLayout->getFieldByIndex(fieldIndex), nullptr);
                        }
                    }
                    else if (resultTypeLayout->getKind() != slang::TypeReflection::Kind::None)
                    {
                        appendOutput(resultLayout, "SV_Target");
                    }
                }
            }
        }

        // Mirror AZSLC's empty-options layout so the ShaderVariantKey machinery sees the same
        // single boolean DefaultOption; when the shader declares ATOM_OPTIONs, the backend
        // replaces this with the discovered descriptors after the walk
        const AZStd::vector<RPI::ShaderOptionValuePair> defaultOptionValues = {
            {Name("false"), RPI::ShaderOptionValue(0)},
            {Name("true"), RPI::ShaderOptionValue(1)},
        };
        reflectionData.m_shaderOptions.push_back(RPI::ShaderOptionDescriptor{
            Name("DefaultOption"), RPI::ShaderOptionType::Boolean, 0, 0, defaultOptionValues, Name("false")});

        return AZ::Success(AZStd::move(reflectionData));
    }
} // namespace AZ::ShaderBuilder::SlangReflectionWalker
