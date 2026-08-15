/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/ConstraintComponent.h>

#include <Jolt/ConstraintComponent.h>
#include <Jolt/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>

namespace Jolt::Editor
{
    namespace
    {
        struct VisualizationLine final
        {
            AZ::Vector3 m_start = AZ::Vector3::CreateZero();
            AZ::Vector3 m_end = AZ::Vector3::CreateZero();
        };

        struct ConstraintVisualization final
        {
            void AddLine(
                const AZ::Vector3& start,
                const AZ::Vector3& end)
            {
                AZ_Assert(
                    m_lineCount < m_lines.size(),
                    "The Jolt constraint visualization line capacity is too small.");
                if (m_lineCount >= m_lines.size())
                {
                    return;
                }

                m_lines[m_lineCount] = {
                    .m_start = start,
                    .m_end = end,
                };
                ++m_lineCount;
            }

            AZStd::array<VisualizationLine, 3> m_lines;
            AZ::Vector3 m_firstAxisStart = AZ::Vector3::CreateZero();
            AZ::Vector3 m_firstAxisEnd = AZ::Vector3::CreateZero();
            AZ::Vector3 m_secondAxisStart = AZ::Vector3::CreateZero();
            AZ::Vector3 m_secondAxisEnd = AZ::Vector3::CreateZero();
            AZ::u32 m_lineCount = 0;
        };

        [[nodiscard]]
        AZ::Transform GetEntityWorldTransform(const AZ::EntityId entityId)
        {
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            if (entityId.IsValid())
            {
                AZ::TransformBus::EventResult(
                    transform,
                    entityId,
                    &AZ::TransformInterface::GetWorldTM);
            }
            return transform;
        }

        [[nodiscard]]
        AZ::Vector3 ToEditorPoint(
            const WorldPosition& point,
            const ConstraintSpace space,
            const AZ::Transform& bodyTransform)
        {
            const AZ::Vector3 localPoint(
                static_cast<float>(point.m_x),
                static_cast<float>(point.m_y),
                static_cast<float>(point.m_z));
            if (space == ConstraintSpace::World)
            {
                return localPoint;
            }
            return bodyTransform.TransformPoint(localPoint);
        }

        [[nodiscard]]
        AZ::Vector3 ToEditorAxis(
            const AZ::Vector3& axis,
            const ConstraintSpace space,
            const AZ::Transform& bodyTransform)
        {
            AZ::Vector3 worldAxis = axis;
            if (space != ConstraintSpace::World)
            {
                worldAxis = bodyTransform.TransformVector(axis);
            }
            if (!worldAxis.IsZero())
            {
                worldAxis.Normalize();
            }
            return worldAxis;
        }

