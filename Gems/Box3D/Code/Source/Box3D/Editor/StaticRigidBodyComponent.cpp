/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/StaticRigidBodyComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <Box3D/StaticRigidBodyComponent.h>

namespace Box3D::Editor
{
    void StaticRigidBodyComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StaticRigidBodyComponent, AzToolsFramework::Components::EditorComponentBase>()->Version(2)->Field(
                "WorldName", &StaticRigidBodyComponent::m_worldName);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<StaticRigidBodyComponent>(
                        QT_TRANSLATE_NOOP("Box3D", "Box3D Static Rigid Body"),
                        QT_TRANSLATE_NOOP("Box3D", "Simulates the entity as a non-movable rigid body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &StaticRigidBodyComponent::m_worldName,
                        QT_TRANSLATE_NOOP("Box3D", "World"),
                        QT_TRANSLATE_NOOP("Box3D", "An empty name selects the default world."));
            }
        }
    }

    void StaticRigidBodyComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::StaticRigidBodyComponent::GetProvidedServices(provided);
    }

    void StaticRigidBodyComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::StaticRigidBodyComponent::GetIncompatibleServices(incompatible);
    }

    void StaticRigidBodyComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::StaticRigidBodyComponent::GetRequiredServices(required);
    }

    void StaticRigidBodyComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::StaticRigidBodyComponent>(m_worldName);
    }
} // namespace Box3D::Editor
