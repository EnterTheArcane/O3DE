/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ComponentEntityEditorSystemComponent.h>

#include <AzCore/Module/Module.h>

namespace AZ::Editor::ComponentEntityEditor
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, "{E2BE3B5B-620D-4C91-A0BD-65C5DD2B3308}", AZ::Module);
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
} // namespace AZ::Editor::ComponentEntityEditor

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), AZ::Editor::ComponentEntityEditor::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_ComponentEntityEditor_Editor, AZ::Editor::ComponentEntityEditor::Module)
#endif
