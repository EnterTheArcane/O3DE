/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/RigidBodyComponent.h>

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
    class RigidBodyNotificationBusBehaviorHandler final
        : public RigidBodyNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            RigidBodyNotificationBusBehaviorHandler,
            "{207AA14F-D44D-4CD4-A2B8-AE3970D85CC6}",
            AZ::SystemAllocator,
            OnBodyCreated,
            OnBodyDestroyed);

        void OnBodyCreated(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override
        {
            Call(FN_OnBodyCreated, worldHandle, bodyHandle);
        }

        void OnBodyDestroyed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override
        {
            Call(FN_OnBodyDestroyed, worldHandle, bodyHandle);
        }
    };

    RigidBodyComponent::RigidBodyComponent(
        RigidBodyConfiguration configuration,
        AZ::Name worldName)
        : m_configuration(AZStd::move(configuration))
        , m_worldName(AZStd::move(worldName))
    {
    }

    void RigidBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<RigidBodyComponent, AZ::Component>()
                ->Field("Configuration", &RigidBodyComponent::m_configuration)
                ->Field("WorldName", &RigidBodyComponent::m_worldName);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<RigidBodyComponent>("Box3D Rigid Body", "Simulates an entity in a Box3D world")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyComponent::m_worldName, "World", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyComponent::m_configuration, "Configuration", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Enum<static_cast<AZ::u8>(BodyType::Static)>("BodyType_Static")
                ->Enum<static_cast<AZ::u8>(BodyType::Kinematic)>("BodyType_Kinematic")
                ->Enum<static_cast<AZ::u8>(BodyType::Dynamic)>("BodyType_Dynamic");

            behaviorContext->Class<MotionLocks>("MotionLocks")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("linearX", BehaviorValueProperty(&MotionLocks::m_linearX))
                ->Property("linearY", BehaviorValueProperty(&MotionLocks::m_linearY))
                ->Property("linearZ", BehaviorValueProperty(&MotionLocks::m_linearZ))
                ->Property("angularX", BehaviorValueProperty(&MotionLocks::m_angularX))
                ->Property("angularY", BehaviorValueProperty(&MotionLocks::m_angularY))
                ->Property("angularZ", BehaviorValueProperty(&MotionLocks::m_angularZ));

            behaviorContext->Class<BodyState>("BodyState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("transform", BehaviorValueProperty(&BodyState::m_transform))
                ->Property("linearVelocity", BehaviorValueProperty(&BodyState::m_linearVelocity))
                ->Property("angularVelocity", BehaviorValueProperty(&BodyState::m_angularVelocity))
                ->Property("entityId", BehaviorValueProperty(&BodyState::m_entityId))
                ->Property("name", BehaviorValueProperty(&BodyState::m_name))
                ->Property("bodyType", BehaviorValueProperty(&BodyState::m_bodyType))
                ->Property("isAwake", BehaviorValueProperty(&BodyState::m_isAwake))
                ->Property("isEnabled", BehaviorValueProperty(&BodyState::m_isEnabled))
                ->Property("allowFastRotation", BehaviorValueProperty(&BodyState::m_allowFastRotation));

            behaviorContext->Class<BodyProperties>("BodyProperties")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("motionLocks", BehaviorValueProperty(&BodyProperties::m_motionLocks))
                ->Property("bodyType", BehaviorValueProperty(&BodyProperties::m_bodyType))
                ->Property("linearDamping", BehaviorValueProperty(&BodyProperties::m_linearDamping))
                ->Property("angularDamping", BehaviorValueProperty(&BodyProperties::m_angularDamping))
                ->Property("gravityScale", BehaviorValueProperty(&BodyProperties::m_gravityScale))
                ->Property("sleepThreshold", BehaviorValueProperty(&BodyProperties::m_sleepThreshold))
                ->Property("isBullet", BehaviorValueProperty(&BodyProperties::m_isBullet))
                ->Property("enableSleep", BehaviorValueProperty(&BodyProperties::m_enableSleep))
                ->Property("enableContactRecycling", BehaviorValueProperty(&BodyProperties::m_enableContactRecycling));

            behaviorContext->Class<MassProperties>("MassProperties")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("inertia", BehaviorValueProperty(&MassProperties::m_inertia))
                ->Property("center", BehaviorValueProperty(&MassProperties::m_center))
                ->Property("mass", BehaviorValueProperty(&MassProperties::m_mass));

            behaviorContext->Class<ClosestPoint>("ClosestPoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("position", BehaviorValueProperty(&ClosestPoint::m_position))
                ->Property("distance", BehaviorValueProperty(&ClosestPoint::m_distance))
                ->Property("found", BehaviorValueProperty(&ClosestPoint::m_found));

            behaviorContext->EBus<RigidBodyRequestBus>("Box3DRigidBodyRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("EnableSimulation", &RigidBodyRequests::EnableSimulation)
                ->Event("DisableSimulation", &RigidBodyRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &RigidBodyRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &RigidBodyRequests::GetWorldHandle)
                ->Event("GetBodyHandle", &RigidBodyRequests::GetBodyHandle)
                ->Event("GetState", &RigidBodyRequests::GetState)
                ->Event("GetName", &RigidBodyRequests::GetName)
                ->Event("SetName", &RigidBodyRequests::SetName)
                ->Event("GetProperties", &RigidBodyRequests::GetProperties)
                ->Event("SetProperties", &RigidBodyRequests::SetProperties)
                ->Event("GetAabb", &RigidBodyRequests::GetAabb)
                ->Event("GetClosestPoint", &RigidBodyRequests::GetClosestPoint)
                ->Event("GetMassProperties", &RigidBodyRequests::GetMassProperties)
                ->Event("SetMassProperties", &RigidBodyRequests::SetMassProperties)
                ->Event("RecomputeMassFromShapes", &RigidBodyRequests::RecomputeMassFromShapes)
                ->Event("SetTransform", &RigidBodyRequests::SetTransform)
                ->Event("SetLinearVelocity", &RigidBodyRequests::SetLinearVelocity)
                ->Event("SetAngularVelocity", &RigidBodyRequests::SetAngularVelocity)
                ->Event("GetLinearVelocityAtWorldPoint", &RigidBodyRequests::GetLinearVelocityAtWorldPoint)
                ->Event("SetKinematicTarget", &RigidBodyRequests::SetKinematicTarget)
                ->Event("ApplyLinearImpulse", &RigidBodyRequests::ApplyLinearImpulse)
                ->Event("ApplyLinearImpulseAtWorldPoint", &RigidBodyRequests::ApplyLinearImpulseAtWorldPoint)
                ->Event("ApplyAngularImpulse", &RigidBodyRequests::ApplyAngularImpulse)
                ->Event("ApplyForce", &RigidBodyRequests::ApplyForce)
                ->Event("ApplyForceAtWorldPoint", &RigidBodyRequests::ApplyForceAtWorldPoint)
                ->Event("ApplyTorque", &RigidBodyRequests::ApplyTorque)
                ->Event("SetAwake", &RigidBodyRequests::SetAwake)
                ->Event("SetHitEventsEnabled", &RigidBodyRequests::SetHitEventsEnabled);

            behaviorContext->EBus<RigidBodyNotificationBus>("Box3DRigidBodyNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Handler<RigidBodyNotificationBusBehaviorHandler>();

            behaviorContext->Class<RigidBodyComponent>("Box3D::RigidBodyComponent")->RequestBus("Box3DRigidBodyRequestBus");
        }
    }

    void RigidBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DBodyService"));
    }

    void RigidBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DBodyService"));
        incompatible.push_back(AZ_CRC_CE("Box3DCharacterService"));
    }

    void RigidBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
        required.push_back(AZ_CRC_CE("Box3DColliderService"));
    }

    bool RigidBodyComponent::EnableSimulation()
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

        AZ::TransformBus::EventResult(m_configuration.m_transform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        m_uniformScale = m_configuration.m_transform.ExtractUniformScale();
        m_configuration.m_entityId = GetEntityId();
        m_configuration.m_name = AZ::Name(GetEntity()->GetName());
        m_bodyHandle = m_system->CreateBody(m_worldHandle, m_configuration);
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

    bool RigidBodyComponent::DisableSimulation()
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

    bool RigidBodyComponent::IsSimulationEnabled() const
    {
        return m_system && m_bodyHandle.IsValid();
    }

    WorldHandle RigidBodyComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle RigidBodyComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    BodyState RigidBodyComponent::GetState() const
    {
        BodyState state;
        if (m_system)
        {
            [[maybe_unused]] const bool stateFound = m_system->GetBodyState(m_worldHandle, m_bodyHandle, state);
        }
        return state;
    }

    AZ::Name RigidBodyComponent::GetName() const
    {
        if (m_system && m_bodyHandle)
        {
            return m_system->GetBodyName(m_worldHandle, m_bodyHandle);
        }

        return m_configuration.m_name;
    }

    bool RigidBodyComponent::SetName(
        const AZ::Name name)
    {
        if (m_system && m_bodyHandle.IsValid() && !m_system->SetBodyName(m_worldHandle, m_bodyHandle, name))
        {
            return false;
        }
        m_configuration.m_name = name;
        return true;
    }

    BodyProperties RigidBodyComponent::GetProperties() const
    {
        BodyProperties properties;
        if (m_system && m_system->GetBodyProperties(m_worldHandle, m_bodyHandle, properties))
        {
            return properties;
        }
        properties.m_motionLocks = m_configuration.m_motionLocks;
        properties.m_bodyType = m_configuration.m_bodyType;
        properties.m_linearDamping = m_configuration.m_linearDamping;
        properties.m_angularDamping = m_configuration.m_angularDamping;
        properties.m_gravityScale = m_configuration.m_gravityScale;
        properties.m_sleepThreshold = m_configuration.m_sleepThreshold;
        properties.m_isBullet = m_configuration.m_isBullet;
        properties.m_enableSleep = m_configuration.m_enableSleep;
        properties.m_enableContactRecycling = m_configuration.m_enableContactRecycling;
        return properties;
    }

    bool RigidBodyComponent::SetProperties(
        const BodyProperties& properties)
    {
        if (m_system && m_bodyHandle.IsValid() && !m_system->SetBodyProperties(m_worldHandle, m_bodyHandle, properties))
        {
            return false;
        }
        m_configuration.m_motionLocks = properties.m_motionLocks;
        m_configuration.m_bodyType = properties.m_bodyType;
        m_configuration.m_linearDamping = properties.m_linearDamping;
        m_configuration.m_angularDamping = properties.m_angularDamping;
        m_configuration.m_gravityScale = properties.m_gravityScale;
        m_configuration.m_sleepThreshold = properties.m_sleepThreshold;
        m_configuration.m_isBullet = properties.m_isBullet;
        m_configuration.m_enableSleep = properties.m_enableSleep;
        m_configuration.m_enableContactRecycling = properties.m_enableContactRecycling;
        return true;
    }

    AZ::Aabb RigidBodyComponent::GetAabb() const
    {
        if (m_system)
        {
            return m_system->GetBodyAabb(m_worldHandle, m_bodyHandle);
        }

        return AZ::Aabb::CreateNull();
    }

    ClosestPoint RigidBodyComponent::GetClosestPoint(
        const AZ::Vector3& target) const
    {
        ClosestPoint result;
        if (m_system)
        {
            result.m_found = m_system->GetBodyClosestPoint(m_worldHandle, m_bodyHandle, target, result.m_position, result.m_distance);
        }
        return result;
    }

    MassProperties RigidBodyComponent::GetMassProperties() const
    {
        MassProperties properties;
        if (m_system)
        {
            [[maybe_unused]] const bool propertiesFound = m_system->GetMassProperties(m_worldHandle, m_bodyHandle, properties);
        }
        return properties;
    }

    bool RigidBodyComponent::SetMassProperties(
        const MassProperties& properties)
    {
        return m_system && m_system->SetMassProperties(m_worldHandle, m_bodyHandle, properties);
    }

    bool RigidBodyComponent::RecomputeMassFromShapes()
    {
        return m_system && m_system->RecomputeMassFromShapes(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::SetTransform(
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

    bool RigidBodyComponent::SetLinearVelocity(
        const AZ::Vector3& velocity)
    {
        return m_system && m_system->SetLinearVelocity(m_worldHandle, m_bodyHandle, velocity);
    }

    bool RigidBodyComponent::SetAngularVelocity(
        const AZ::Vector3& velocity)
    {
        return m_system && m_system->SetAngularVelocity(m_worldHandle, m_bodyHandle, velocity);
    }

    AZ::Vector3 RigidBodyComponent::GetLinearVelocityAtWorldPoint(
        const AZ::Vector3& worldPoint) const
    {
        if (m_system)
        {
            return m_system->GetLinearVelocityAtWorldPoint(m_worldHandle, m_bodyHandle, worldPoint);
        }

        return AZ::Vector3::CreateZero();
    }

    bool RigidBodyComponent::SetKinematicTarget(
        const AZ::Transform& transform,
        const float fixedTimeStep)
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
        if (!m_system->SetKinematicTarget(m_worldHandle, m_bodyHandle, pose, fixedTimeStep))
        {
            [[maybe_unused]] const bool restored = m_collider->UpdateUniformScale(m_uniformScale);
            return false;
        }
        m_uniformScale = uniformScale;
        return true;
    }

    bool RigidBodyComponent::ApplyLinearImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->ApplyLinearImpulse(m_worldHandle, m_bodyHandle, impulse);
    }

    bool RigidBodyComponent::ApplyLinearImpulseAtWorldPoint(
        const AZ::Vector3& impulse,
        const AZ::Vector3& worldPoint)
    {
        return m_system && m_system->ApplyLinearImpulseAtWorldPoint(m_worldHandle, m_bodyHandle, impulse, worldPoint);
    }

    bool RigidBodyComponent::ApplyAngularImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->ApplyAngularImpulse(m_worldHandle, m_bodyHandle, impulse);
    }

    bool RigidBodyComponent::ApplyForce(
        const AZ::Vector3& force,
        const bool wake)
    {
        return m_system && m_system->ApplyForce(m_worldHandle, m_bodyHandle, force, wake);
    }

    bool RigidBodyComponent::ApplyForceAtWorldPoint(
        const AZ::Vector3& force,
        const AZ::Vector3& worldPoint,
        const bool wake)
    {
        return m_system && m_system->ApplyForceAtWorldPoint(m_worldHandle, m_bodyHandle, force, worldPoint, wake);
    }

    bool RigidBodyComponent::ApplyTorque(
        const AZ::Vector3& torque,
        const bool wake)
    {
        return m_system && m_system->ApplyTorque(m_worldHandle, m_bodyHandle, torque, wake);
    }

    bool RigidBodyComponent::SetAwake(
        const bool awake)
    {
        return m_system && m_system->SetBodyAwake(m_worldHandle, m_bodyHandle, awake);
    }

    bool RigidBodyComponent::SetHitEventsEnabled(
        const bool enabled)
    {
        return m_system && m_system->SetBodyHitEventsEnabled(m_worldHandle, m_bodyHandle, enabled);
    }

    void RigidBodyComponent::Activate()
    {
        m_system = AZ::Interface<ISystem>::Get();
        m_collider = GetEntity()->FindComponent<ColliderComponent>();
        AZ_Error("Box3D", m_system, "Box3D SystemComponent must be active before body components.");
        AZ_Error("Box3D", m_collider, "Box3D RigidBodyComponent requires a ColliderComponent.");
        RigidBodyRequestBus::Handler::BusConnect(GetEntityId());
        RigidBodyNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        EnableSimulation();
    }

    void RigidBodyComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        RigidBodyNotificationBus::Handler::BusDisconnect();
        RigidBodyRequestBus::Handler::BusDisconnect();
        DisableSimulation();
        m_collider = nullptr;
        m_system = nullptr;
        m_worldHandle = {};
    }

    void RigidBodyComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (!m_syncingTransform && m_system && m_bodyHandle.IsValid())
        {
            [[maybe_unused]] const bool transformSet = SetTransform(world);
        }
    }

    void RigidBodyComponent::OnBodyMoved(
        const BodyMoveEvent& event)
    {
        if (event.m_bodyHandle != m_bodyHandle)
        {
            return;
        }

        AZ::Transform currentTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(currentTransform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        if (currentTransform.GetTranslation().IsClose(event.m_transform.GetTranslation())
            && currentTransform.GetRotation().IsClose(event.m_transform.GetRotation()))
        {
            return;
        }

        m_syncingTransform = true;
        AZ::Transform transform = event.m_transform;
        transform.SetUniformScale(m_uniformScale);
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTM, transform);
        m_syncingTransform = false;
    }
} // namespace Box3D
