/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/ColliderComponent.h>

#include <Jolt/ColliderComponent.h>
#include <Jolt/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>
#include <AzToolsFramework/ComponentModes/BoxComponentMode.h>
#include <AzToolsFramework/ComponentModes/CapsuleComponentMode.h>
#include <AzToolsFramework/ComponentModes/CylinderComponentMode.h>
#include <AzToolsFramework/ComponentModes/ShapeTranslationOffsetViewportEdit.h>
#include <AzToolsFramework/ComponentModes/SphereComponentMode.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>

namespace Jolt::Editor
{
    namespace ComponentModes
    {
        class OffsetComponentMode final
            : public AzToolsFramework::BaseShapeComponentMode
        {
        public:
            AZ_CLASS_ALLOCATOR(OffsetComponentMode, AZ::SystemAllocator);

            AZ_RTTI(
                OffsetComponentMode,
                "{F1027870-8021-4661-9999-F7D78BCE1632}",
                AzToolsFramework::BaseShapeComponentMode);

            OffsetComponentMode(
                const AZ::EntityComponentIdPair& entityComponentIdPair,
                AZ::Uuid componentType)
                : BaseShapeComponentMode(entityComponentIdPair, componentType)
            {
                auto offsetEdit = AZStd::make_unique<AzToolsFramework::ShapeTranslationOffsetViewportEdit>();
                AzToolsFramework::InstallBaseShapeViewportEditFunctions(
                    offsetEdit.get(),
                    m_entityComponentIdPair);

                const auto dimensionsSubMode =
                    static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions);
                m_subModes[dimensionsSubMode] = AZStd::move(offsetEdit);
                m_subModes[dimensionsSubMode]->Setup(AzToolsFramework::GetMainManipulatorManagerId());
                m_subModes[dimensionsSubMode]->AddEntityComponentIdPair(m_entityComponentIdPair);
                AzToolsFramework::ShapeComponentModeRequestBus::Handler::BusConnect(m_entityComponentIdPair);
            }

            ~OffsetComponentMode() override
            {
                AzToolsFramework::ShapeComponentModeRequestBus::Handler::BusDisconnect();
            }

            [[nodiscard]]
            AZStd::string GetComponentModeName() const override
            {
                return "Shape Offset Edit Mode";
            }

