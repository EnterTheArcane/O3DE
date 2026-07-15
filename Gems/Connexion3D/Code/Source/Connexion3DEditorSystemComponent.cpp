/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Connexion3DEditorSystemComponent.h>
#include <Platform/DeviceDriverFactory.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Editor::Connexion3D
{
    void EditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorSystemComponent, AZ::Component>()
                ->Version(0);
        }
    }

    void EditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Connexion3DEditorService"));
    }

    void EditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Connexion3DEditorService"));
    }

    void EditorSystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void EditorSystemComponent::Deactivate()
    {
        Shutdown();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void EditorSystemComponent::NotifyRegisterViews()
    {
        if (!m_driver)
        {
            m_driver = CreateDeviceDriver();
            if (m_driver)
            {
                AZ::Interface<IDeviceDriver>::Register(m_driver.get());
            }
        }
    }

    void EditorSystemComponent::Shutdown()
    {
        if (m_driver)
        {
            AZ::Interface<IDeviceDriver>::Unregister(m_driver.get());
            m_driver.reset();
        }
    }
} // namespace AZ::Editor::Connexion3D
