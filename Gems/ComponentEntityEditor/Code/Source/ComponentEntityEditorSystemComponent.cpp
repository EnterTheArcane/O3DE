/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ComponentEntityEditorIntegration.h>
#include <ComponentEntityEditorSystemComponent.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Editor::ComponentEntityEditor
{
    SystemComponent::~SystemComponent() = default;

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SystemComponent, AZ::Component>()
                ->Version(0);
        }
    }

    void SystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("ComponentEntityEditorService"));
    }

    void SystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ComponentEntityEditorService"));
    }

    void SystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void SystemComponent::Deactivate()
    {
        Shutdown();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void SystemComponent::NotifyRegisterViews()
    {
        if (!m_integration)
        {
            m_integration = AZStd::make_unique<::ComponentEntityEditorIntegration>();
        }
    }

    void SystemComponent::NotifyEditorAboutToShutdown()
    {
        Shutdown();
    }

    void SystemComponent::Shutdown()
    {
        m_integration.reset();
    }
} // namespace AZ::Editor::ComponentEntityEditor
