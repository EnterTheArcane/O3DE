/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/RigidBodyComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <Box3D/RigidBodyComponent.h>

namespace Box3D::Editor
{
    void RigidBodyComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<RigidBodyComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(2)
                ->Field("Configuration", &RigidBodyComponent::m_configuration)
                ->Field("WorldName", &RigidBodyComponent::m_worldName);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<RigidBodyComponent>(
                        QT_TRANSLATE_NOOP("Box3D", "Box3D Rigid Body"),
                        QT_TRANSLATE_NOOP("Box3D", "Simulates the entity as a movable rigid body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RigidBodyComponent::m_worldName,
                        QT_TRANSLATE_NOOP("Box3D", "World"),
                        QT_TRANSLATE_NOOP("Box3D", "An empty name selects the default world."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RigidBodyComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Box3D", "Configuration"),
                        QT_TRANSLATE_NOOP("Box3D", "Mass, damping, gravity, sleeping, and initial state."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void RigidBodyComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::RigidBodyComponent::GetProvidedServices(provided);
    }

    void RigidBodyComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::RigidBodyComponent::GetIncompatibleServices(incompatible);
    }

    void RigidBodyComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::RigidBodyComponent::GetRequiredServices(required);
    }

    void RigidBodyComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::RigidBodyComponent>(m_configuration, m_worldName);
    }
} // namespace Box3D::Editor
