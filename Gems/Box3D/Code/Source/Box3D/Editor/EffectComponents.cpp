/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/EffectComponents.h>

#include <Box3D/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>
#include <AzToolsFramework/ComponentModes/BaseShapeComponentMode.h>
#include <AzToolsFramework/ComponentModes/ShapeTranslationOffsetViewportEdit.h>
#include <AzToolsFramework/ComponentModes/SphereComponentMode.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <Box3D/EffectComponents.h>

namespace Box3D::Editor
{
    namespace ComponentModes
    {
        class VelocityComponentMode final
            : public AzToolsFramework::BaseShapeComponentMode
        {
        public:
            AZ_CLASS_ALLOCATOR(VelocityComponentMode, AZ::SystemAllocator);
            AZ_RTTI(VelocityComponentMode, "{EC42DDBB-3C3F-451B-8B9A-EFE72CCB8D90}", AzToolsFramework::BaseShapeComponentMode);

            VelocityComponentMode(const AZ::EntityComponentIdPair& entityComponentIdPair, const AZ::Uuid componentType)
                : BaseShapeComponentMode(entityComponentIdPair, componentType)
            {
                auto velocityEdit = AZStd::make_unique<AzToolsFramework::ShapeTranslationOffsetViewportEdit>();
                AzToolsFramework::InstallBaseShapeViewportEditFunctions(velocityEdit.get(), m_entityComponentIdPair);
                m_subModes[static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions)] =
                    AZStd::move(velocityEdit);
                m_subModes[static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions)]->Setup(
                    AzToolsFramework::GetMainManipulatorManagerId());
                m_subModes[static_cast<AZ::u32>(AzToolsFramework::ShapeComponentModeRequests::SubMode::Dimensions)]
                    ->AddEntityComponentIdPair(m_entityComponentIdPair);
                AzToolsFramework::ShapeComponentModeRequestBus::Handler::BusConnect(m_entityComponentIdPair);
            }

            ~VelocityComponentMode() override
            {
                AzToolsFramework::ShapeComponentModeRequestBus::Handler::BusDisconnect();
            }

            AZStd::string GetComponentModeName() const override
            {
                return "Wind Velocity Edit Mode";
            }

