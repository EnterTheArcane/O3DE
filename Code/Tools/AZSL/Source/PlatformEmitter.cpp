/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <cassert>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Emitter.h"
#include "Emitters/PlatformEmitter_DX12.h"
#include "Emitters/PlatformEmitter_Metal.h"
#include "Emitters/PlatformEmitter_Vulkan.h"
#include "PlatformEmitter.h"

namespace AZ::ShaderCompiler
{
    const PlatformEmitter* PlatformEmitter::GetDefaultEmitter() noexcept(true)
    {
        // The default platform emitter is never registered and will never be a result in PlatformEmitter::GetEmitter()
        static PlatformEmitter platformEmitter; // Static linkage, will be destroyed
        return &platformEmitter;
    }

    void RegisterPlatformEmitters()
    {
        (void)PlatformEmitter_DX12::RegisterPlatformEmitter();
        (void)PlatformEmitter_Metal::RegisterPlatformEmitter();
        (void)PlatformEmitter_Vulkan::RegisterPlatformEmitter();
    }

    const PlatformEmitter* PlatformEmitter::GetEmitter(const std::string& key) noexcept(true)
    {
        RegisterPlatformEmitters();
        if (key == "dx")
        {
            return PlatformEmitter_DX12::RegisterPlatformEmitter();
        }
        if (key == "mt")
        {
            return PlatformEmitter_Metal::RegisterPlatformEmitter();
        }
        if (key == "vk")
        {
            return PlatformEmitter_Vulkan::RegisterPlatformEmitter();
        }

        return nullptr;
    }

    // Virtual methods to be overridden

    std::string PlatformEmitter::GetRootSig(
        const CodeEmitter&,
        const RootSigDesc&,
        const Options&,
        BindingPair::Set) const
    {
        // The default implementation of most emission methods does nothing
        return "";
    }

    std::string PlatformEmitter::GetRootConstantsView(
        const CodeEmitter& codeEmitter,
        const RootSigDesc& rootSig,
        const Options&,
        const BindingPair::Set signatureQuery) const
    {
        std::stringstream strOut;

        const auto& structUid = codeEmitter.GetIR()->m_rootConstantStructUID;
        const auto& bindInfo = rootSig.Get(structUid);
        assert(structUid == bindInfo.m_uid);
        const auto& rootCBForEmission = codeEmitter.GetTranslatedName(RootConstantsViewName, UsageContext::DeclarationSite);
        const auto& rootConstClassForEmission = codeEmitter.GetTranslatedName(structUid.GetName(), UsageContext::ReferenceSite);
        const auto& spaceX = ", space" + std::to_string(bindInfo.m_registerBinding.m_pair[signatureQuery].m_logicalSpace);
        strOut << "ConstantBuffer<" << rootConstClassForEmission << "> " << rootCBForEmission << " : register(b" << bindInfo.m_registerBinding.m_pair[signatureQuery].m_registerIndex << spaceX << ");\n\n";

        return strOut.str();
    }

    std::pair<std::string, std::string> PlatformEmitter::GetDataViewHeaderFooter(
        const CodeEmitter&,
        const IdentifierUID&,
        uint32_t bindInfoRegisterIndex,
        std::string_view registerTypeLetter,
        const std::optional<std::string> stringifiedLogicalSpace,
        const Options&) const
    {
        // in the general case, we output normal HLSL `var decl : register(b0, space0);`
        // no special header, but the post colon part is the footer
        std::string bindingSpaceStringlet;
        if (stringifiedLogicalSpace)
        {
            bindingSpaceStringlet = ", space" + *stringifiedLogicalSpace;
        }
        std::string footer{ConcatString(" : register(", registerTypeLetter, bindInfoRegisterIndex, bindingSpaceStringlet, ")")};
        return {{}, footer};
    }

    uint32_t PlatformEmitter::AlignRootConstants(const uint32_t size) const
    {
        return size;
    }

    std::string PlatformEmitter::GetSpecializationConstant(
        const CodeEmitter&,
        const IdentifierUID&,
        const Options&) const
    {
        return "";
    }

    SubpassInputSupportFlag PlatformEmitter::GetSubpassInputSupport() const
    {
        return SubpassInputSupportFlag::None;
    }
}
