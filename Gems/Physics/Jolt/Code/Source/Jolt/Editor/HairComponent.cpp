/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/HairComponent.h>

#include <Jolt/Editor/DebugDraw.h>
#include <Jolt/HairComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>

namespace Jolt::Editor
{
    void HairComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<HairComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &HairComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<HairComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Hair"),
                        QT_TRANSLATE_NOOP("Jolt", "Authors hair strands, materials, and scalp skinning."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Strands, scalp, materials, and fixed-step update policy."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void HairComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::HairComponent::GetProvidedServices(provided);
    }

    void HairComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::HairComponent::GetIncompatibleServices(incompatible);
    }

    void HairComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::HairComponent::GetRequiredServices(required);
    }

    void HairComponent::Init()
    {
        if (m_configuration.m_definition.m_vertices.empty())
        {
            m_configuration = HairComponentConfiguration::CreateDefault();
        }

        AzToolsFramework::Components::EditorComponentBase::Init();
    }

    void HairComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void HairComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void HairComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::HairComponent>(m_configuration);
    }

    void HairComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        const AZStd::vector<HairVertex>& vertices = m_configuration.m_definition.m_vertices;
        for (const HairStrand& strand : m_configuration.m_definition.m_strands)
        {
            if (strand.m_beginVertex >= strand.m_endVertex || strand.m_endVertex > vertices.size())
            {
                continue;
            }
            for (AZ::u32 vertexIndex = strand.m_beginVertex + 1; vertexIndex < strand.m_endVertex; ++vertexIndex)
            {
                debugDisplay.DrawLine(
                    transform.TransformPoint(vertices[vertexIndex - 1].m_position),
                    transform.TransformPoint(vertices[vertexIndex].m_position));
            }
        }
    }

    bool HairComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb HairComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        for (const HairVertex& vertex : m_configuration.m_definition.m_vertices)
        {
            bounds.AddPoint(transform.TransformPoint(vertex.m_position));
        }
        return bounds;
    }

    bool HairComponent::EditorSelectionIntersectRayViewport(
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
