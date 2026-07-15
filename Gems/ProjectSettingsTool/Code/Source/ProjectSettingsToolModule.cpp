/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ProjectSettingsToolSystemComponent.h>

#include <AzCore/Module/Module.h>

namespace AZ::Editor::ProjectSettingsTool
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, "{D042DA05-0C25-43C2-A260-885C393ACF2D}", AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);

        Module()
        {
            m_descriptors.push_back(SystemComponent::CreateDescriptor());
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return { azrtti_typeid<SystemComponent>() };
        }
    };
} // namespace AZ::Editor::ProjectSettingsTool

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), AZ::Editor::ProjectSettingsTool::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_ProjectSettingsTool_Editor, AZ::Editor::ProjectSettingsTool::Module)
#endif
