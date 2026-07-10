/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <cassert>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "Emitter.h"
#include "PlatformEmitter.h"

namespace AZ::ShaderCompiler
{
    const PlatformEmitter* PlatformEmitter::GetDefaultEmitter() noexcept(true)
    {
        // The default platform emitter is never registered and will never be a result in PlatformEmitter::GetEmitter()
        static PlatformEmitter platformEmitter; // Static linkage, will be destroyed
        return &platformEmitter;
    }

    std::unordered_map<std::string, const PlatformEmitter*>* s_emitters = nullptr;
    std::mutex emitterListMutex;

    const PlatformEmitter* PlatformEmitter::GetEmitter(const std::string& key) noexcept(true)
    {
        std::lock_guard<std::mutex> lock(emitterListMutex);

        try
        {
            return (*s_emitters)[key];
        }
        catch (...)
        {
            // We should never return a default emitter here. This method searches by name only
            return nullptr;
        }
    }

    void PlatformEmitter::SetEmitter(const std::string& key, const PlatformEmitter* const platformEmitter) noexcept(false)
    {
        std::lock_guard<std::mutex> lock(emitterListMutex);

        if (!s_emitters)
        {
            s_emitters = new std::unordered_map<std::string, const PlatformEmitter*>();
        }

        if (s_emitters->find(key) != s_emitters->end())
        {
            throw std::runtime_error{"PlatformEmitter::RegisterEmitter cannot register two platforms with the same key!"};
        }
        s_emitters->try_emplace(key, platformEmitter);
    }

    // Virtual methods to be overridden

    std::string PlatformEmitter::GetRootSig(const CodeEmitter&, const RootSigDesc&, const Options&, BindingPair::Set) const
    {
        // The default implementation of most emission methods does nothing
        return "";
    }

    std::string PlatformEmitter::GetRootConstantsView(const CodeEmitter& codeEmitter, const RootSigDesc& rootSig, const Options& options, const BindingPair::Set signatureQuery) const
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
        const CodeEmitter& codeEmitter,
        const IdentifierUID& symbol,
        uint32_t bindInfoRegisterIndex,
        std::string_view registerTypeLetter,
        const std::optional<std::string> stringifiedLogicalSpace,
        const Options& options) const
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

    std::string PlatformEmitter::GetSpecializationConstant(const CodeEmitter& codeEmitter, const IdentifierUID& symbol, const Options& options) const
    {
        return "";
    }

    SubpassInputSupportFlag PlatformEmitter::GetSubpassInputSupport() const
    {
        return SubpassInputSupportFlag::None;
    }
}
