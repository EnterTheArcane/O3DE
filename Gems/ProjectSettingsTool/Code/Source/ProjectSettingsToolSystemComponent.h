/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ProjectSettingsToolIntegration.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>

namespace AZ::Editor::ProjectSettingsTool
{
    class SystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
        , private AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(SystemComponent, "{B07CB869-EF56-469D-95D7-09E35366CCAD}");

        ~SystemComponent() override;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;
        void Deactivate() override;

    private:
        void NotifyRegisterViews() override;
        void NotifyEditorAboutToShutdown() override;
        void OnActionRegistrationHook() override;
        void OnMenuBindingHook() override;
        void Shutdown();

        AZStd::unique_ptr<::ProjectSettingsToolIntegration> m_integration;
    };
} // namespace AZ::Editor::ProjectSettingsTool
