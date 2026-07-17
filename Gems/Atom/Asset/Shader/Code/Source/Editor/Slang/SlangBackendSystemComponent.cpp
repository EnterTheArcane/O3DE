/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangBackendSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>

#include <Editor/ShaderCompilerBackendBus.h>

namespace AZ::ShaderBuilder
{
    void SlangBackendSystemComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serialize = azrtti_cast<SerializeContext*>(context))
        {
            serialize->Class<SlangBackendSystemComponent, Component>()
                ->Version(0)
                ->Attribute(Edit::Attributes::SystemComponentTags, AZStd::vector<Crc32>({AssetBuilderSDK::ComponentTags::AssetBuilder}))
                ;
        }
    }

    void SlangBackendSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("SlangShaderCompilerBackendService"));
    }

    void SlangBackendSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("SlangShaderCompilerBackendService"));
    }

    void SlangBackendSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The generic ShaderBuilderSystemComponent handles ShaderCompilerBackendBus; it must be
        // active before backends register.
        required.push_back(AZ_CRC_CE("ShaderBuilderService"));
    }

    void SlangBackendSystemComponent::Activate()
    {
        ShaderCompilerBackendBus::Broadcast(&ShaderCompilerBackendBus::Events::RegisterShaderCompilerBackend, &m_backend);
    }

    void SlangBackendSystemComponent::Deactivate()
    {
        ShaderCompilerBackendBus::Broadcast(&ShaderCompilerBackendBus::Events::UnregisterShaderCompilerBackend, &m_backend);
    }
} // namespace AZ::ShaderBuilder
