/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/CharacterControllerComponent.h>

#include <Jolt/CharacterControllerComponent.h>
#include <Jolt/Editor/CharacterDebug.h>
#include <Jolt/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Translation/TranslationDef.h>

namespace Jolt::Editor
{
    CharacterControllerComponent::CharacterControllerComponent(
        CharacterComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void CharacterControllerComponent::Reflect(
        AZ::ReflectContext* context)
    {
        Jolt::CharacterComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<CharacterControllerComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &CharacterControllerComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<CharacterControllerComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Character Controller"),
                        QT_TRANSLATE_NOOP("Jolt", "Simulates an entity as a rigid character controller."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &CharacterControllerComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Collision, movement, slope, and support properties."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void CharacterControllerComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::CharacterControllerComponent::GetProvidedServices(provided);
    }

    void CharacterControllerComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::CharacterControllerComponent::GetIncompatibleServices(incompatible);
    }

    void CharacterControllerComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::CharacterControllerComponent::GetRequiredServices(required);
    }

    void CharacterControllerComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void CharacterControllerComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void CharacterControllerComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::CharacterControllerComponent>(m_configuration);
    }

    void CharacterControllerComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        DrawCharacterGuides(
            debugDisplay,
            CharacterGuideConfiguration{
                .m_entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM()),
                .m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal,
                .m_up = m_configuration.m_up,
                .m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle,
                .m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance,
            });
    }

    bool CharacterControllerComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb CharacterControllerComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        return CalculateCharacterGuideBounds(
            CharacterGuideConfiguration{
                .m_entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM()),
                .m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal,
                .m_up = m_configuration.m_up,
                .m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle,
                .m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance,
            });
    }

    bool CharacterControllerComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo,
        const AZ::Vector3& rayStart,
        const AZ::Vector3& rayDirection,
        float& distance)
    {
        return IntersectEditorBounds(
            GetEditorSelectionBoundsViewport(viewportInfo),
            rayStart,
            rayDirection,
            distance);
    }
} // namespace Jolt::Editor