            [[nodiscard]]
            AZ::Uuid GetComponentModeType() const override
            {
                return azrtti_typeid<OffsetComponentMode>();
            }
        };
    } // namespace ComponentModes

    ColliderComponent::ColliderComponent(
        AZStd::vector<ColliderShapeConfiguration> configurations)
        : m_configurations(AZStd::move(configurations))
    {
    }

    void ColliderComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            ColliderShapeConfiguration::Reflect(serializeContext);

            serializeContext
                ->Class<ColliderComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Shapes", &ColliderComponent::m_configurations)
                ->Field("ActiveShapeIndex", &ColliderComponent::m_activeShapeIndex)
                ->Field("ComponentMode", &ColliderComponent::m_componentModeDelegate);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ColliderComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Collider"),
                        QT_TRANSLATE_NOOP("Jolt", "Adds one or more collision shapes to a Jolt body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_configurations,
                        QT_TRANSLATE_NOOP("Jolt", "Shapes"),
                        QT_TRANSLATE_NOOP("Jolt", "Geometry, materials, and body-local transforms."))
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ColliderComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_activeShapeIndex,
                        QT_TRANSLATE_NOOP("Jolt", "Active shape"),
                        QT_TRANSLATE_NOOP("Jolt", "Zero-based index of the shape edited by component mode."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ColliderComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_componentModeDelegate,
                        QT_TRANSLATE_NOOP("Jolt", "Component mode"),
                        QT_TRANSLATE_NOOP("Jolt", "Edit the active shape in the viewport."))
                    ->Attribute(
                        AZ::Edit::Attributes::Visibility,
                        AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void ColliderComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::ColliderComponent::GetProvidedServices(provided);
    }

    void ColliderComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::ColliderComponent::GetIncompatibleServices(incompatible);
    }

    void ColliderComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());

        const AZ::EntityComponentIdPair entityComponentIdPair(GetEntityId(), GetId());
        AzToolsFramework::BoxManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::CapsuleManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::CylinderManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);

        m_componentModeDelegate.Connect<ColliderComponent>(entityComponentIdPair, this);
        m_componentModeDelegate.SetAddComponentModeCallback(
            [this](const AZ::EntityComponentIdPair& pair)
            {
                if (m_activeShapeIndex >= m_configurations.size())
                {
                    return;
                }

                const ShapeGeometry& geometry = m_configurations[m_activeShapeIndex].m_shape.m_geometry;
                m_componentModeGeometryIndex = geometry.index();

                const auto addComponentMode = [&pair](auto builder)
                {
                    using Requests =
                        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests;
                    using Builders =
                        AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders;

                    AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                        &Requests::AddComponentModes,
                        Builders(pair.GetEntityId(), AZStd::move(builder)));
                };

                const bool allowAsymmetricalEditing = true;
                if (AZStd::holds_alternative<SphereShapeConfiguration>(geometry))
                {
                    auto builder =
                        AzToolsFramework::ComponentModeFramework::CreateComponentModeBuilder<
                            ColliderComponent,
                            AzToolsFramework::SphereComponentMode>(
                                pair,
                                allowAsymmetricalEditing);
                    addComponentMode(AZStd::move(builder));
                    return;
                }

                if (AZStd::holds_alternative<CapsuleShapeConfiguration>(geometry))
                {
                    auto builder =
                        AzToolsFramework::ComponentModeFramework::CreateComponentModeBuilder<
                            ColliderComponent,
                            AzToolsFramework::CapsuleComponentMode>(
                                pair,
                                allowAsymmetricalEditing);
                    addComponentMode(AZStd::move(builder));
                    return;
                }

                if (AZStd::holds_alternative<BoxShapeConfiguration>(geometry))
                {
                    auto builder =
                        AzToolsFramework::ComponentModeFramework::CreateComponentModeBuilder<
                            ColliderComponent,
                            AzToolsFramework::BoxComponentMode>(
                                pair,
                                allowAsymmetricalEditing);
                    addComponentMode(AZStd::move(builder));
                    return;
                }

                if (AZStd::holds_alternative<CylinderShapeConfiguration>(geometry))
                {
                    auto builder =
                        AzToolsFramework::ComponentModeFramework::CreateComponentModeBuilder<
                            ColliderComponent,
                            AzToolsFramework::CylinderComponentMode>(
                                pair,
                                allowAsymmetricalEditing);
                    addComponentMode(AZStd::move(builder));
                    return;
                }

                auto builder =
                    AzToolsFramework::ComponentModeFramework::CreateComponentModeBuilder<
                        ColliderComponent,
                        ComponentModes::OffsetComponentMode>(pair);
                addComponentMode(AZStd::move(builder));
            });
    }

    void ColliderComponent::Deactivate()
    {
        m_componentModeDelegate.Disconnect();
        m_componentModeGeometryIndex = AZStd::variant_npos;
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::CylinderManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::CapsuleManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::BoxManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void ColliderComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::ColliderComponent>(m_configurations);
    }

    void ColliderComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        const AZ::Matrix3x4 entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        for (const ColliderShapeConfiguration& configuration : m_configurations)
        {
            const AZ::Matrix3x4 shapeTransform = entityTransform
                * AZ::Matrix3x4::CreateFromTransform(configuration.m_localTransform);
            DrawShapeGeometry(debugDisplay, configuration.m_shape.m_geometry, shapeTransform);
        }
    }

    bool ColliderComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb ColliderComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        const AZ::Matrix3x4 entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        for (const ColliderShapeConfiguration& configuration : m_configurations)
        {
            const AZ::Matrix3x4 shapeTransform = entityTransform
                * AZ::Matrix3x4::CreateFromTransform(configuration.m_localTransform);
            bounds.AddAabb(CalculateShapeBounds(configuration.m_shape.m_geometry, shapeTransform));
        }
        return bounds;
    }

    bool ColliderComponent::EditorSelectionIntersectRayViewport(
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

    float ColliderComponent::GetRadius() const
    {
        if (m_activeShapeIndex >= m_configurations.size())
        {
            return 0.0f;
        }

        const ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        const float scale = configuration.m_localTransform.GetUniformScale();
        const ShapeGeometry& geometry = configuration.m_shape.m_geometry;
        if (const auto* sphere = AZStd::get_if<SphereShapeConfiguration>(&geometry))
        {
            return sphere->m_radius * scale;
        }

        if (const auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&geometry))
        {
            return capsule->m_radius * scale;
        }

        if (const auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&geometry))
        {
            return cylinder->m_radius * scale;
        }

        return 0.0f;
    }

    void ColliderComponent::SetRadius(
        float radius)
    {
        if (m_activeShapeIndex >= m_configurations.size()
            || !AZ::IsFiniteFloat(radius)
            || radius <= 0.0f)
        {
            return;
        }

        ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        const float scale = configuration.m_localTransform.GetUniformScale();
        if (!AZ::IsFiniteFloat(scale) || scale <= 0.0f)
        {
            return;
        }

        ShapeGeometry& geometry = configuration.m_shape.m_geometry;
        const float unscaledRadius = radius / scale;
        if (auto* sphere = AZStd::get_if<SphereShapeConfiguration>(&geometry))
        {
            sphere->m_radius = unscaledRadius;
        }
        else if (auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&geometry))
        {
            const float totalHeight = capsule->m_cylinderHeight + 2.0f * capsule->m_radius;
            capsule->m_radius = unscaledRadius;
            capsule->m_cylinderHeight = AZStd::max(totalHeight - 2.0f * unscaledRadius, 0.0f);
        }
        else if (auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&geometry))
        {
            cylinder->m_radius = unscaledRadius;
        }
        else
        {
            return;
        }

        ConfigurationChanged();
    }

    float ColliderComponent::GetHeight() const
    {
        if (m_activeShapeIndex >= m_configurations.size())
        {
            return 0.0f;
        }

        const ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        const float scale = configuration.m_localTransform.GetUniformScale();
        const ShapeGeometry& geometry = configuration.m_shape.m_geometry;
        if (const auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&geometry))
        {
            return (capsule->m_cylinderHeight + 2.0f * capsule->m_radius) * scale;
        }

        if (const auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&geometry))
        {
            return cylinder->m_height * scale;
        }

        return 0.0f;
    }

    void ColliderComponent::SetHeight(
        float height)
    {
        if (m_activeShapeIndex >= m_configurations.size()
            || !AZ::IsFiniteFloat(height)
            || height <= 0.0f)
        {
            return;
        }

        ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        const float scale = configuration.m_localTransform.GetUniformScale();
        if (!AZ::IsFiniteFloat(scale) || scale <= 0.0f)
        {
            return;
        }

        ShapeGeometry& geometry = configuration.m_shape.m_geometry;
        const float unscaledHeight = height / scale;
        if (auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&geometry))
        {
            capsule->m_cylinderHeight = AZStd::max(unscaledHeight - 2.0f * capsule->m_radius, 0.0f);
        }
        else if (auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&geometry))
        {
            cylinder->m_height = unscaledHeight;
        }
        else
        {
            return;
        }

        ConfigurationChanged();
    }

    AZ::Vector3 ColliderComponent::GetDimensions() const
    {
        if (m_activeShapeIndex >= m_configurations.size())
        {
            return AZ::Vector3::CreateZero();
        }

        const ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        const auto* box = AZStd::get_if<BoxShapeConfiguration>(&configuration.m_shape.m_geometry);
        if (box)
        {
            return box->m_dimensions * configuration.m_localTransform.GetUniformScale();
        }

        return AZ::Vector3::CreateZero();
    }

    void ColliderComponent::SetDimensions(
        const AZ::Vector3& dimensions)
    {
        if (m_activeShapeIndex >= m_configurations.size()
            || !dimensions.IsFinite()
            || dimensions.GetMinElement() <= 0.0f)
        {
            return;
        }

        ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        auto* box = AZStd::get_if<BoxShapeConfiguration>(&configuration.m_shape.m_geometry);
        const float scale = configuration.m_localTransform.GetUniformScale();
        if (!box || !AZ::IsFiniteFloat(scale) || scale <= 0.0f)
        {
            return;
        }

        box->m_dimensions = dimensions / scale;
        ConfigurationChanged();
    }

    AZ::Transform ColliderComponent::GetCurrentLocalTransform() const
    {
        if (m_activeShapeIndex < m_configurations.size())
        {
            return m_configurations[m_activeShapeIndex].m_localTransform;
        }

        return AZ::Transform::CreateIdentity();
    }

    AZ::Vector3 ColliderComponent::GetTranslationOffset() const
    {
        return GetCurrentLocalTransform().GetTranslation();
    }

    void ColliderComponent::SetTranslationOffset(
        const AZ::Vector3& translationOffset)
    {
        if (m_activeShapeIndex >= m_configurations.size() || !translationOffset.IsFinite())
        {
            return;
        }

        m_configurations[m_activeShapeIndex].m_localTransform.SetTranslation(translationOffset);
        ConfigurationChanged();
    }

    AZ::Transform ColliderComponent::GetManipulatorSpace() const
    {
        return GetWorldTM();
    }

    AZ::Quaternion ColliderComponent::GetRotationOffset() const
    {
        if (m_activeShapeIndex >= m_configurations.size())
        {
            return AZ::Quaternion::CreateIdentity();
        }

        const ColliderShapeConfiguration& configuration = m_configurations[m_activeShapeIndex];
        AZ::Quaternion rotation = configuration.m_localTransform.GetRotation();
        const ShapeGeometry& geometry = configuration.m_shape.m_geometry;
        if (AZStd::holds_alternative<CapsuleShapeConfiguration>(geometry)
            || AZStd::holds_alternative<CylinderShapeConfiguration>(geometry))
        {
            rotation *= AZ::Quaternion::CreateRotationX(-AZ::Constants::HalfPi);
        }

        return rotation;
    }

    AZ::u32 ColliderComponent::ConfigurationChanged()
    {
        SetDirty();
        if (m_componentModeDelegate.AddedToComponentMode()
            && (m_activeShapeIndex >= m_configurations.size()
                || m_configurations[m_activeShapeIndex].m_shape.m_geometry.index()
                    != m_componentModeGeometryIndex))
        {
            AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::EndComponentMode);
            return AzToolsFramework::Refresh_EntireTree;
        }

        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::Refresh,
            AZ::EntityComponentIdPair(GetEntityId(), GetId()));
        return AzToolsFramework::Refresh_Values;
    }
} // namespace Jolt::Editor