        [[nodiscard]]
        ConstraintVisualization BuildConstraintVisualization(
            const ConstraintComponentConfiguration& configuration,
            const float drawScale)
        {
            ConstraintVisualization visualization;
            const AZ::Transform firstBodyTransform = GetEntityWorldTransform(configuration.m_firstBodyEntityId);
            const AZ::Transform secondBodyTransform = GetEntityWorldTransform(configuration.m_secondBodyEntityId);
            const AZ::Vector3 firstBodyPosition = firstBodyTransform.GetTranslation();
            const AZ::Vector3 secondBodyPosition = secondBodyTransform.GetTranslation();

            AZStd::visit(
                [&](const auto& geometry)
                {
                    using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                    if constexpr (AZStd::is_same_v<Geometry, PulleyConstraintConfiguration>)
                    {
                        const AZ::Vector3 firstBodyPoint = ToEditorPoint(
                            geometry.m_firstBodyPoint,
                            geometry.m_space,
                            firstBodyTransform);
                        const AZ::Vector3 firstFixedPoint = ToEditorPoint(
                            geometry.m_firstFixedPoint,
                            geometry.m_space,
                            firstBodyTransform);
                        const AZ::Vector3 secondBodyPoint = ToEditorPoint(
                            geometry.m_secondBodyPoint,
                            geometry.m_space,
                            secondBodyTransform);
                        const AZ::Vector3 secondFixedPoint = ToEditorPoint(
                            geometry.m_secondFixedPoint,
                            geometry.m_space,
                            secondBodyTransform);
                        visualization.AddLine(firstBodyPoint, firstFixedPoint);
                        visualization.AddLine(firstFixedPoint, secondFixedPoint);
                        visualization.AddLine(secondFixedPoint, secondBodyPoint);
                        visualization.m_firstAxisStart = firstBodyPoint;
                        visualization.m_firstAxisEnd = firstBodyPoint;
                        visualization.m_secondAxisStart = secondBodyPoint;
                        visualization.m_secondAxisEnd = secondBodyPoint;
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, PathConstraintComponentConfiguration>)
                    {
                        const AZ::Vector3 pathPosition =
                            GetEntityWorldTransform(geometry.m_pathEntityId).TransformPoint(geometry.m_pathPosition);
                        visualization.AddLine(firstBodyPosition, pathPosition);
                        visualization.AddLine(pathPosition, secondBodyPosition);
                        visualization.m_firstAxisStart = pathPosition;
                        visualization.m_firstAxisEnd = pathPosition
                            + drawScale * geometry.m_pathRotation.TransformVector(AZ::Vector3::CreateAxisX());
                        visualization.m_secondAxisStart = secondBodyPosition;
                        visualization.m_secondAxisEnd = secondBodyPosition;
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, GearConstraintComponentConfiguration>)
                    {
                        visualization.AddLine(firstBodyPosition, secondBodyPosition);
                        visualization.m_firstAxisStart = firstBodyPosition;
                        visualization.m_firstAxisEnd = firstBodyPosition
                            + drawScale * ToEditorAxis(
                                geometry.m_firstHingeAxis,
                                geometry.m_space,
                                firstBodyTransform);
                        visualization.m_secondAxisStart = secondBodyPosition;
                        visualization.m_secondAxisEnd = secondBodyPosition
                            + drawScale * ToEditorAxis(
                                geometry.m_secondHingeAxis,
                                geometry.m_space,
                                secondBodyTransform);
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintComponentConfiguration>)
                    {
                        visualization.AddLine(firstBodyPosition, secondBodyPosition);
                        visualization.m_firstAxisStart = firstBodyPosition;
                        visualization.m_firstAxisEnd = firstBodyPosition
                            + drawScale * ToEditorAxis(
                                geometry.m_hingeAxis,
                                geometry.m_space,
                                firstBodyTransform);
                        visualization.m_secondAxisStart = secondBodyPosition;
                        visualization.m_secondAxisEnd = secondBodyPosition
                            + drawScale * ToEditorAxis(
                                geometry.m_sliderAxis,
                                geometry.m_space,
                                secondBodyTransform);
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, CustomConstraintConfiguration>)
                    {
                        const AZ::Vector3 firstPoint = ToEditorPoint(
                            geometry.m_firstFrame.m_position,
                            geometry.m_space,
                            firstBodyTransform);
                        const AZ::Vector3 secondPoint = ToEditorPoint(
                            geometry.m_secondFrame.m_position,
                            geometry.m_space,
                            secondBodyTransform);
                        visualization.AddLine(firstPoint, secondPoint);
                        visualization.m_firstAxisStart = firstPoint;
                        visualization.m_firstAxisEnd = firstPoint
                            + drawScale * ToEditorAxis(
                                geometry.m_firstFrame.m_rotation.TransformVector(AZ::Vector3::CreateAxisX()),
                                geometry.m_space,
                                firstBodyTransform);
                        visualization.m_secondAxisStart = secondPoint;
                        visualization.m_secondAxisEnd = secondPoint
                            + drawScale * ToEditorAxis(
                                geometry.m_secondFrame.m_rotation.TransformVector(AZ::Vector3::CreateAxisX()),
                                geometry.m_space,
                                secondBodyTransform);
                    }
                    else
                    {
                        const AZ::Vector3 firstPoint = ToEditorPoint(
                            geometry.m_firstPoint,
                            geometry.m_space,
                            firstBodyTransform);
                        const AZ::Vector3 secondPoint = ToEditorPoint(
                            geometry.m_secondPoint,
                            geometry.m_space,
                            secondBodyTransform);
                        visualization.AddLine(firstPoint, secondPoint);

                        AZ::Vector3 firstAxis = AZ::Vector3::CreateZero();
                        AZ::Vector3 secondAxis = AZ::Vector3::CreateZero();
                        if constexpr (AZStd::is_same_v<Geometry, ConeConstraintConfiguration>)
                        {
                            firstAxis = geometry.m_firstTwistAxis;
                            secondAxis = geometry.m_secondTwistAxis;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, FixedConstraintConfiguration>)
                        {
                            firstAxis = geometry.m_firstAxisX;
                            secondAxis = geometry.m_secondAxisX;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, HingeConstraintConfiguration>)
                        {
                            firstAxis = geometry.m_firstHingeAxis;
                            secondAxis = geometry.m_secondHingeAxis;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, SixDofConstraintConfiguration>)
                        {
                            firstAxis = geometry.m_firstAxisX;
                            secondAxis = geometry.m_secondAxisX;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, SliderConstraintConfiguration>)
                        {
                            firstAxis = geometry.m_firstSliderAxis;
                            secondAxis = geometry.m_secondSliderAxis;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, SwingTwistConstraintConfiguration>)
                        {
                            firstAxis = geometry.m_firstTwistAxis;
                            secondAxis = geometry.m_secondTwistAxis;
                        }

                        visualization.m_firstAxisStart = firstPoint;
                        visualization.m_firstAxisEnd = firstPoint
                            + drawScale * ToEditorAxis(firstAxis, geometry.m_space, firstBodyTransform);
                        visualization.m_secondAxisStart = secondPoint;
                        visualization.m_secondAxisEnd = secondPoint
                            + drawScale * ToEditorAxis(secondAxis, geometry.m_space, secondBodyTransform);
                    }
                },
                configuration.m_geometry);
            return visualization;
        }

