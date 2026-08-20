/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/StaticRigidBodyComponent.h>

#include <Jolt/ColliderComponent.h>
#include <Jolt/ComponentDependencyManager.h>
#include <Jolt/ComponentUtilities.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    StaticRigidBodyComponent::StaticRigidBodyComponent(
        StaticRigidBodyConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void StaticRigidBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        StaticRigidBodyConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<StaticRigidBodyComponent, AZ::Component>()
                ->Field("Configuration", &StaticRigidBodyComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<StaticRigidBodyComponent>(
                        "Jolt Static Rigid Body",
                        "Simulates an entity as a static Jolt rigid body.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &StaticRigidBodyComponent::m_configuration,
                        "Configuration",
                        "Static body properties.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<StaticRigidBodyRequestBus>("JoltStaticRigidBodyRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &IStaticRigidBodyRequests::EnableSimulation)
                ->Event("DisableSimulation", &IStaticRigidBodyRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &IStaticRigidBodyRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IStaticRigidBodyRequests::GetWorldHandle)
                ->Event("GetBodyHandle", &IStaticRigidBodyRequests::GetBodyHandle)
                ->Event("GetCenterOfMassTransform", &IStaticRigidBodyRequests::GetCenterOfMassTransform)
                ->Event("GetState", &IStaticRigidBodyRequests::GetState);

            behaviorContext->Class<StaticRigidBodyComponent>("Jolt::StaticRigidBodyComponent")
                ->RequestBus("JoltStaticRigidBodyRequestBus");
        }
    }

    void StaticRigidBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltBodyService"));
    }

    void StaticRigidBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltBodyService"));
    }

    void StaticRigidBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
        required.push_back(AZ_CRC_CE("JoltColliderService"));
    }

    bool StaticRigidBodyComponent::EnableSimulation()
    {
        if (m_bodyHandle)
        {
            return true;
        }
        if (!m_system || !m_collider)
        {
            return false;
        }

        m_worldHandle = m_system->GetDefaultWorldHandle();
        AZ::Transform entityTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(entityTransform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        m_uniformScale = entityTransform.GetUniformScale();
        if (!m_collider->CreateShapes(*m_system, m_worldHandle, m_uniformScale))
        {
            return false;
        }

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = m_collider->GetRootShapeHandle();
        bodyConfiguration.m_transform = Internal::ToWorldTransform(entityTransform);
        bodyConfiguration.m_entityId = GetEntityId();
        bodyConfiguration.m_name = AZ::Name(GetEntity()->GetName());
        bodyConfiguration.m_userData = m_configuration.m_userData;
        bodyConfiguration.m_collisionGroup = m_configuration.m_collisionGroup;
        bodyConfiguration.m_objectLayer = m_configuration.m_objectLayer;
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_friction = m_configuration.m_friction;
        bodyConfiguration.m_restitution = m_configuration.m_restitution;
        bodyConfiguration.m_enhancedInternalEdgeRemoval = m_configuration.m_enhancedInternalEdgeRemoval;
        bodyConfiguration.m_isSensor = m_configuration.m_isSensor;
        bodyConfiguration.m_useManifoldReduction = m_configuration.m_useManifoldReduction;
        bodyConfiguration.m_activate = false;
        m_bodyHandle = m_system->CreateBody(m_worldHandle, bodyConfiguration);
        if (!m_bodyHandle)
        {
            m_collider->DestroyShapes();
            return false;
        }
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyCreated,
            m_worldHandle,
            m_bodyHandle);
        return true;
    }

    bool StaticRigidBodyComponent::DisableSimulation()
    {
        if (!m_bodyHandle)
        {
            return true;
        }
        const BodyHandle bodyHandle = m_bodyHandle;
        auto* dependencyManager = AZ::Interface<IComponentDependencyManager>::Get();
        if (dependencyManager
            && !dependencyManager->PrepareBodyDestruction(GetEntityId(), m_worldHandle, bodyHandle))
        {
            return false;
        }
        if (!m_system->DestroyBody(m_worldHandle, bodyHandle))
        {
            return false;
        }

        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyDestroying,
            m_worldHandle,
            bodyHandle);
        m_bodyHandle = {};
        m_collider->DestroyShapes();
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyDestroyed,
            m_worldHandle,
            bodyHandle);
        return true;
    }

    bool StaticRigidBodyComponent::IsSimulationEnabled() const
    {
        return m_system && m_bodyHandle;
    }

    WorldHandle StaticRigidBodyComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle StaticRigidBodyComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    AZ::u64 StaticRigidBodyComponent::GetUserData() const
    {
        AZ::u64 userData = m_configuration.m_userData;
        if (m_system && m_bodyHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyUserData(m_worldHandle, m_bodyHandle, userData);
        }
        return userData;
    }

    bool StaticRigidBodyComponent::SetUserData(
        const AZ::u64 userData)
    {
        if (m_bodyHandle
            && (!m_system || !m_system->SetBodyUserData(m_worldHandle, m_bodyHandle, userData)))
        {
            return false;
        }

        m_configuration.m_userData = userData;
        return true;
    }

    WorldTransform StaticRigidBodyComponent::GetCenterOfMassTransform() const
    {
        WorldTransform transform;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyCenterOfMassTransform(m_worldHandle, m_bodyHandle, transform);
        }
        return transform;
    }

    BodyState StaticRigidBodyComponent::GetState() const
    {
        BodyState state;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetBodyState(m_worldHandle, m_bodyHandle, state);
        }
        return state;
    }

    void StaticRigidBodyComponent::Activate()
    {
        m_system = GetRuntime();
        m_collider = GetEntity()->FindComponent<ColliderComponent>();
        StaticRigidBodyRequestBus::Handler::BusConnect(GetEntityId());
        BodyRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    void StaticRigidBodyComponent::Deactivate()
    {
        [[maybe_unused]] const bool disabled = DisableSimulation();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        BodyRequestBus::Handler::BusDisconnect();
        StaticRigidBodyRequestBus::Handler::BusDisconnect();
        m_collider = nullptr;
        m_system = nullptr;
    }

    void StaticRigidBodyComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (!m_system || !m_bodyHandle)
        {
            return;
        }

        const float uniformScale = world.GetUniformScale();
        if (!m_collider->UpdateUniformScale(m_bodyHandle, uniformScale))
        {
            return;
        }
        m_uniformScale = uniformScale;
        [[maybe_unused]] const bool updated = m_system->SetBodyTransform(
            m_worldHandle,
            m_bodyHandle,
            Internal::ToWorldTransform(world),
            false);
    }
} // namespace Jolt
