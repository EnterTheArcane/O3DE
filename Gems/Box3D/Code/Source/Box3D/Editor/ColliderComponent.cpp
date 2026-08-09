/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/ColliderComponent.h>

#include <Box3D/ColliderComponent.h>
#include <Box3D/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>
#include <AzToolsFramework/ComponentModes/BoxComponentMode.h>
#include <AzToolsFramework/ComponentModes/CapsuleComponentMode.h>
#include <AzToolsFramework/ComponentModes/CylinderComponentMode.h>
#include <AzToolsFramework/ComponentModes/ShapeTranslationOffsetViewportEdit.h>
#include <AzToolsFramework/ComponentModes/SphereComponentMode.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>

namespace Box3D::Editor
{
    namespace
    {
        class OffsetComponentMode final
            : public AzToolsFramework::BaseShapeComponentMode
        {
        public:
            AZ_CLASS_ALLOCATOR(OffsetComponentMode, AZ::SystemAllocator);
            AZ_RTTI(OffsetComponentMode, "{535356E4-2609-4421-ADCD-0BEFDAB38B80}", AzToolsFramework::BaseShapeComponentMode);

            OffsetComponentMode(const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType)
                : BaseShapeComponentMode(entityComponentIdPair, componentType)
            {
                auto offsetEdit = AZStd::make_unique<AzToolsFramework::ShapeTranslationOffsetViewportEdit>();
                AzToolsFramework::InstallBaseShapeViewportEditFunctions(offsetEdit.get(), m_entityComponentIdPair);
                m_subModes[static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions)] =
                    AZStd::move(offsetEdit);
                m_subModes[static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions)]->Setup(
                    AzToolsFramework::GetMainManipulatorManagerId());
                m_subModes[static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions)]
                    ->AddEntityComponentIdPair(m_entityComponentIdPair);
                AzToolsFramework::ShapeComponentModeRequestBus::Handler::BusConnect(m_entityComponentIdPair);
            }

            ~OffsetComponentMode() override
            {
                AzToolsFramework::ShapeComponentModeRequestBus::Handler::BusDisconnect();
            }

            AZStd::string GetComponentModeName() const override
            {
                return "Shape Offset Edit Mode";
            }

            AZ::Uuid GetComponentModeType() const override
            {
                return azrtti_typeid<OffsetComponentMode>();
            }
        };
    } // namespace

    void ColliderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ColliderComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(5)
                ->Field("Shapes", &ColliderComponent::m_shapes)
                ->Field("ActiveShapeIndex", &ColliderComponent::m_activeShapeIndex)
                ->Field("ComponentMode", &ColliderComponent::m_componentModeDelegate);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ColliderComponent>(
                        QT_TRANSLATE_NOOP("Box3D", "Box3D Collider"),
                        QT_TRANSLATE_NOOP("Box3D", "Adds one or more collision shapes to a Box3D body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_shapes,
                        QT_TRANSLATE_NOOP("Box3D", "Shapes"),
                        QT_TRANSLATE_NOOP("Box3D", "Geometry, materials, filtering, and event policy."))
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ColliderComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_activeShapeIndex,
                        QT_TRANSLATE_NOOP("Box3D", "Active shape"),
                        QT_TRANSLATE_NOOP("Box3D", "Zero-based index of the shape edited by component mode."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ColliderComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_componentModeDelegate,
                        QT_TRANSLATE_NOOP("Box3D", "Component mode"),
                        QT_TRANSLATE_NOOP("Box3D", "Edit the active primitive shape in the viewport."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void ColliderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::ColliderComponent::GetProvidedServices(provided);
    }

    void ColliderComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::ColliderComponent::GetIncompatibleServices(incompatible);
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
                if (m_activeShapeIndex >= m_shapes.size())
                {
                    return;
                }

                const ShapeGeometry& geometry = m_shapes[m_activeShapeIndex].m_geometry;
                m_componentModeGeometryIndex = geometry.index();
                const bool allowAsymmetricalEditing = true;
                if (AZStd::holds_alternative<SphereShapeConfiguration>(geometry))
                {
                    const auto builder = AzToolsFramework::ComponentModeFramework::
                        CreateComponentModeBuilder<ColliderComponent, AzToolsFramework::SphereComponentMode>(
                            pair, allowAsymmetricalEditing);
                    AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                        &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::AddComponentModes,
                        AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders(pair.GetEntityId(), builder));
                    return;
                }
                if (AZStd::holds_alternative<CapsuleShapeConfiguration>(geometry))
                {
                    const auto builder = AzToolsFramework::ComponentModeFramework::
                        CreateComponentModeBuilder<ColliderComponent, AzToolsFramework::CapsuleComponentMode>(
                            pair, allowAsymmetricalEditing);
                    AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                        &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::AddComponentModes,
                        AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders(pair.GetEntityId(), builder));
                    return;
                }
                if (AZStd::holds_alternative<BoxShapeConfiguration>(geometry))
                {
                    const auto builder = AzToolsFramework::ComponentModeFramework::
                        CreateComponentModeBuilder<ColliderComponent, AzToolsFramework::BoxComponentMode>(pair, allowAsymmetricalEditing);
                    AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                        &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::AddComponentModes,
                        AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders(pair.GetEntityId(), builder));
                    return;
                }
                if (AZStd::holds_alternative<CylinderShapeConfiguration>(geometry))
                {
                    const auto builder = AzToolsFramework::ComponentModeFramework::
                        CreateComponentModeBuilder<ColliderComponent, AzToolsFramework::CylinderComponentMode>(
                            pair, allowAsymmetricalEditing);
                    AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                        &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::AddComponentModes,
                        AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders(pair.GetEntityId(), builder));
                    return;
                }
                const auto builder =
                    AzToolsFramework::ComponentModeFramework::CreateComponentModeBuilder<ColliderComponent, OffsetComponentMode>(pair);
                AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                    &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::AddComponentModes,
                    AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders(pair.GetEntityId(), builder));
            });
    }

    void ColliderComponent::Deactivate()
    {
        m_componentModeDelegate.Disconnect();
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::CylinderManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::CapsuleManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::BoxManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void ColliderComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::ColliderComponent>(m_shapes);
    }

    void ColliderComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        for (const ShapeConfiguration& shape : m_shapes)
        {
            const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM()) *
                AZ::Matrix3x4::CreateFromTransform(shape.m_properties.m_localTransform) *
                AZ::Matrix3x4::CreateScale(shape.m_properties.m_scale);
            DrawShapeGeometry(debugDisplay, shape.m_geometry, transform);
        }
    }

    bool ColliderComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb ColliderComponent::GetEditorSelectionBoundsViewport([[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        for (const ShapeConfiguration& shape : m_shapes)
        {
            const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM()) *
                AZ::Matrix3x4::CreateFromTransform(shape.m_properties.m_localTransform) *
                AZ::Matrix3x4::CreateScale(shape.m_properties.m_scale);
            bounds.AddAabb(CalculateShapeBounds(shape.m_geometry, transform));
        }
        return bounds;
    }

    bool ColliderComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo, const AZ::Vector3& rayStart, const AZ::Vector3& rayDirection, float& distance)
    {
        return IntersectEditorBounds(GetEditorSelectionBoundsViewport(viewportInfo), rayStart, rayDirection, distance);
    }

    float ColliderComponent::GetRadius() const
    {
        if (m_activeShapeIndex >= m_shapes.size())
        {
            return 0.0f;
        }
        const ShapeConfiguration& shape = m_shapes[m_activeShapeIndex];
        const float scale = shape.m_properties.m_scale.GetX();
        if (const auto* sphere = AZStd::get_if<SphereShapeConfiguration>(&shape.m_geometry))
        {
            return sphere->m_radius * scale;
        }
        if (const auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&shape.m_geometry))
        {
            return capsule->m_radius * scale;
        }
        if (const auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&shape.m_geometry))
        {
            return cylinder->m_radius * scale;
        }
        return 0.0f;
    }

    void ColliderComponent::SetRadius(float radius)
    {
        if (m_activeShapeIndex >= m_shapes.size() || !AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return;
        }
        ShapeConfiguration& shape = m_shapes[m_activeShapeIndex];
        const float scale = shape.m_properties.m_scale.GetX();
        if (!AZ::IsFiniteFloat(scale) || scale <= 0.0f)
        {
            return;
        }
        if (auto* sphere = AZStd::get_if<SphereShapeConfiguration>(&shape.m_geometry))
        {
            sphere->m_radius = radius / scale;
        }
        else if (auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&shape.m_geometry))
        {
            capsule->m_radius = radius / scale;
        }
        else if (auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&shape.m_geometry))
        {
            cylinder->m_radius = radius / scale;
        }
        ConfigurationChanged();
    }

    float ColliderComponent::GetHeight() const
    {
        if (m_activeShapeIndex >= m_shapes.size())
        {
            return 0.0f;
        }
        const ShapeConfiguration& shape = m_shapes[m_activeShapeIndex];
        const float scale = shape.m_properties.m_scale.GetZ();
        if (const auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&shape.m_geometry))
        {
            return capsule->m_height * scale;
        }
        if (const auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&shape.m_geometry))
        {
            return cylinder->m_height * scale;
        }
        return 0.0f;
    }

    void ColliderComponent::SetHeight(float height)
    {
        if (m_activeShapeIndex >= m_shapes.size() || !AZ::IsFiniteFloat(height) || height <= 0.0f)
        {
            return;
        }
        ShapeConfiguration& shape = m_shapes[m_activeShapeIndex];
        const float scale = shape.m_properties.m_scale.GetZ();
        if (!AZ::IsFiniteFloat(scale) || scale <= 0.0f)
        {
            return;
        }
        if (auto* capsule = AZStd::get_if<CapsuleShapeConfiguration>(&shape.m_geometry))
        {
            capsule->m_height = height / scale;
        }
        else if (auto* cylinder = AZStd::get_if<CylinderShapeConfiguration>(&shape.m_geometry))
        {
            cylinder->m_height = height / scale;
        }
        ConfigurationChanged();
    }

    AZ::Vector3 ColliderComponent::GetDimensions() const
    {
        if (m_activeShapeIndex >= m_shapes.size())
        {
            return AZ::Vector3::CreateZero();
        }
        const ShapeConfiguration& shape = m_shapes[m_activeShapeIndex];
        const auto* box = AZStd::get_if<BoxShapeConfiguration>(&shape.m_geometry);
        return box != nullptr ? 2.0f * box->m_halfExtents * shape.m_properties.m_scale : AZ::Vector3::CreateZero();
    }

    void ColliderComponent::SetDimensions(const AZ::Vector3& dimensions)
    {
        if (m_activeShapeIndex >= m_shapes.size() || !dimensions.IsFinite() || dimensions.GetMinElement() <= 0.0f)
        {
            return;
        }
        ShapeConfiguration& shape = m_shapes[m_activeShapeIndex];
        auto* box = AZStd::get_if<BoxShapeConfiguration>(&shape.m_geometry);
        if (box == nullptr || !shape.m_properties.m_scale.IsFinite() || shape.m_properties.m_scale.GetMinElement() <= 0.0f)
        {
            return;
        }
        box->m_halfExtents = 0.5f * dimensions / shape.m_properties.m_scale;
        ConfigurationChanged();
    }

    AZ::Transform ColliderComponent::GetCurrentLocalTransform() const
    {
        return m_activeShapeIndex < m_shapes.size() ? m_shapes[m_activeShapeIndex].m_properties.m_localTransform
                                                    : AZ::Transform::CreateIdentity();
    }

    AZ::Vector3 ColliderComponent::GetTranslationOffset() const
    {
        return GetCurrentLocalTransform().GetTranslation();
    }

    void ColliderComponent::SetTranslationOffset(const AZ::Vector3& translationOffset)
    {
        if (m_activeShapeIndex >= m_shapes.size() || !translationOffset.IsFinite())
        {
            return;
        }
        m_shapes[m_activeShapeIndex].m_properties.m_localTransform.SetTranslation(translationOffset);
        ConfigurationChanged();
    }

    AZ::Transform ColliderComponent::GetManipulatorSpace() const
    {
        return GetWorldTM();
    }

    AZ::Quaternion ColliderComponent::GetRotationOffset() const
    {
        return GetCurrentLocalTransform().GetRotation();
    }

    AZ::u32 ColliderComponent::ConfigurationChanged()
    {
        SetDirty();
        if (m_componentModeDelegate.AddedToComponentMode() &&
            (m_activeShapeIndex >= m_shapes.size() || m_shapes[m_activeShapeIndex].m_geometry.index() != m_componentModeGeometryIndex))
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
} // namespace Box3D::Editor
