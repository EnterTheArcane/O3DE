/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Emitter.h>
#include "PlatformEmitter_Vulkan.h"

#include <sstream>
#include <string>

namespace AZ::ShaderCompiler
{
    static constexpr char PlatformEmitter_VulkanName[] = "vk";
    static const PlatformEmitter* s_platformEmitterVulkan = PlatformEmitter_Vulkan::RegisterPlatformEmitter();

    const PlatformEmitter* PlatformEmitter_Vulkan::RegisterPlatformEmitter() noexcept(false)
    {
        static PlatformEmitter_Vulkan platformEmitter; // Static linkage, will be destroyed

        static bool alreadyRegistered = false;
        if (!alreadyRegistered)
        {
            PlatformEmitter::SetEmitter(PlatformEmitter_VulkanName, &platformEmitter);
            alreadyRegistered = true;
        }

        return &platformEmitter;
    }

    std::string PlatformEmitter_Vulkan::GetRootConstantsView(
        const CodeEmitter& codeEmitter,
        const RootSigDesc&,
        const Options&,
        BindingPair::Set) const
    {
        std::stringstream strOut;

        const auto& structUid = codeEmitter.GetIR()->m_rootConstantStructUID;
        const auto& rootCBForEmission = codeEmitter.GetTranslatedName(RootConstantsViewName, UsageContext::DeclarationSite);
        strOut << "[[vk::push_constant]]\n";
        strOut << UnmangleTrimedName(structUid.GetName()) << " " << rootCBForEmission << ";\n\n";

        return strOut.str();
    }

    SubpassInputSupportFlag PlatformEmitter_Vulkan::GetSubpassInputSupport() const
    {
        return SubpassInputSupportFlag::All;
    }
}
