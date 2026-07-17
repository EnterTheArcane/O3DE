/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ShaderStages.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderInputContract.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroupLayout.h>
#include <Atom/RPI.Reflect/Shader/ShaderOutputContract.h>

namespace AZ
{
    class ReflectContext;

    namespace RHI
    {
        class PipelineLayoutDescriptor;
        class ShaderPlatformInterface;
        struct ShaderBuildArguments;
    }
}

namespace AZ::ShaderBuilder
{
    using MapOfStringToStageType = AZStd::unordered_map<AZStd::string, RPI::ShaderStageType>;

    //! One positional argument of a shader function attribute. The alternatives mirror the value
    //! kinds a language frontend can produce; monostate stands for an argument of unknown kind.
    using ShaderFunctionAttributeArgument = AZStd::variant<AZStd::monostate, bool, int32_t, double, AZStd::string>;

    //! One attribute placed on a shader entry function, e.g. [numthreads(8, 8, 1)].
    struct ShaderFunctionAttributeReflection final
    {
        AZ_TYPE_INFO(ShaderFunctionAttributeReflection, "{0C0B7B2E-9B7F-4B6E-9F2B-2E63C5F8A331}");

        Name m_name;
        AZStd::vector<ShaderFunctionAttributeArgument> m_arguments;
    };

    //! A shader function eligible as an entry point, with its attributes.
    struct ShaderFunctionReflection final
    {
        AZ_TYPE_INFO(ShaderFunctionReflection, "{B5C08E5B-86F1-4B77-BB7E-0D2F4C1A6E42}");

        AZStd::string m_name;
        AZStd::vector<ShaderFunctionAttributeReflection> m_attributes;
    };

    //! One member of a vertex entry input structure. The component count is already resolved
    //! against matrix majorness by the language backend.
    struct ShaderStageInputReflection final
    {
        AZ_TYPE_INFO(ShaderStageInputReflection, "{A6F53E6C-1B67-4A9B-8F3E-6E1D2C4B5A70}");

        AZStd::string m_name;
        AZStd::string m_semanticText;
        int32_t m_semanticIndex = -1;
        uint32_t m_componentCount = 1;
    };

    //! One member of a fragment entry output structure.
    struct ShaderStageOutputReflection final
    {
        AZ_TYPE_INFO(ShaderStageOutputReflection, "{7D8B7C2A-3F1E-4D5B-A6C8-9E0F1A2B3C4D}");

        AZStd::string m_semanticText;
        uint32_t m_componentCount = 1;
    };

    //! Register-level binding of one named shader resource, and the shader functions that
    //! reference it. Stage masks are resolved in shared builder code by matching the dependent
    //! function names against the entry points of the .shader being built.
    struct ResourceBindingReflection final
    {
        AZ_TYPE_INFO(ResourceBindingReflection, "{E2A4D6B8-0C1D-4E3F-A5B7-C9D0E1F2A3B4}");

        Name m_name;
        uint32_t m_registerId = RHI::UndefinedRegisterSlot;
        uint32_t m_registerSpace = RHI::UndefinedRegisterSlot;
        AZStd::vector<AZStd::string> m_dependentFunctions;
    };

    //! Register-level bindings of everything inside one ShaderResourceGroup.
    struct ShaderResourceGroupBindingReflection final
    {
        AZ_TYPE_INFO(ShaderResourceGroupBindingReflection, "{1F2E3D4C-5B6A-4790-8812-93A4B5C6D7E8}");

        //! Binding of the implicit constant buffer that holds all loose SRG constants.
        ResourceBindingReflection m_constantDataBinding;
        AZStd::vector<ResourceBindingReflection> m_resourceBindings;
    };

    //! Everything needed to construct the RHI::ShaderResourceGroupLayout of one
    //! ShaderResourceGroup. The descriptor vectors are the exact inputs the layout takes;
    //! within each vector the order is the layout construction order and is hash-relevant.
    struct ShaderResourceGroupReflection final
    {
        AZ_TYPE_INFO(ShaderResourceGroupReflection, "{6A7B8C9D-0E1F-4234-A567-B89C0D1E2F30}");

        Name m_name;
        //! Uniquifier mixed into the layout hash; today the name of the source file declaring the ShaderResourceGroup.
        AZStd::string m_uniqueId;
        uint32_t m_bindingSlot = RHI::Limits::Pipeline::ShaderResourceGroupCountMax;

        //! A size greater than 0 designates this ShaderResourceGroup as the ShaderVariantKey fallback.
        Name m_shaderVariantKeyFallbackName;
        uint32_t m_shaderVariantKeyFallbackSize = 0;

        AZStd::vector<RHI::ShaderInputStaticSamplerDescriptor> m_staticSamplers;
        AZStd::vector<RHI::ShaderInputSamplerDescriptor> m_samplers;
        AZStd::vector<RHI::ShaderInputImageDescriptor> m_images;
        AZStd::vector<RHI::ShaderInputImageUnboundedArrayDescriptor> m_imageUnboundedArrays;
        AZStd::vector<RHI::ShaderInputBufferDescriptor> m_buffers;
        AZStd::vector<RHI::ShaderInputBufferUnboundedArrayDescriptor> m_bufferUnboundedArrays;
        AZStd::vector<RHI::ShaderInputConstantDescriptor> m_constants;
    };

