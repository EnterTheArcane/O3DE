/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/RagdollComponent.h>

#include <Jolt/Editor/DebugDraw.h>
#include <Jolt/RagdollComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>

namespace Jolt::Editor
{
    RagdollComponent::RagdollComponent() = default;

    void RagdollComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<RagdollComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &RagdollComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<RagdollComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Ragdoll"),
                        QT_TRANSLATE_NOOP("Jolt", "Authors an articulated skeleton and its physical bodies."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Skeleton, bodies, shapes, and constraints."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void RagdollComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::RagdollComponent::GetProvidedServices(provided);
    }

    void RagdollComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::RagdollComponent::GetIncompatibleServices(incompatible);
    }

    void RagdollComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::RagdollComponent::GetRequiredServices(required);
    }

    void RagdollComponent::Init()
    {
        if (m_configuration.m_parts.empty())
        {
            m_configuration = RagdollComponentConfiguration::CreateDefault();
        }

        AzToolsFramework::Components::EditorComponentBase::Init();
    }

    void RagdollComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void RagdollComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void RagdollComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::RagdollComponent>(m_configuration);
    }

    void RagdollComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        const AZ::Matrix3x4 entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        for (size_t partIndex = 0; partIndex < m_configuration.m_parts.size(); ++partIndex)
        {
            const RagdollPartComponentConfiguration& part = m_configuration.m_parts[partIndex];
            const AZ::Matrix3x4 partTransform =
                entityTransform * AZ::Matrix3x4::CreateFromTransform(part.m_modelTransform);
            for (const ColliderShapeConfiguration& shape : part.m_shapes)
            {
                DrawShapeGeometry(
                    debugDisplay,
                    shape.m_shape.m_geometry,
                    partTransform * AZ::Matrix3x4::CreateFromTransform(shape.m_localTransform));
            }

            if (partIndex >= m_configuration.m_skeleton.m_joints.size())
            {
                continue;
            }
            const AZ::s32 parentIndex = m_configuration.m_skeleton.m_joints[partIndex].m_parentIndex;
            if (parentIndex < 0 || static_cast<size_t>(parentIndex) >= m_configuration.m_parts.size())
            {
                continue;
            }
            debugDisplay.DrawLine(
                partTransform.GetTranslation(),
                entityTransform.TransformPoint(m_configuration.m_parts[parentIndex].m_modelTransform.GetTranslation()));
        }
    }

    bool RagdollComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb RagdollComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        const AZ::Matrix3x4 entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        for (const RagdollPartComponentConfiguration& part : m_configuration.m_parts)
        {
            const AZ::Matrix3x4 partTransform =
                entityTransform * AZ::Matrix3x4::CreateFromTransform(part.m_modelTransform);
            for (const ColliderShapeConfiguration& shape : part.m_shapes)
            {
                bounds.AddAabb(CalculateShapeBounds(
                    shape.m_shape.m_geometry,
                    partTransform * AZ::Matrix3x4::CreateFromTransform(shape.m_localTransform)));
            }
        }
        return bounds;
    }

    bool RagdollComponent::EditorSelectionIntersectRayViewport(
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
