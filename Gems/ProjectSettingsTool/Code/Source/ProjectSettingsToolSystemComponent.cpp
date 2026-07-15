/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ProjectSettingsToolIntegration.h>
#include <ProjectSettingsToolSystemComponent.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/Menu/MenuManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorContextIdentifiers.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorMenuIdentifiers.h>

namespace AZ::Editor::ProjectSettingsTool
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
        provided.push_back(AZ_CRC_CE("ProjectSettingsToolEditorService"));
    }

    void SystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ProjectSettingsToolEditorService"));
    }

    void SystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusConnect();
    }

    void SystemComponent::Deactivate()
    {
        Shutdown();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void SystemComponent::NotifyRegisterViews()
    {
        if (!m_integration)
        {
            m_integration = AZStd::make_unique<::ProjectSettingsToolIntegration>();
        }
    }

    void SystemComponent::NotifyEditorAboutToShutdown()
    {
        Shutdown();
    }

    void SystemComponent::OnActionRegistrationHook()
    {
        auto* actionManager = AZ::Interface<AzToolsFramework::ActionManagerInterface>::Get();
        if (!actionManager)
        {
            return;
        }

        constexpr AZStd::string_view actionIdentifier = "o3de.action.platform.editSettings";
        AzToolsFramework::ActionProperties actionProperties;
        actionProperties.m_name = "Edit Platform Settings...";
        actionProperties.m_description = "Open the Platform Settings panel.";
        actionProperties.m_category = "Platform";

        actionManager->RegisterAction(
            EditorIdentifiers::MainWindowActionContextIdentifier,
            actionIdentifier,
            actionProperties,
            []()
            {
                AzToolsFramework::OpenViewPane(::ProjectSettingsTool::ViewPaneName);
            });

        actionManager->AssignModeToAction(AzToolsFramework::DefaultActionContextModeIdentifier, actionIdentifier);
    }

    void SystemComponent::OnMenuBindingHook()
    {
        if (auto* menuManager = AZ::Interface<AzToolsFramework::MenuManagerInterface>::Get())
        {
            menuManager->AddActionToMenu(
                EditorIdentifiers::EditSettingsMenuIdentifier, "o3de.action.platform.editSettings", 400);
        }
    }

    void SystemComponent::Shutdown()
    {
        m_integration.reset();
    }
} // namespace AZ::Editor::ProjectSettingsTool
