/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzslcEmitter.h>
#include "VulkanPlatformEmitter.h"

namespace AZ::ShaderCompiler
{
    static constexpr char VulkanPlatformEmitterName[] = "vk";
    static const PlatformEmitter* s_vulkanPlatformEmitter = VulkanPlatformEmitter::RegisterPlatformEmitter();

    const PlatformEmitter* VulkanPlatformEmitter::RegisterPlatformEmitter() noexcept(false)
    {
        static VulkanPlatformEmitter platformEmitter; // Static linkage, will be destroyed

        static bool alreadyRegistered = false;
        if (!alreadyRegistered)
        {
            PlatformEmitter::SetEmitter(VulkanPlatformEmitterName, &platformEmitter);
            alreadyRegistered = true;
        }

        return &platformEmitter;
    }

    string VulkanPlatformEmitter::GetRootConstantsView(const CodeEmitter& codeEmitter, const RootSigDesc&, const Options&, BindingPair::Set) const
    {
        std::stringstream strOut;

        const auto& structUid = codeEmitter.GetIR()->m_rootConstantStructUID;
        const auto& rootCBForEmission = codeEmitter.GetTranslatedName(RootConstantsViewName, UsageContext::DeclarationSite);
        strOut << "[[vk::push_constant]]\n";
        strOut << UnmangleTrimedName(structUid.GetName()) << " " << rootCBForEmission << ";\n\n";

        return strOut.str();        
    }

    SubpassInputSupportFlag VulkanPlatformEmitter::GetSubpassInputSupport() const
    {
        return SubpassInputSupportFlag::All;
    }

    std::pair<string, string> VulkanPlatformEmitter::GetDataViewHeaderFooter(
        const CodeEmitter& codeEmitter,
        const IdentifierUID& symbol,
        uint32_t bindInfoRegisterIndex,
        string_view registerTypeLetter,
        optional<string> stringifiedLogicalSpace,
        const Options& options) const
    {
        auto headerFooter = CommonVulkanPlatformEmitter::GetDataViewHeaderFooter(codeEmitter, symbol, bindInfoRegisterIndex, registerTypeLetter, stringifiedLogicalSpace, options);
        const auto* ir = codeEmitter.GetIR();
        const auto* varInfo = ir->GetSymbolSubAs<VarInfo>(symbol.GetName());
        const auto& typeName = varInfo->m_typeInfoExt.m_coreType.m_typeId.GetName();
        const auto scalarType = varInfo->m_typeInfoExt.m_genericParameter.m_arithmeticInfo.UnderlyingScalarToStr();
        const bool hasFloatingPointElements = scalarType == "half" || scalarType == "float" || scalarType == "double";

        // Integer image atomics require a declared SPIR-V format.
        if (hasFloatingPointElements
            && (StartsWith(typeName, "?RWTexture")
            || StartsWith(typeName, "?RasterizerOrderedTexture")
            || typeName == "?RWBuffer"
            || typeName == "?RasterizerOrderedBuffer")
            && !ir->m_symbols.GetAttribute(symbol, "image_format"))
        {
            // Floating point elements can use normalized or smaller image formats.
            headerFooter.first += "[[vk::image_format(\"unknown\")]]\n";
        }
        return headerFooter;
    }
}
