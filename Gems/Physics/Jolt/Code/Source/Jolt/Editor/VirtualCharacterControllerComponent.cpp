/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/VirtualCharacterControllerComponent.h>

#include <Jolt/Editor/CharacterDebug.h>
#include <Jolt/Editor/DebugDraw.h>
#include <Jolt/VirtualCharacterControllerComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Translation/TranslationDef.h>

namespace Jolt::Editor
{
    VirtualCharacterControllerComponent::VirtualCharacterControllerComponent(
        VirtualCharacterComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void VirtualCharacterControllerComponent::Reflect(
        AZ::ReflectContext* context)
    {
        Jolt::VirtualCharacterComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<VirtualCharacterControllerComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &VirtualCharacterControllerComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<VirtualCharacterControllerComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Virtual Character Controller"),
                        QT_TRANSLATE_NOOP("Jolt", "Moves a query-based character without creating a rigid body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterControllerComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Collision, movement, stairs, slope, and support properties."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void VirtualCharacterControllerComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::VirtualCharacterControllerComponent::GetProvidedServices(provided);
    }

    void VirtualCharacterControllerComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::VirtualCharacterControllerComponent::GetIncompatibleServices(incompatible);
    }

    void VirtualCharacterControllerComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::VirtualCharacterControllerComponent::GetRequiredServices(required);
    }

    void VirtualCharacterControllerComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void VirtualCharacterControllerComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void VirtualCharacterControllerComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::VirtualCharacterControllerComponent>(m_configuration);
    }

    void VirtualCharacterControllerComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        DrawCharacterGuides(
            debugDisplay,
            CharacterGuideConfiguration{
                .m_entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM()),
                .m_shapeOffset = m_configuration.m_shapeOffset,
                .m_stairsStepDown = m_configuration.m_update.m_walkStairsStepDownExtra,
                .m_stairsStepUp = m_configuration.m_update.m_walkStairsStepUp,
                .m_stickToFloorStepDown = m_configuration.m_update.m_stickToFloorStepDown,
                .m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal,
                .m_up = m_configuration.m_up,
                .m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle,
                .m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance,
                .m_drawShapeOffset = true,
                .m_drawStepVectors = true,
            });
    }

    bool VirtualCharacterControllerComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb VirtualCharacterControllerComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        return CalculateCharacterGuideBounds(
            CharacterGuideConfiguration{
                .m_entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM()),
                .m_shapeOffset = m_configuration.m_shapeOffset,
                .m_stairsStepDown = m_configuration.m_update.m_walkStairsStepDownExtra,
                .m_stairsStepUp = m_configuration.m_update.m_walkStairsStepUp,
                .m_stickToFloorStepDown = m_configuration.m_update.m_stickToFloorStepDown,
                .m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal,
                .m_up = m_configuration.m_up,
                .m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle,
                .m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance,
                .m_drawShapeOffset = true,
                .m_drawStepVectors = true,
            });
    }

    bool VirtualCharacterControllerComponent::EditorSelectionIntersectRayViewport(
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
