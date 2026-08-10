/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/StaticRigidBodyComponent.h>

#include <Box3D/ColliderComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Box3D
{
    StaticRigidBodyComponent::StaticRigidBodyComponent(
        AZ::Name worldName)
        : m_worldName(AZStd::move(worldName))
    {
    }

    void StaticRigidBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StaticRigidBodyComponent, AZ::Component>()->Field("WorldName", &StaticRigidBodyComponent::m_worldName);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<StaticRigidBodyComponent>("Box3D Static Rigid Body", "Creates a static Box3D body")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticRigidBodyComponent::m_worldName, "World", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<StaticRigidBodyComponent>("Box3D::StaticRigidBodyComponent")->RequestBus("Box3DRigidBodyRequestBus");
        }
    }

    void StaticRigidBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DBodyService"));
    }

    void StaticRigidBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DBodyService"));
        incompatible.push_back(AZ_CRC_CE("Box3DCharacterService"));
    }

    void StaticRigidBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
        required.push_back(AZ_CRC_CE("Box3DColliderService"));
    }

    bool StaticRigidBodyComponent::EnableSimulation()
    {
        if (m_bodyHandle.IsValid())
        {
            return true;
        }
        if (!m_system || !m_collider)
        {
            return false;
        }

        m_worldHandle = m_system->GetDefaultWorldHandle();
        if (!m_worldName.IsEmpty())
        {
            m_worldHandle = m_system->FindWorld(m_worldName);
        }
        if (!m_worldHandle.IsValid())
        {
            return false;
        }

        RigidBodyConfiguration configuration;
        AZ::TransformBus::EventResult(configuration.m_transform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        m_uniformScale = configuration.m_transform.ExtractUniformScale();
        configuration.m_entityId = GetEntityId();
        configuration.m_name = AZ::Name(GetEntity()->GetName());
        configuration.m_bodyType = BodyType::Static;
        configuration.m_enableSleep = false;
        configuration.m_startAwake = false;
        m_bodyHandle = m_system->CreateBody(m_worldHandle, configuration);
        if (!m_bodyHandle.IsValid())
        {
            return false;
        }
        if (!m_collider->Attach(*m_system, m_worldHandle, m_bodyHandle, m_uniformScale))
        {
            m_system->DestroyBody(m_worldHandle, m_bodyHandle);
            m_bodyHandle = {};
            return false;
        }

        RigidBodyNotificationBus::Event(GetEntityId(), &RigidBodyNotifications::OnBodyCreated, m_worldHandle, m_bodyHandle);
        return true;
    }

    bool StaticRigidBodyComponent::DisableSimulation()
    {
        if (!m_bodyHandle.IsValid())
        {
            return true;
        }
        const BodyHandle bodyHandle = m_bodyHandle;
        m_collider->Detach();
        const bool destroyed = m_system->DestroyBody(m_worldHandle, bodyHandle);
        if (destroyed)
        {
            m_bodyHandle = {};
            m_uniformScale = 1.0f;
            RigidBodyNotificationBus::Event(GetEntityId(), &RigidBodyNotifications::OnBodyDestroyed, m_worldHandle, bodyHandle);
        }
        return destroyed;
    }

    bool StaticRigidBodyComponent::IsSimulationEnabled() const
    {
        return m_system && m_bodyHandle.IsValid();
    }

    WorldHandle StaticRigidBodyComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle StaticRigidBodyComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    BodyState StaticRigidBodyComponent::GetState() const
    {
        BodyState state;
        if (m_system)
        {
            [[maybe_unused]] const bool stateFound = m_system->GetBodyState(m_worldHandle, m_bodyHandle, state);
        }
        return state;
    }

    AZ::Name StaticRigidBodyComponent::GetName() const
    {
        if (m_system && m_bodyHandle.IsValid())
        {
            return m_system->GetBodyName(m_worldHandle, m_bodyHandle);
        }
        if (GetEntity())
        {
            return AZ::Name(GetEntity()->GetName());
        }

        return {};
    }

    bool StaticRigidBodyComponent::SetName(
        const AZ::Name name)
    {
        return m_system && m_bodyHandle.IsValid() && m_system->SetBodyName(m_worldHandle, m_bodyHandle, name);
    }

    BodyProperties StaticRigidBodyComponent::GetProperties() const
    {
        BodyProperties properties;
        properties.m_bodyType = BodyType::Static;
        if (m_system)
        {
            [[maybe_unused]] const bool propertiesFound = m_system->GetBodyProperties(m_worldHandle, m_bodyHandle, properties);
        }
        return properties;
    }

    bool StaticRigidBodyComponent::SetProperties(
        const BodyProperties& properties)
    {
        BodyProperties staticProperties = properties;
        staticProperties.m_bodyType = BodyType::Static;
        return m_system && m_system->SetBodyProperties(m_worldHandle, m_bodyHandle, staticProperties);
    }

    AZ::Aabb StaticRigidBodyComponent::GetAabb() const
    {
        if (m_system)
        {
            return m_system->GetBodyAabb(m_worldHandle, m_bodyHandle);
        }

        return AZ::Aabb::CreateNull();
    }

    ClosestPoint StaticRigidBodyComponent::GetClosestPoint(
        const AZ::Vector3& target) const
    {
        ClosestPoint result;
        if (m_system)
        {
            result.m_found = m_system->GetBodyClosestPoint(m_worldHandle, m_bodyHandle, target, result.m_position, result.m_distance);
        }
        return result;
    }

    MassProperties StaticRigidBodyComponent::GetMassProperties() const
    {
        MassProperties properties;
        if (m_system)
        {
            [[maybe_unused]] const bool propertiesFound = m_system->GetMassProperties(m_worldHandle, m_bodyHandle, properties);
        }
        return properties;
    }

    bool StaticRigidBodyComponent::SetMassProperties(
        const MassProperties& properties)
    {
        return m_system && m_system->SetMassProperties(m_worldHandle, m_bodyHandle, properties);
    }

    bool StaticRigidBodyComponent::RecomputeMassFromShapes()
    {
        return m_system && m_system->RecomputeMassFromShapes(m_worldHandle, m_bodyHandle);
    }

    bool StaticRigidBodyComponent::SetTransform(
        const AZ::Transform& transform)
    {
        if (!m_system || !m_collider)
        {
            return false;
        }

        AZ::Transform pose = transform;
        const float uniformScale = pose.ExtractUniformScale();
        if (!m_collider->UpdateUniformScale(uniformScale))
        {
            return false;
        }
        if (!m_system->SetBodyTransform(m_worldHandle, m_bodyHandle, pose))
        {
            [[maybe_unused]] const bool restored = m_collider->UpdateUniformScale(m_uniformScale);
            return false;
        }
        m_uniformScale = uniformScale;
        return true;
    }

    bool StaticRigidBodyComponent::SetLinearVelocity(
        const AZ::Vector3& velocity)
    {
        return m_system && m_system->SetLinearVelocity(m_worldHandle, m_bodyHandle, velocity);
    }

    bool StaticRigidBodyComponent::SetAngularVelocity(
        const AZ::Vector3& velocity)
    {
        return m_system && m_system->SetAngularVelocity(m_worldHandle, m_bodyHandle, velocity);
    }

    AZ::Vector3 StaticRigidBodyComponent::GetLinearVelocityAtWorldPoint(
        const AZ::Vector3& worldPoint) const
    {
        if (m_system)
        {
            return m_system->GetLinearVelocityAtWorldPoint(m_worldHandle, m_bodyHandle, worldPoint);
        }

        return AZ::Vector3::CreateZero();
    }

    bool StaticRigidBodyComponent::SetKinematicTarget(
        const AZ::Transform&,
        float)
    {
        return false;
    }

    bool StaticRigidBodyComponent::ApplyLinearImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->ApplyLinearImpulse(m_worldHandle, m_bodyHandle, impulse);
    }

    bool StaticRigidBodyComponent::ApplyLinearImpulseAtWorldPoint(
        const AZ::Vector3& impulse,
        const AZ::Vector3& worldPoint)
    {
        return m_system && m_system->ApplyLinearImpulseAtWorldPoint(m_worldHandle, m_bodyHandle, impulse, worldPoint);
    }

    bool StaticRigidBodyComponent::ApplyAngularImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->ApplyAngularImpulse(m_worldHandle, m_bodyHandle, impulse);
    }

    bool StaticRigidBodyComponent::ApplyForce(
        const AZ::Vector3& force,
        const bool wake)
    {
        return m_system && m_system->ApplyForce(m_worldHandle, m_bodyHandle, force, wake);
    }

    bool StaticRigidBodyComponent::ApplyForceAtWorldPoint(
        const AZ::Vector3& force,
        const AZ::Vector3& worldPoint,
        const bool wake)
    {
        return m_system && m_system->ApplyForceAtWorldPoint(m_worldHandle, m_bodyHandle, force, worldPoint, wake);
    }

    bool StaticRigidBodyComponent::ApplyTorque(
        const AZ::Vector3& torque,
        const bool wake)
    {
        return m_system && m_system->ApplyTorque(m_worldHandle, m_bodyHandle, torque, wake);
    }

    bool StaticRigidBodyComponent::SetAwake(
        const bool awake)
    {
        return m_system && m_system->SetBodyAwake(m_worldHandle, m_bodyHandle, awake);
    }

    bool StaticRigidBodyComponent::SetHitEventsEnabled(
        const bool enabled)
    {
        return m_system && m_system->SetBodyHitEventsEnabled(m_worldHandle, m_bodyHandle, enabled);
    }

    void StaticRigidBodyComponent::Activate()
    {
        m_system = AZ::Interface<ISystem>::Get();
        m_collider = GetEntity()->FindComponent<ColliderComponent>();
        AZ_Error("Box3D", m_system, "Box3D SystemComponent must be active before body components.");
        AZ_Error("Box3D", m_collider, "Box3D StaticRigidBodyComponent requires a ColliderComponent.");
        RigidBodyRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        EnableSimulation();
    }

    void StaticRigidBodyComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        RigidBodyRequestBus::Handler::BusDisconnect();
        DisableSimulation();
        m_collider = nullptr;
        m_system = nullptr;
        m_worldHandle = {};
    }

    void StaticRigidBodyComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (m_system && m_bodyHandle.IsValid())
        {
            [[maybe_unused]] const bool transformSet = SetTransform(world);
        }
    }
} // namespace Box3D