            AZ::Uuid GetComponentModeType() const override
            {
                return azrtti_typeid<VelocityComponentMode>();
            }
        };
    } // namespace ComponentModes

    void ExplosionComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ExplosionComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(2)
                ->Field("Configuration", &ExplosionComponent::m_configuration)
                ->Field("WorldName", &ExplosionComponent::m_worldName)
                ->Field("ExplodeOnActivate", &ExplosionComponent::m_explodeOnActivate)
                ->Field("ComponentMode", &ExplosionComponent::m_componentModeDelegate);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ExplosionComponent>(
                        QT_TRANSLATE_NOOP("Box3D", "Box3D Explosion"),
                        QT_TRANSLATE_NOOP("Box3D", "Applies a radial impulse to bodies in the default physics scene."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ExplosionComponent::m_worldName,
                        QT_TRANSLATE_NOOP("Box3D", "World"),
                        QT_TRANSLATE_NOOP("Box3D", "An empty name selects the default world."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ExplosionComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Box3D", "Explosion"),
                        QT_TRANSLATE_NOOP("Box3D", "Radius, falloff, impulse, and collision filtering."))
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &ExplosionComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ExplosionComponent::m_explodeOnActivate,
                        QT_TRANSLATE_NOOP("Box3D", "Explode on activate"),
                        QT_TRANSLATE_NOOP("Box3D", "Trigger once when the game entity activates."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ExplosionComponent::m_componentModeDelegate,
                        QT_TRANSLATE_NOOP("Box3D", "Component mode"),
                        QT_TRANSLATE_NOOP("Box3D", "Edit explosion position and radius in the viewport."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void ExplosionComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::ExplosionComponent::GetProvidedServices(provided);
    }

    void ExplosionComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::ExplosionComponent::GetIncompatibleServices(incompatible);
    }

    void ExplosionComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::ExplosionComponent::GetRequiredServices(required);
    }

    void ExplosionComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
        const AZ::EntityComponentIdPair entityComponentIdPair(GetEntityId(), GetId());
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        const bool allowAsymmetricalEditing = true;
        m_componentModeDelegate.ConnectWithSingleComponentMode<ExplosionComponent, AzToolsFramework::SphereComponentMode>(
            entityComponentIdPair, this, allowAsymmetricalEditing);
    }

    void ExplosionComponent::Deactivate()
    {
        m_componentModeDelegate.Disconnect();
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::RadiusManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void ExplosionComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::ExplosionComponent>(m_configuration, m_worldName, m_explodeOnActivate);
    }

    void ExplosionComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        debugDisplay.DrawWireSphere(
            GetWorldTM().TransformPoint(m_configuration.m_position), AZStd::abs(GetWorldTM().GetUniformScale()) * m_configuration.m_radius);
    }

    bool ExplosionComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb ExplosionComponent::GetEditorSelectionBoundsViewport([[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        const AZ::Transform worldTransform = GetWorldTM();
        const AZ::Vector3 center = worldTransform.TransformPoint(m_configuration.m_position);
        const AZ::Vector3 extent(AZStd::abs(worldTransform.GetUniformScale()) * m_configuration.m_radius);
        return AZ::Aabb::CreateCenterHalfExtents(center, extent);
    }

    bool ExplosionComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo, const AZ::Vector3& rayStart, const AZ::Vector3& rayDirection, float& distance)
    {
        return IntersectEditorBounds(GetEditorSelectionBoundsViewport(viewportInfo), rayStart, rayDirection, distance);
    }

    float ExplosionComponent::GetRadius() const
    {
        return m_configuration.m_radius;
    }

    void ExplosionComponent::SetRadius(const float radius)
    {
        if (!AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return;
        }
        m_configuration.m_radius = radius;
        ConfigurationChanged();
    }

    AZ::Vector3 ExplosionComponent::GetTranslationOffset() const
    {
        return m_configuration.m_position;
    }

    void ExplosionComponent::SetTranslationOffset(const AZ::Vector3& translationOffset)
    {
        if (!translationOffset.IsFinite())
        {
            return;
        }
        m_configuration.m_position = translationOffset;
        ConfigurationChanged();
    }

    AZ::Transform ExplosionComponent::GetManipulatorSpace() const
    {
        return GetWorldTM();
    }

    AZ::Quaternion ExplosionComponent::GetRotationOffset() const
    {
        return AZ::Quaternion::CreateIdentity();
    }

    AZ::u32 ExplosionComponent::ConfigurationChanged()
    {
        SetDirty();
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::Refresh,
            AZ::EntityComponentIdPair(GetEntityId(), GetId()));
        return AzToolsFramework::Refresh_Values;
    }

    void WindComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<WindComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(1)
                ->Field("Configuration", &WindComponent::m_configuration)
                ->Field("Enabled", &WindComponent::m_enabled)
                ->Field("ComponentMode", &WindComponent::m_componentModeDelegate);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<WindComponent>(
                        QT_TRANSLATE_NOOP("Box3D", "Box3D Wind"),
                        QT_TRANSLATE_NOOP("Box3D", "Applies aerodynamic drag and lift to this rigid body."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &WindComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Box3D", "Wind"),
                        QT_TRANSLATE_NOOP("Box3D", "Velocity, drag, lift, speed limit, and wake behavior."))
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &WindComponent::ConfigurationChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &WindComponent::m_enabled,
                        QT_TRANSLATE_NOOP("Box3D", "Enabled"),
                        QT_TRANSLATE_NOOP("Box3D", "Apply wind before each simulation tick."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &WindComponent::m_componentModeDelegate,
                        QT_TRANSLATE_NOOP("Box3D", "Component mode"),
                        QT_TRANSLATE_NOOP("Box3D", "Edit wind velocity in the viewport."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void WindComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::WindComponent::GetProvidedServices(provided);
    }

    void WindComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::WindComponent::GetIncompatibleServices(incompatible);
    }

    void WindComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::WindComponent::GetRequiredServices(required);
    }

    void WindComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
        const AZ::EntityComponentIdPair entityComponentIdPair(GetEntityId(), GetId());
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusConnect(entityComponentIdPair);
        m_componentModeDelegate.ConnectWithSingleComponentMode<WindComponent, ComponentModes::VelocityComponentMode>(
            entityComponentIdPair, this);
    }

    void WindComponent::Deactivate()
    {
        m_componentModeDelegate.Disconnect();
        AzToolsFramework::ShapeManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void WindComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Box3D::WindComponent>(m_configuration, m_enabled);
    }

    void WindComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        const AZ::Vector3 start = GetWorldTM().GetTranslation();
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        debugDisplay.DrawLine(start, start + m_configuration.m_velocity);
    }

    bool WindComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb WindComponent::GetEditorSelectionBoundsViewport([[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        const AZ::Vector3 start = GetWorldTM().GetTranslation();
        AZ::Aabb bounds =
            AZ::Aabb::CreateFromMinMax(start.GetMin(start + m_configuration.m_velocity), start.GetMax(start + m_configuration.m_velocity));
        bounds.Expand(AZ::Vector3(0.1f));
        return bounds;
    }

    bool WindComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo, const AZ::Vector3& rayStart, const AZ::Vector3& rayDirection, float& distance)
    {
        return IntersectEditorBounds(GetEditorSelectionBoundsViewport(viewportInfo), rayStart, rayDirection, distance);
    }

    AZ::Vector3 WindComponent::GetTranslationOffset() const
    {
        return m_configuration.m_velocity;
    }

    void WindComponent::SetTranslationOffset(const AZ::Vector3& translationOffset)
    {
        if (!translationOffset.IsFinite())
        {
            return;
        }
        m_configuration.m_velocity = translationOffset;
        ConfigurationChanged();
    }

    AZ::Transform WindComponent::GetManipulatorSpace() const
    {
        return AZ::Transform::CreateTranslation(GetWorldTM().GetTranslation());
    }

    AZ::Quaternion WindComponent::GetRotationOffset() const
    {
        return AZ::Quaternion::CreateIdentity();
    }

    AZ::u32 WindComponent::ConfigurationChanged()
    {
        SetDirty();
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::Refresh,
            AZ::EntityComponentIdPair(GetEntityId(), GetId()));
        return AzToolsFramework::Refresh_Values;
    }
} // namespace Box3D::Editor
