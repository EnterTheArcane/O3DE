/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CharacterControllerComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/ComponentUtilities.h>
#include <Jolt/Reflection.h>
#include <Jolt/System.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    CharacterControllerComponent::CharacterControllerComponent(
        CharacterComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void CharacterControllerComponent::Reflect(
        AZ::ReflectContext* context)
    {
        CharacterComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<CharacterControllerComponent, AZ::Component>()
                ->Field("Configuration", &CharacterControllerComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<CharacterControllerComponent>(
                        "Jolt Character Controller",
                        "Simulates an entity as a rigid character controller.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &CharacterControllerComponent::m_configuration,
                        "Configuration",
                        "Collision, movement, slope, and support properties.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, GroundState, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, GroundState, InAir);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, GroundState, NotSupported);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, GroundState, OnGround);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, GroundState, OnSteepGround);

            if (ShouldReflect(
                *behaviorContext,
                behaviorContext->m_classes.contains("SubShapeId")))
            {
                behaviorContext->Class<SubShapeId>("SubShapeId")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Method("GetValue", &SubShapeId::GetValue);
            }

            behaviorContext->Class<CharacterCollisionRequest>("CharacterCollisionRequest")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("transform", JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterCollisionRequest::m_transform))
                ->Property(
                    "movementDirection",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterCollisionRequest::m_movementDirection))
                ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterCollisionRequest::m_shapeHandle))
                ->Property(
                    "maximumSeparationDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterCollisionRequest::m_maximumSeparationDistance));

            behaviorContext->Class<CharacterCollisionHit>("CharacterCollisionHit")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "queryContactPosition",
                    BehaviorValueGetter(&CharacterCollisionHit::m_queryContactPosition),
                    nullptr)
                ->Property(
                    "targetContactPosition",
                    BehaviorValueGetter(&CharacterCollisionHit::m_targetContactPosition),
                    nullptr)
                ->Property(
                    "penetrationAxis",
                    BehaviorValueGetter(&CharacterCollisionHit::m_penetrationAxis),
                    nullptr)
                ->Property("bodyHandle", BehaviorValueGetter(&CharacterCollisionHit::m_bodyHandle), nullptr)
                ->Property(
                    "materialHandle",
                    BehaviorValueGetter(&CharacterCollisionHit::m_materialHandle),
                    nullptr)
                ->Property("shapeHandle", BehaviorValueGetter(&CharacterCollisionHit::m_shapeHandle), nullptr)
                ->Property(
                    "characterHandle",
                    BehaviorValueGetter(&CharacterCollisionHit::m_characterHandle),
                    nullptr)
                ->Property(
                    "virtualCharacterHandle",
                    BehaviorValueGetter(&CharacterCollisionHit::m_virtualCharacterHandle),
                    nullptr)
                ->Property(
                    "querySubShapeId",
                    BehaviorValueGetter(&CharacterCollisionHit::m_querySubShapeId),
                    nullptr)
                ->Property(
                    "targetSubShapeId",
                    BehaviorValueGetter(&CharacterCollisionHit::m_targetSubShapeId),
                    nullptr)
                ->Property(
                    "penetrationDepth",
                    BehaviorValueGetter(&CharacterCollisionHit::m_penetrationDepth),
                    nullptr);

            behaviorContext->Class<CharacterState>("JoltCharacterState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "CharacterState")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "CharacterState")
                ->Property("transform", BehaviorValueGetter(&CharacterState::m_transform), nullptr)
                ->Property("groundPosition", BehaviorValueGetter(&CharacterState::m_groundPosition), nullptr)
                ->Property("groundNormal", BehaviorValueGetter(&CharacterState::m_groundNormal), nullptr)
                ->Property("groundVelocity", BehaviorValueGetter(&CharacterState::m_groundVelocity), nullptr)
                ->Property("linearVelocity", BehaviorValueGetter(&CharacterState::m_linearVelocity), nullptr)
                ->Property("bodyHandle", BehaviorValueGetter(&CharacterState::m_bodyHandle), nullptr)
                ->Property("groundBodyHandle", BehaviorValueGetter(&CharacterState::m_groundBodyHandle), nullptr)
                ->Property("groundMaterialHandle", BehaviorValueGetter(&CharacterState::m_groundMaterialHandle), nullptr)
                ->Property("shapeHandle", BehaviorValueGetter(&CharacterState::m_shapeHandle), nullptr)
                ->Property("groundSubShapeId", BehaviorValueGetter(&CharacterState::m_groundSubShapeId), nullptr)
                ->Property("entityId", BehaviorValueGetter(&CharacterState::m_entityId), nullptr)
                ->Property("name", BehaviorValueGetter(&CharacterState::m_name), nullptr)
                ->Property("userData", BehaviorValueGetter(&CharacterState::m_userData), nullptr)
                ->Property("groundState", BehaviorValueGetter(&CharacterState::m_groundState), nullptr)
                ->Property("isSupported", BehaviorValueGetter(&CharacterState::m_isSupported), nullptr);

            behaviorContext->Class<CharacterRuntimeConfiguration>("CharacterRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "objectLayer",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterRuntimeConfiguration::m_objectLayer))
                ->Property(
                    "supportingPlaneNormal",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterRuntimeConfiguration::m_supportingPlaneNormal))
                ->Property("up", JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterRuntimeConfiguration::m_up))
                ->Property(
                    "maximumSeparationDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterRuntimeConfiguration::m_maximumSeparationDistance))
                ->Property(
                    "maximumSlopeAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterRuntimeConfiguration::m_maximumSlopeAngle))
                ->Property(
                    "supportingPlaneDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&CharacterRuntimeConfiguration::m_supportingPlaneDistance));

            behaviorContext->EBus<CharacterRequestBus>("JoltCharacterRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &ICharacterRequests::EnableSimulation)
                ->Event("DisableSimulation", &ICharacterRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &ICharacterRequests::IsSimulationEnabled)
                ->Event("GetCharacterHandle", &ICharacterRequests::GetCharacterHandle)
                ->Event("GetWorldHandle", &ICharacterRequests::GetWorldHandle)
                ->Event("GetCenterOfMassTransform", &ICharacterRequests::GetCenterOfMassTransform)
                ->Event("GetState", &ICharacterRequests::GetState)
                ->Event("GetRuntimeConfiguration", &ICharacterRequests::GetRuntimeConfiguration)
                ->Event("CheckCollision", &ICharacterRequests::CheckCollision)
                ->Event("UpdateRuntimeConfiguration", &ICharacterRequests::UpdateRuntimeConfiguration)
                ->Event("SetTransform", &ICharacterRequests::SetTransform)
                ->Event("SetVelocity", &ICharacterRequests::SetVelocity)
                ->Event("AddImpulse", &ICharacterRequests::AddImpulse);

            behaviorContext->Class<CharacterControllerComponent>("Jolt::CharacterControllerComponent")
                ->RequestBus("JoltCharacterRequestBus");
        }
    }

    void CharacterControllerComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltBodyService"));
        provided.push_back(AZ_CRC_CE("JoltCharacterService"));
    }

    void CharacterControllerComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltBodyService"));
        incompatible.push_back(AZ_CRC_CE("JoltCharacterService"));
    }

    void CharacterControllerComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
        required.push_back(AZ_CRC_CE("JoltColliderService"));
    }

    bool CharacterControllerComponent::EnableSimulation()
    {
        if (m_characterHandle)
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

        CharacterConfiguration configuration;
        configuration.m_shapeHandle = m_collider->GetRootShapeHandle();
        configuration.m_transform = Internal::ToWorldTransform(entityTransform);
        configuration.m_entityId = GetEntityId();
        configuration.m_name = AZ::Name(GetEntity()->GetName());
        configuration.m_userData = m_configuration.m_userData;
        configuration.m_collisionGroup = m_configuration.m_collisionGroup;
        configuration.m_objectLayer = m_configuration.m_objectLayer;
        configuration.m_allowedDofs = m_configuration.m_allowedDofs;
        configuration.m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal;
        configuration.m_up = m_configuration.m_up;
        configuration.m_friction = m_configuration.m_friction;
        configuration.m_gravityFactor = m_configuration.m_gravityFactor;
        configuration.m_mass = m_configuration.m_mass;
        configuration.m_maximumSeparationDistance = m_configuration.m_maximumSeparationDistance;
        configuration.m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle;
        configuration.m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance;
        configuration.m_activate = m_configuration.m_activate;
        configuration.m_enhancedInternalEdgeRemoval = m_configuration.m_enhancedInternalEdgeRemoval;

        m_characterHandle = m_system->CreateCharacter(m_worldHandle, configuration);
        if (!m_characterHandle)
        {
            m_collider->DestroyShapes();
            return false;
        }

        CharacterState state;
        if (!m_system->GetCharacterState(m_worldHandle, m_characterHandle, state)
            || !m_system->SetBodyMoveEventsEnabled(m_worldHandle, state.m_bodyHandle, true))
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroyCharacter(m_worldHandle, m_characterHandle);
            m_characterHandle = {};
            m_collider->DestroyShapes();
            return false;
        }
        m_bodyHandle = state.m_bodyHandle;
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyCreated,
            m_worldHandle,
            m_bodyHandle);
        return true;
    }

    bool CharacterControllerComponent::DisableSimulation()
    {
        if (!m_characterHandle)
        {
            return true;
        }
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyDestroying,
            m_worldHandle,
            m_bodyHandle);
        if (!m_system->DestroyCharacter(m_worldHandle, m_characterHandle))
        {
            return false;
        }

        const BodyHandle bodyHandle = m_bodyHandle;
        m_characterHandle = {};
        m_bodyHandle = {};
        m_collider->DestroyShapes();
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyDestroyed,
            m_worldHandle,
            bodyHandle);
        return true;
    }

    bool CharacterControllerComponent::IsSimulationEnabled() const
    {
        return m_system && m_characterHandle;
    }

    CharacterHandle CharacterControllerComponent::GetCharacterHandle() const
    {
        return m_characterHandle;
    }

    WorldHandle CharacterControllerComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle CharacterControllerComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    AZ::u64 CharacterControllerComponent::GetUserData() const
    {
        AZ::u64 userData = m_configuration.m_userData;
        if (m_system && m_characterHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetCharacterUserData(m_worldHandle, m_characterHandle, userData);
        }
        return userData;
    }

    bool CharacterControllerComponent::SetUserData(
        const AZ::u64 userData)
    {
        if (m_characterHandle
            && (!m_system || !m_system->SetCharacterUserData(m_worldHandle, m_characterHandle, userData)))
        {
            return false;
        }

        m_configuration.m_userData = userData;
        return true;
    }

    WorldTransform CharacterControllerComponent::GetCenterOfMassTransform() const
    {
        WorldTransform transform;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyCenterOfMassTransform(m_worldHandle, m_bodyHandle, transform);
        }
        return transform;
    }

    CharacterState CharacterControllerComponent::GetState() const
    {
        CharacterState state;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetCharacterState(m_worldHandle, m_characterHandle, state);
        }
        return state;
    }

    CharacterRuntimeConfiguration CharacterControllerComponent::GetRuntimeConfiguration() const
    {
        CharacterRuntimeConfiguration configuration = {
            .m_objectLayer = m_configuration.m_objectLayer,
            .m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal,
            .m_up = m_configuration.m_up,
            .m_maximumSeparationDistance = m_configuration.m_maximumSeparationDistance,
            .m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle,
            .m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance,
        };
        if (m_system && m_characterHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetCharacterRuntimeConfiguration(
                    m_worldHandle,
                    m_characterHandle,
                    configuration);
        }
        return configuration;
    }

    AZStd::vector<CharacterCollisionHit> CharacterControllerComponent::CheckCollision(
        const CharacterCollisionRequest& request) const
    {
        AZStd::vector<CharacterCollisionHit> hits;
        if (!m_system || !m_characterHandle)
        {
            return hits;
        }

        AZStd::array<CharacterCollisionHit, 8> inlineHits;
        QueryResult result = m_system->CheckCharacterCollision(
            m_worldHandle,
            m_characterHandle,
            request,
            inlineHits);
        if (result.IsComplete())
        {
            hits.assign(inlineHits.begin(), inlineHits.begin() + result.m_hitCount);
            return hits;
        }

        hits.resize(result.m_requiredHitCount);
        result = m_system->CheckCharacterCollision(
            m_worldHandle,
            m_characterHandle,
            request,
            hits);
        hits.resize(result.m_hitCount);
        return hits;
    }

    bool CharacterControllerComponent::UpdateRuntimeConfiguration(
        const CharacterRuntimeConfiguration& configuration)
    {
        if (m_characterHandle
            && (!m_system
                || !m_system->UpdateCharacterRuntimeConfiguration(
                    m_worldHandle,
                    m_characterHandle,
                    configuration)))
        {
            return false;
        }

        m_configuration.m_objectLayer = configuration.m_objectLayer;
        m_configuration.m_supportingPlaneNormal = configuration.m_supportingPlaneNormal;
        m_configuration.m_up = configuration.m_up;
        m_configuration.m_maximumSeparationDistance = configuration.m_maximumSeparationDistance;
        m_configuration.m_maximumSlopeAngle = configuration.m_maximumSlopeAngle;
        m_configuration.m_supportingPlaneDistance = configuration.m_supportingPlaneDistance;
        return true;
    }

    bool CharacterControllerComponent::SetTransform(
        const WorldTransform& transform,
        const bool activate)
    {
        return m_system
            && m_system->SetCharacterTransform(
                m_worldHandle,
                m_characterHandle,
                transform,
                activate);
    }

    bool CharacterControllerComponent::SetVelocity(
        const AZ::Vector3& velocity)
    {
        return m_system && m_system->SetCharacterVelocity(m_worldHandle, m_characterHandle, velocity);
    }

    bool CharacterControllerComponent::AddImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->AddCharacterImpulse(m_worldHandle, m_characterHandle, impulse);
    }

    void CharacterControllerComponent::Activate()
    {
        m_system = AZ::Interface<ISystem>::Get();
        m_collider = GetEntity()->FindComponent<ColliderComponent>();
        CharacterRequestBus::Handler::BusConnect(GetEntityId());
        BodyRequestBus::Handler::BusConnect(GetEntityId());
        BodyNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    void CharacterControllerComponent::Deactivate()
    {
        [[maybe_unused]] const bool disabled = DisableSimulation();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        BodyNotificationBus::Handler::BusDisconnect();
        BodyRequestBus::Handler::BusDisconnect();
        CharacterRequestBus::Handler::BusDisconnect();
        m_collider = nullptr;
        m_system = nullptr;
    }

    void CharacterControllerComponent::OnBodyMoved(
        const BodyMoveEvent& event)
    {
        if (event.m_bodyHandle != m_bodyHandle)
        {
            return;
        }

        m_syncingTransform = true;
        AZ::TransformBus::Event(
            GetEntityId(),
            &AZ::TransformInterface::SetWorldTM,
            Internal::ToEntityTransform(event.m_transform, m_uniformScale));
        m_syncingTransform = false;
    }

    void CharacterControllerComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (m_syncingTransform || !m_system || !m_characterHandle)
        {
            return;
        }

        const float uniformScale = world.GetUniformScale();
        if (!m_collider->UpdateCharacterUniformScale(
            m_characterHandle,
            m_configuration.m_maximumPenetrationDepth,
            uniformScale))
        {
            return;
        }
        m_uniformScale = uniformScale;
        [[maybe_unused]] const bool updated = m_system->SetCharacterTransform(
            m_worldHandle,
            m_characterHandle,
            Internal::ToWorldTransform(world),
            m_configuration.m_activate);
    }
} // namespace Jolt
