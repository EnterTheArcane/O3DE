/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/CharacterControllerComponent.h>

#include <Box3D/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>
#include <AzToolsFramework/ComponentModes/CapsuleComponentMode.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <Box3D/CharacterControllerComponent.h>

namespace Box3D::Editor
{
    void CharacterControllerComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CharacterControllerComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(3)
                ->Field("Configuration", &CharacterControllerComponent::m_configuration)
                ->Field("WorldName", &CharacterControllerComponent::m_worldName)
                ->Field("ComponentMode", &CharacterControllerComponent::m_componentModeDelegate);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<CharacterControllerComponent>(
                        QT_TRANSLATE_NOOP("Box3D", "Box3D Character Controller"),
                        QT_TRANSLATE_NOOP("Box3D", "Moves a capsule through the physics scene with collision-aware sliding."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &CharacterControllerComponent::m_worldName,
                        QT_TRANSLATE_NOOP("Box3D", "World"),
                        QT_TRANSLATE_NOOP("Box3D", "An empty name selects the default world."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &CharacterControllerComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Box3D", "Movement"),
                        QT_TRANSLATE_NOOP("Box3D", "Filtering, speed, slope, step, and update settings."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &CharacterControllerComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &CharacterControllerComponent::m_componentModeDelegate,
                        QT_TRANSLATE_NOOP("Box3D", "Component mode"),
                        QT_TRANSLATE_NOOP("Box3D", "Edit capsule height and radius in the viewport."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void CharacterControllerComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::CharacterControllerComponent::GetProvidedServices(provided);
    }

    void CharacterControllerComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::CharacterControllerComponent::GetIncompatibleServices(incompatible);
    }

    void CharacterControllerComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::CharacterControllerComponent::GetRequiredServices(required);
    }

    void CharacterControllerComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
        const AZ::EntityComponentIdPair entityComponentIdPair(GetEntityId(), GetId());
        AzToolsFramework::CapsuleManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        const bool allowAsymmetricalEditing = false;
        m_componentModeDelegate.ConnectWithSingleComponentMode<CharacterControllerComponent, AzToolsFramework::CapsuleComponentMode>(
            entityComponentIdPair, this, allowAsymmetricalEditing);
    }

    void CharacterControllerComponent::Deactivate()
    {
        m_componentModeDelegate.Disconnect();
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::CapsuleManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void CharacterControllerComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::CharacterControllerComponent>(m_configuration, m_worldName);
    }

    void CharacterControllerComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        const AZ::Vector3 up =
            m_configuration.m_upDirection.IsZero() ? AZ::Vector3::CreateAxisZ() : m_configuration.m_upDirection.GetNormalizedSafe();
        const float uniformScale = AZStd::abs(GetWorldTM().GetUniformScale());
        const AZ::Vector3 center = GetWorldTM().GetTranslation() + 0.5f * uniformScale * m_configuration.m_height * up;
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        debugDisplay.DrawWireCapsule(
            center,
            up,
            uniformScale * m_configuration.m_radius,
            uniformScale * AZStd::max(m_configuration.m_height - 2.0f * m_configuration.m_radius, 0.0f));
    }

    bool CharacterControllerComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb CharacterControllerComponent::GetEditorSelectionBoundsViewport([[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        const AZ::Vector3 up =
            m_configuration.m_upDirection.IsZero() ? AZ::Vector3::CreateAxisZ() : m_configuration.m_upDirection.GetNormalizedSafe();
        const float uniformScale = AZStd::abs(GetWorldTM().GetUniformScale());
        const float radius = uniformScale * m_configuration.m_radius;
        const float halfSegment = uniformScale * AZStd::max(0.5f * m_configuration.m_height - m_configuration.m_radius, 0.0f);
        const AZ::Vector3 center = GetWorldTM().GetTranslation() + 0.5f * uniformScale * m_configuration.m_height * up;
        const AZ::Vector3 extent = AZ::Vector3(radius) + halfSegment * up.GetAbs();
        return AZ::Aabb::CreateCenterHalfExtents(center, extent);
    }

    bool CharacterControllerComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo, const AZ::Vector3& rayStart, const AZ::Vector3& rayDirection, float& distance)
    {
        return IntersectEditorBounds(GetEditorSelectionBoundsViewport(viewportInfo), rayStart, rayDirection, distance);
    }

    float CharacterControllerComponent::GetRadius() const
    {
        return m_configuration.m_radius;
    }

    void CharacterControllerComponent::SetRadius(const float radius)
    {
        if (!AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return;
        }
        m_configuration.m_radius = radius;
        m_configuration.m_height = AZStd::max(m_configuration.m_height, 2.0f * radius);
        ConfigurationChanged();
    }

    float CharacterControllerComponent::GetHeight() const
    {
        return m_configuration.m_height;
    }

    void CharacterControllerComponent::SetHeight(const float height)
    {
        if (!AZ::IsFiniteFloat(height) || height < 2.0f * m_configuration.m_radius)
        {
            return;
        }
        m_configuration.m_height = height;
        ConfigurationChanged();
    }

    AZ::Vector3 CharacterControllerComponent::GetTranslationOffset() const
    {
        return AZ::Vector3::CreateZero();
    }

    void CharacterControllerComponent::SetTranslationOffset([[maybe_unused]] const AZ::Vector3& translationOffset)
    {
    }

    AZ::Transform CharacterControllerComponent::GetManipulatorSpace() const
    {
        return GetWorldTM();
    }

    AZ::Quaternion CharacterControllerComponent::GetRotationOffset() const
    {
        const AZ::Vector3 up =
            m_configuration.m_upDirection.IsZero() ? AZ::Vector3::CreateAxisZ() : m_configuration.m_upDirection.GetNormalizedSafe();
        return AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisZ(), up);
    }

    AZ::u32 CharacterControllerComponent::ConfigurationChanged()
    {
        SetDirty();
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::Refresh,
            AZ::EntityComponentIdPair(GetEntityId(), GetId()));
        return AzToolsFramework::Refresh_Values;
    }
} // namespace Box3D::Editor
