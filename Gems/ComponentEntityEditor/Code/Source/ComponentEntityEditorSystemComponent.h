/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ComponentEntityEditorIntegration.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace AZ::Editor::ComponentEntityEditor
{
    class SystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(SystemComponent, "{D8175008-525E-4914-8304-AF743512EA3C}");

        ~SystemComponent() override;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;
        void Deactivate() override;

    private:
        void NotifyRegisterViews() override;
        void NotifyEditorAboutToShutdown() override;
        void Shutdown();

        AZStd::unique_ptr<::ComponentEntityEditorIntegration> m_integration;
    };
} // namespace AZ::Editor::ComponentEntityEditor