        [[nodiscard]]
        AZ::Aabb CalculateVisualizationBounds(const ConstraintVisualization& visualization)
        {
            AZ::Aabb bounds = AZ::Aabb::CreateNull();
            for (AZ::u32 lineIndex = 0; lineIndex < visualization.m_lineCount; ++lineIndex)
            {
                bounds.AddPoint(visualization.m_lines[lineIndex].m_start);
                bounds.AddPoint(visualization.m_lines[lineIndex].m_end);
            }
            bounds.AddPoint(visualization.m_firstAxisStart);
            bounds.AddPoint(visualization.m_firstAxisEnd);
            bounds.AddPoint(visualization.m_secondAxisStart);
            bounds.AddPoint(visualization.m_secondAxisEnd);
            bounds.Expand(AZ::Vector3(0.1f));
            return bounds;
        }
    } // namespace

    ConstraintComponent::ConstraintComponent(
        ConstraintComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void ConstraintComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ConstraintComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &ConstraintComponent::m_configuration)
                ->Field("DrawScale", &ConstraintComponent::m_drawScale);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ConstraintComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Constraint"),
                        QT_TRANSLATE_NOOP("Jolt", "Constrains two Jolt body entities."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ConstraintComponent::m_drawScale,
                        QT_TRANSLATE_NOOP("Jolt", "Draw scale"),
                        QT_TRANSLATE_NOOP("Jolt", "Length of the constraint frame axes in the viewport."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ConstraintComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Bodies, geometry, motors, limits, and solver overrides."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void ConstraintComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::ConstraintComponent::GetProvidedServices(provided);
    }

    void ConstraintComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::ConstraintComponent::GetIncompatibleServices(incompatible);
    }

    void ConstraintComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void ConstraintComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void ConstraintComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::ConstraintComponent>(m_configuration);
    }

    void ConstraintComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        const ConstraintVisualization visualization = BuildConstraintVisualization(m_configuration, m_drawScale);
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        for (AZ::u32 lineIndex = 0; lineIndex < visualization.m_lineCount; ++lineIndex)
        {
            debugDisplay.DrawLine(
                visualization.m_lines[lineIndex].m_start,
                visualization.m_lines[lineIndex].m_end);
        }

        debugDisplay.SetColor(AZ::Colors::Red);
        debugDisplay.DrawLine(visualization.m_firstAxisStart, visualization.m_firstAxisEnd);
        debugDisplay.SetColor(AZ::Colors::Green);
        debugDisplay.DrawLine(visualization.m_secondAxisStart, visualization.m_secondAxisEnd);
    }

    bool ConstraintComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb ConstraintComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        return CalculateVisualizationBounds(BuildConstraintVisualization(m_configuration, m_drawScale));
    }

    bool ConstraintComponent::EditorSelectionIntersectRayViewport(
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
