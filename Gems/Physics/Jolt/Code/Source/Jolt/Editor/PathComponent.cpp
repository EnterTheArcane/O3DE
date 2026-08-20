/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/PathComponent.h>

#include <Jolt/Editor/DebugDraw.h>
#include <Jolt/PathComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>
#include <AzCore/std/utility/move.h>

namespace Jolt::Editor
{
    namespace
    {
        constexpr size_t PathSegmentSampleCount = 16;

        AZ::Vector3 EvaluateHermiteSegment(
            const HermitePathPoint& start,
            const HermitePathPoint& end,
            const float fraction)
        {
            const float fractionSquared = fraction * fraction;
            const float fractionCubed = fractionSquared * fraction;
            const float startPositionWeight = 2.0f * fractionCubed - 3.0f * fractionSquared + 1.0f;
            const float startTangentWeight = fractionCubed - 2.0f * fractionSquared + fraction;
            const float endPositionWeight = -2.0f * fractionCubed + 3.0f * fractionSquared;
            const float endTangentWeight = fractionCubed - fractionSquared;
            return startPositionWeight * start.m_position
                + startTangentWeight * start.m_tangent
                + endPositionWeight * end.m_position
                + endTangentWeight * end.m_tangent;
        }

        template<typename Visitor>
        void VisitPathSamples(
            const HermitePathConfiguration& configuration,
            Visitor&& visitor)
        {
            if (!configuration.IsValid())
            {
                return;
            }

            size_t segmentCount = configuration.m_points.size() - 1;
            if (configuration.m_isLooping)
            {
                ++segmentCount;
            }
            for (size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
            {
                const HermitePathPoint& start = configuration.m_points[segmentIndex];
                const HermitePathPoint& end =
                    configuration.m_points[(segmentIndex + 1) % configuration.m_points.size()];
                AZ::Vector3 previous = start.m_position;
                for (size_t sampleIndex = 1; sampleIndex <= PathSegmentSampleCount; ++sampleIndex)
                {
                    const float fraction = static_cast<float>(sampleIndex) / PathSegmentSampleCount;
                    const AZ::Vector3 current = EvaluateHermiteSegment(start, end, fraction);
                    visitor(previous, current);
                    previous = current;
                }
            }
        }
    } // namespace

    PathComponent::PathComponent() = default;

    PathComponent::PathComponent(
        HermitePathConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void PathComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            HermitePathConfiguration::Reflect(serializeContext);

            serializeContext
                ->Class<PathComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &PathComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<PathComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Path"),
                        QT_TRANSLATE_NOOP("Jolt", "Defines a Hermite path for constraints."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &PathComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Path"),
                        QT_TRANSLATE_NOOP("Jolt", "Ordered Hermite control points and loop state."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void PathComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::PathComponent::GetProvidedServices(provided);
    }

    void PathComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::PathComponent::GetIncompatibleServices(incompatible);
    }

    void PathComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void PathComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void PathComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void PathComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::PathComponent>(m_configuration);
    }

    void PathComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        VisitPathSamples(
            m_configuration,
            [&](const AZ::Vector3& start, const AZ::Vector3& end)
            {
                debugDisplay.DrawLine(transform.TransformPoint(start), transform.TransformPoint(end));
            });
    }

    bool PathComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb PathComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        VisitPathSamples(
            m_configuration,
            [&](const AZ::Vector3& start, const AZ::Vector3& end)
            {
                bounds.AddPoint(transform.TransformPoint(start));
                bounds.AddPoint(transform.TransformPoint(end));
            });
        return bounds;
    }

    bool PathComponent::EditorSelectionIntersectRayViewport(
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
