/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/VehicleComponents.h>

#include <Jolt/Editor/DebugDraw.h>
#include <Jolt/VehicleComponents.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>

namespace Jolt::Editor
{
    namespace
    {
        struct WheelVisualization final
        {
            AZ::Matrix3x4 m_transform = AZ::Matrix3x4::CreateIdentity();
            AZ::Vector3 m_attachmentPoint = AZ::Vector3::CreateZero();
            AZ::Vector3 m_forcePoint = AZ::Vector3::CreateZero();
            AZ::Vector3 m_maximumSuspensionPoint = AZ::Vector3::CreateZero();
            AZ::Vector3 m_minimumSuspensionPoint = AZ::Vector3::CreateZero();
        };

        [[nodiscard]]
        const WheelConfiguration& GetCommonWheel(const WheelConfiguration& wheel)
        {
            return wheel;
        }

        [[nodiscard]]
        const WheelConfiguration& GetCommonWheel(const TrackedWheelConfiguration& wheel)
        {
            return wheel.m_common;
        }

        [[nodiscard]]
        bool BuildWheelVisualization(
            const WheelConfiguration& wheel,
            const AZ::Matrix3x4& entityTransform,
            WheelVisualization& visualization)
        {
            if (!wheel.m_position.IsFinite()
                || !wheel.m_suspensionDirection.IsFinite()
                || wheel.m_suspensionDirection.IsZero()
                || !wheel.m_suspensionForcePoint.IsFinite()
                || !wheel.m_wheelForward.IsFinite()
                || wheel.m_wheelForward.IsZero()
                || !wheel.m_wheelUp.IsFinite()
                || wheel.m_wheelUp.IsZero()
                || !AZ::IsFiniteFloat(wheel.m_radius)
                || wheel.m_radius <= 0.0f
                || !AZ::IsFiniteFloat(wheel.m_suspensionMaximumLength)
                || !AZ::IsFiniteFloat(wheel.m_suspensionMinimumLength)
                || !AZ::IsFiniteFloat(wheel.m_width)
                || wheel.m_width <= 0.0f)
            {
                return false;
            }

            const AZ::Vector3 suspensionDirection = wheel.m_suspensionDirection.GetNormalized();
            const AZ::Vector3 forward = wheel.m_wheelForward.GetNormalized();
            AZ::Vector3 right = forward.Cross(wheel.m_wheelUp);
            if (right.IsZero())
            {
                return false;
            }
            right.Normalize();
            const AZ::Vector3 up = right.Cross(forward).GetNormalized();

            const AZ::Vector3 maximumSuspensionPoint =
                wheel.m_position + suspensionDirection * wheel.m_suspensionMaximumLength;
            const AZ::Matrix3x4 localWheelTransform = AZ::Matrix3x4::CreateFromColumns(
                up,
                right,
                forward,
                maximumSuspensionPoint);
            visualization = {
                .m_transform = entityTransform * localWheelTransform,
                .m_attachmentPoint = entityTransform.TransformPoint(wheel.m_position),
                .m_forcePoint = entityTransform.TransformPoint(wheel.m_suspensionForcePoint),
                .m_maximumSuspensionPoint = entityTransform.TransformPoint(maximumSuspensionPoint),
                .m_minimumSuspensionPoint = entityTransform.TransformPoint(
                    wheel.m_position + suspensionDirection * wheel.m_suspensionMinimumLength),
            };
            return true;
        }

        void DrawWheel(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const WheelConfiguration& wheel,
            const WheelVisualization& visualization)
        {
            debugDisplay.DrawLine(
                visualization.m_minimumSuspensionPoint,
                visualization.m_maximumSuspensionPoint);
            if (wheel.m_enableSuspensionForcePoint)
            {
                debugDisplay.DrawLine(
                    visualization.m_attachmentPoint,
                    visualization.m_forcePoint);
                debugDisplay.DrawWireSphere(visualization.m_forcePoint, 0.05f);
            }

            debugDisplay.PushPremultipliedMatrix(visualization.m_transform);
            debugDisplay.DrawWireCylinder(
                AZ::Vector3::CreateZero(),
                AZ::Vector3::CreateAxisY(),
                wheel.m_radius,
                wheel.m_width);
            debugDisplay.PopPremultipliedMatrix();
        }

