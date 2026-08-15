/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/StaticRigidBodyComponent.h>

#include <Jolt/StaticRigidBodyComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>

namespace Jolt::Editor
{
    void StaticRigidBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<StaticRigidBodyComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &StaticRigidBodyComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<StaticRigidBodyComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Static Rigid Body"),
                        QT_TRANSLATE_NOOP("Jolt", "Simulates the entity as a static rigid body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &StaticRigidBodyComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Collision and contact properties."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void StaticRigidBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::StaticRigidBodyComponent::GetProvidedServices(provided);
    }

    void StaticRigidBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::StaticRigidBodyComponent::GetIncompatibleServices(incompatible);
    }

    void StaticRigidBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::StaticRigidBodyComponent::GetRequiredServices(required);
    }

    void StaticRigidBodyComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::StaticRigidBodyComponent>(m_configuration);
    }
} // namespace Jolt::Editor