    //! Root constants of the shader: the register-level binding of the root constant buffer and
    //! the byte layout of each constant inside it.
    struct RootConstantsReflection final
    {
        AZ_TYPE_INFO(RootConstantsReflection, "{3C4D5E6F-7A8B-49C0-B1D2-E3F4A5B6C7D8}");

        struct Constant final
        {
            AZ_TYPE_INFO(Constant, "{9E8D7C6B-5A49-4382-9170-8F6E5D4C3B2A}");

            Name m_name;
            uint32_t m_byteOffset = 0;
            uint32_t m_byteSize = 0;
        };

        Name m_bufferName;
        uint32_t m_sizeInBytes = 0;
        uint32_t m_registerId = RHI::UndefinedRegisterSlot;
        uint32_t m_registerSpace = RHI::UndefinedRegisterSlot;
        AZStd::vector<Constant> m_constants;
    };

    //! The canonical, language-neutral reflection contract every shader language backend produces
    //! from its frontend run — one instance per (RHI API, supervariant). Shared builder code
    //! converts it to the final RHI/RPI objects (SRG layouts, ShaderOptionGroupLayout, pipeline
    //! layout descriptor, input/output contracts); no legacy language-specific structures cross
    //! the backend seam. Versioned and reflected so backends can also serialize it as a job
    //! product (RPI::ShaderAssetSubId::ReflectionData) for downstream builders.
    struct ShaderReflectionData final
    {
        AZ_TYPE_INFO(ShaderReflectionData, "{D1E2F3A4-B5C6-47D8-90E1-F2A3B4C5D6E7}");
        AZ_CLASS_ALLOCATOR(ShaderReflectionData, AZ::SystemAllocator);

        static void Reflect(ReflectContext* context);

        static constexpr uint32_t CurrentSchemaVersion = 1;

        //! Embedded schema version so serialized products can be rejected on mismatch.
        uint32_t m_schemaVersion = CurrentSchemaVersion;

        AZStd::vector<ShaderResourceGroupReflection> m_shaderResourceGroups;

        //! Register-level bindings keyed by ShaderResourceGroup name. A missing key for a group
        //! in m_shaderResourceGroups is an error surfaced when the pipeline layout is built.
        AZStd::unordered_map<AZStd::string, ShaderResourceGroupBindingReflection> m_shaderResourceGroupBindings;

        //! Shader option descriptors in declaration order, ready to rebuild the ShaderOptionGroupLayout.
        AZStd::vector<RPI::ShaderOptionDescriptor> m_shaderOptions;
        bool m_usesSpecializationConstants = false;

        RootConstantsReflection m_rootConstants;

        //! Entry-point candidates with their attributes.
        AZStd::vector<ShaderFunctionReflection> m_functions;

        //! Input structure members per vertex entry point, and output structure members per
        //! fragment entry point, keyed by entry function name.
        AZStd::unordered_map<AZStd::string, AZStd::vector<ShaderStageInputReflection>> m_vertexEntryInputs;
        AZStd::unordered_map<AZStd::string, AZStd::vector<ShaderStageOutputReflection>> m_fragmentEntryOutputs;
    };

    //! Builds the (unfinalized) ShaderResourceGroupLayout list from the reflection data.
    AZ::Outcome<RPI::ShaderResourceGroupLayoutList, AZStd::string> BuildShaderResourceGroupLayouts(const ShaderReflectionData& reflectionData);

    //! Builds the finalized ShaderOptionGroupLayout from the reflection data. Returns nullptr on failure.
    RPI::Ptr<RPI::ShaderOptionGroupLayout> BuildShaderOptionGroupLayout(const ShaderReflectionData& reflectionData);

    //! Converts serializable attribute arguments back to the RHI attribute argument list.
    RHI::ShaderStageAttributeArguments BuildShaderStageAttributeArguments(AZStd::span<const ShaderFunctionAttributeArgument> attributeArguments);

    //! Builds the pipeline layout descriptor for one RHI API: per-resource stage masks are
    //! resolved by matching dependent function names against @shaderEntryPoints, then the
    //! platform-specific descriptor data is built and finalized through @shaderPlatformInterface.
    //! Must be called before ShaderPlatformInterface::CompilePlatformInternal.
    //! Returns nullptr on failure.
    RHI::Ptr<RHI::PipelineLayoutDescriptor> BuildPipelineLayoutDescriptor(
        AZStd::string_view builderName,
        const ShaderReflectionData& reflectionData,
        const RPI::ShaderResourceGroupLayoutList& srgLayoutList,
        const MapOfStringToStageType& shaderEntryPoints,
        const RHI::ShaderBuildArguments& shaderBuildArguments,
        RHI::ShaderPlatformInterface* shaderPlatformInterface);

    //! Builds the vertex input and fragment output contracts for the given entry points.
    bool BuildShaderInputAndOutputContracts(
        AZStd::string_view builderName,
        const ShaderReflectionData& reflectionData,
        const MapOfStringToStageType& shaderEntryPoints,
        const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout,
        RPI::ShaderInputContract& shaderInputContract,
        RPI::ShaderOutputContract& shaderOutputContract,
        size_t& colorAttachmentCount);

    //! Debug helper: writes an XML dump of the reflection data beside the other temp artifacts.
    //! Returns the path of the written file, or an empty string on failure.
    AZStd::string DumpReflectionData(
        AZStd::string_view builderName,
        const ShaderReflectionData& reflectionData,
        AZStd::string_view tempDirPath,
        AZStd::string_view stemName,
        AZStd::string_view apiName);
} // namespace AZ::ShaderBuilder