        void AddWheelBounds(
            AZ::Aabb& bounds,
            const WheelConfiguration& wheel,
            const WheelVisualization& visualization)
        {
            bounds.AddPoint(visualization.m_minimumSuspensionPoint);
            bounds.AddPoint(visualization.m_maximumSuspensionPoint);
            if (wheel.m_enableSuspensionForcePoint)
            {
                bounds.AddPoint(visualization.m_attachmentPoint);
                bounds.AddPoint(visualization.m_forcePoint);
            }

            const AZ::Vector3 minimum(-wheel.m_radius, -0.5f * wheel.m_width, -wheel.m_radius);
            const AZ::Vector3 maximum(wheel.m_radius, 0.5f * wheel.m_width, wheel.m_radius);
            for (AZ::u32 corner = 0; corner < 8; ++corner)
            {
                AZ::Vector3 point = minimum;
                if ((corner & 1) != 0)
                {
                    point.SetX(maximum.GetX());
                }
                if ((corner & 2) != 0)
                {
                    point.SetY(maximum.GetY());
                }
                if ((corner & 4) != 0)
                {
                    point.SetZ(maximum.GetZ());
                }
                bounds.AddPoint(visualization.m_transform.TransformPoint(point));
            }
        }

        template<class Wheels>
        void DrawWheels(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const Wheels& wheels,
            const AZ::Matrix3x4& entityTransform)
        {
            for (const auto& typedWheel : wheels)
            {
                const WheelConfiguration& wheel = GetCommonWheel(typedWheel);
                WheelVisualization visualization;
                if (BuildWheelVisualization(wheel, entityTransform, visualization))
                {
                    DrawWheel(debugDisplay, wheel, visualization);
                }
            }
        }

        template<class Wheels>
        [[nodiscard]]
        AZ::Aabb CalculateWheelBounds(
            const Wheels& wheels,
            const AZ::Matrix3x4& entityTransform)
        {
            AZ::Aabb bounds = AZ::Aabb::CreateNull();
            for (const auto& typedWheel : wheels)
            {
                const WheelConfiguration& wheel = GetCommonWheel(typedWheel);
                WheelVisualization visualization;
                if (BuildWheelVisualization(wheel, entityTransform, visualization))
                {
                    AddWheelBounds(bounds, wheel, visualization);
                }
            }
            return bounds;
        }

        void DrawTracks(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const TrackedVehicleConfiguration& configuration,
            const AZ::Matrix3x4& entityTransform)
        {
            for (const VehicleTrackConfiguration& track : configuration.m_tracks)
            {
                if (track.m_wheels.size() < 2)
                {
                    continue;
                }

                AZ::u32 previousWheelIndex = track.m_wheels.back();
                for (const AZ::u32 wheelIndex : track.m_wheels)
                {
                    if (previousWheelIndex < configuration.m_wheels.size()
                        && wheelIndex < configuration.m_wheels.size())
                    {
                        WheelVisualization previousVisualization;
                        WheelVisualization visualization;
                        if (BuildWheelVisualization(
                                configuration.m_wheels[previousWheelIndex].m_common,
                                entityTransform,
                                previousVisualization)
                            && BuildWheelVisualization(
                                configuration.m_wheels[wheelIndex].m_common,
                                entityTransform,
                                visualization))
                        {
                            debugDisplay.DrawLine(
                                previousVisualization.m_maximumSuspensionPoint,
                                visualization.m_maximumSuspensionPoint);
                        }
                    }
                    previousWheelIndex = wheelIndex;
                }
            }
        }
    } // namespace

    WheeledVehicleComponent::WheeledVehicleComponent() = default;

    WheeledVehicleComponent::WheeledVehicleComponent(
        WheeledVehicleComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void WheeledVehicleComponent::Reflect(
        AZ::ReflectContext* context)
    {
        Jolt::WheeledVehicleComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<WheeledVehicleComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &WheeledVehicleComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<WheeledVehicleComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Wheeled Vehicle"),
                        QT_TRANSLATE_NOOP("Jolt", "Drives a rigid body using wheels, suspension, and a powertrain."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &WheeledVehicleComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Wheel, suspension, collision, engine, and transmission settings."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void WheeledVehicleComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::WheeledVehicleComponent::GetProvidedServices(provided);
    }

    void WheeledVehicleComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::WheeledVehicleComponent::GetIncompatibleServices(incompatible);
    }

    void WheeledVehicleComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::WheeledVehicleComponent::GetRequiredServices(required);
    }

