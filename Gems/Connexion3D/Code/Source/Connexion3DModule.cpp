/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Connexion3DEditorSystemComponent.h>

#include <AzCore/Module/Module.h>

namespace AZ::Editor::Connexion3D
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, "{D39A47C3-A92D-4E81-94EB-31477C86D06E}", AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);

        Module()
        {
            m_descriptors.push_back(EditorSystemComponent::CreateDescriptor());
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return { azrtti_typeid<EditorSystemComponent>() };
        }
    };
} // namespace AZ::Editor::Connexion3D

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), AZ::Editor::Connexion3D::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Connexion3D_Editor, AZ::Editor::Connexion3D::Module)
#endif
