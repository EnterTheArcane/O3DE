/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/HeightfieldColliderComponent.h>

#include <Box3D/Editor/DebugDraw.h>
#include <Box3D/HeightfieldColliderComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>

namespace Box3D::Editor
{
    void HeightfieldColliderComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<HeightfieldColliderComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &HeightfieldColliderComponent::m_configuration)
                ->Field("WorldName", &HeightfieldColliderComponent::m_worldName);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<HeightfieldColliderComponent>(QT_TRANSLATE_NOOP("Box3D", "Box3D Heightfield Collider"), QT_TRANSLATE_NOOP("Box3D", "Creates mutable static terrain collision from height samples."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldColliderComponent::m_worldName, QT_TRANSLATE_NOOP("Box3D", "World"), QT_TRANSLATE_NOOP("Box3D", "An empty name selects the default world."))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldColliderComponent::m_configuration, QT_TRANSLATE_NOOP("Box3D", "Heightfield"), QT_TRANSLATE_NOOP("Box3D", "Samples, materials, filtering, and event policy."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void HeightfieldColliderComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::HeightfieldColliderComponent::GetProvidedServices(provided);
    }

    void HeightfieldColliderComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::HeightfieldColliderComponent::GetRequiredServices(required);
    }

    void HeightfieldColliderComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::HeightfieldColliderComponent::GetIncompatibleServices(incompatible);
    }

    void HeightfieldColliderComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void HeightfieldColliderComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void HeightfieldColliderComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::HeightfieldColliderComponent>(m_configuration, m_worldName);
    }

    void HeightfieldColliderComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM())
            * AZ::Matrix3x4::CreateFromTransform(m_configuration.m_properties.m_localTransform);
        DrawShapeGeometry(debugDisplay, m_configuration.m_geometry, transform);
    }

    bool HeightfieldColliderComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb HeightfieldColliderComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM())
            * AZ::Matrix3x4::CreateFromTransform(m_configuration.m_properties.m_localTransform);
        return CalculateShapeBounds(m_configuration.m_geometry, transform);
    }

    bool HeightfieldColliderComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo,
        const AZ::Vector3& rayStart,
        const AZ::Vector3& rayDirection,
        float& distance)
    {
        return IntersectEditorBounds(GetEditorSelectionBoundsViewport(viewportInfo), rayStart, rayDirection, distance);
    }
} // namespace Box3D::Editor