    void WheeledVehicleComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void WheeledVehicleComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void WheeledVehicleComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::WheeledVehicleComponent>(m_configuration);
    }

    void WheeledVehicleComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        DrawWheels(
            debugDisplay,
            m_configuration.m_vehicle.m_wheels,
            AZ::Matrix3x4::CreateFromTransform(GetWorldTM()));
    }

    bool WheeledVehicleComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb WheeledVehicleComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        return CalculateWheelBounds(
            m_configuration.m_vehicle.m_wheels,
            AZ::Matrix3x4::CreateFromTransform(GetWorldTM()));
    }

    bool WheeledVehicleComponent::EditorSelectionIntersectRayViewport(
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

    MotorcycleComponent::MotorcycleComponent() = default;

    MotorcycleComponent::MotorcycleComponent(
        MotorcycleComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void MotorcycleComponent::Reflect(
        AZ::ReflectContext* context)
    {
        Jolt::MotorcycleComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<MotorcycleComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &MotorcycleComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<MotorcycleComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Motorcycle"),
                        QT_TRANSLATE_NOOP("Jolt", "Drives a two-wheeled rigid body with an optional lean controller."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &MotorcycleComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Wheel, powertrain, collision, and lean-controller settings."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void MotorcycleComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::MotorcycleComponent::GetProvidedServices(provided);
    }

    void MotorcycleComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::MotorcycleComponent::GetIncompatibleServices(incompatible);
    }

    void MotorcycleComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::MotorcycleComponent::GetRequiredServices(required);
    }

    void MotorcycleComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void MotorcycleComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void MotorcycleComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::MotorcycleComponent>(m_configuration);
    }

    void MotorcycleComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        DrawWheels(
            debugDisplay,
            m_configuration.m_motorcycle.m_wheeled.m_wheels,
            AZ::Matrix3x4::CreateFromTransform(GetWorldTM()));
    }

    bool MotorcycleComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb MotorcycleComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        return CalculateWheelBounds(
            m_configuration.m_motorcycle.m_wheeled.m_wheels,
            AZ::Matrix3x4::CreateFromTransform(GetWorldTM()));
    }

    bool MotorcycleComponent::EditorSelectionIntersectRayViewport(
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

    TrackedVehicleComponent::TrackedVehicleComponent() = default;

    TrackedVehicleComponent::TrackedVehicleComponent(
        TrackedVehicleComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void TrackedVehicleComponent::Reflect(
        AZ::ReflectContext* context)
    {
        Jolt::TrackedVehicleComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<TrackedVehicleComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &TrackedVehicleComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<TrackedVehicleComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Tracked Vehicle"),
                        QT_TRANSLATE_NOOP("Jolt", "Drives a rigid body using two independently controlled tracks."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &TrackedVehicleComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Track, wheel, suspension, collision, engine, and transmission settings."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void TrackedVehicleComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::TrackedVehicleComponent::GetProvidedServices(provided);
    }

    void TrackedVehicleComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::TrackedVehicleComponent::GetIncompatibleServices(incompatible);
    }

    void TrackedVehicleComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::TrackedVehicleComponent::GetRequiredServices(required);
    }

    void TrackedVehicleComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void TrackedVehicleComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void TrackedVehicleComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::TrackedVehicleComponent>(m_configuration);
    }

    void TrackedVehicleComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        const AZ::Matrix3x4 entityTransform = AZ::Matrix3x4::CreateFromTransform(GetWorldTM());
        DrawWheels(debugDisplay, m_configuration.m_vehicle.m_wheels, entityTransform);
        DrawTracks(debugDisplay, m_configuration.m_vehicle, entityTransform);
    }

    bool TrackedVehicleComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb TrackedVehicleComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        return CalculateWheelBounds(
            m_configuration.m_vehicle.m_wheels,
            AZ::Matrix3x4::CreateFromTransform(GetWorldTM()));
    }

    bool TrackedVehicleComponent::EditorSelectionIntersectRayViewport(
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
