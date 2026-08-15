/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/VirtualCharacterControllerComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/ComponentUtilities.h>
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
    class VirtualCharacterNotificationBusBehaviorHandler final
        : public VirtualCharacterNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            VirtualCharacterNotificationBusBehaviorHandler,
            "{9B144107-4260-490C-8500-CDF5848B4D6C}",
            AZ::SystemAllocator,
            OnCharacterCreated,
            OnCharacterDestroyed,
            OnCharacterDestroying,
            OnCharacterMoved);

        void OnCharacterCreated(const VirtualCharacterHandle characterHandle) override
        {
            Call(FN_OnCharacterCreated, characterHandle);
        }

        void OnCharacterDestroyed(const VirtualCharacterHandle characterHandle) override
        {
            Call(FN_OnCharacterDestroyed, characterHandle);
        }

        void OnCharacterDestroying(const VirtualCharacterHandle characterHandle) override
        {
            Call(FN_OnCharacterDestroying, characterHandle);
        }

        void OnCharacterMoved(const VirtualCharacterMoveEvent& event) override
        {
            Call(FN_OnCharacterMoved, event);
        }
    };

    VirtualCharacterControllerComponent::VirtualCharacterControllerComponent(
        VirtualCharacterComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void VirtualCharacterControllerComponent::Reflect(
        AZ::ReflectContext* context)
    {
        VirtualCharacterComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<VirtualCharacterControllerComponent, AZ::Component>()
                ->Field("Configuration", &VirtualCharacterControllerComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<VirtualCharacterControllerComponent>(
                        "Jolt Virtual Character Controller",
                        "Moves a collision-query character without creating a rigid body.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterControllerComponent::m_configuration,
                        "Configuration",
                        "Collision, movement, stairs, slope, and support properties.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<VirtualCharacterMoveEvent>("VirtualCharacterMoveEvent")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("transform", BehaviorValueGetter(&VirtualCharacterMoveEvent::m_transform), nullptr)
                ->Property("characterHandle", BehaviorValueGetter(&VirtualCharacterMoveEvent::m_characterHandle), nullptr)
                ->Property("entityId", BehaviorValueGetter(&VirtualCharacterMoveEvent::m_entityId), nullptr);

            behaviorContext->Class<VirtualCharacterContact>("VirtualCharacterContact")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("position", BehaviorValueGetter(&VirtualCharacterContact::m_position), nullptr)
                ->Property("contactNormal", BehaviorValueGetter(&VirtualCharacterContact::m_contactNormal), nullptr)
                ->Property("linearVelocity", BehaviorValueGetter(&VirtualCharacterContact::m_linearVelocity), nullptr)
                ->Property("surfaceNormal", BehaviorValueGetter(&VirtualCharacterContact::m_surfaceNormal), nullptr)
                ->Property("bodyHandle", BehaviorValueGetter(&VirtualCharacterContact::m_bodyHandle), nullptr)
                ->Property("materialHandle", BehaviorValueGetter(&VirtualCharacterContact::m_materialHandle), nullptr)
                ->Property("shapeHandle", BehaviorValueGetter(&VirtualCharacterContact::m_shapeHandle), nullptr)
                ->Property("characterHandle", BehaviorValueGetter(&VirtualCharacterContact::m_characterHandle), nullptr)
                ->Property("subShapeId", BehaviorValueGetter(&VirtualCharacterContact::m_subShapeId), nullptr)
                ->Property("distance", BehaviorValueGetter(&VirtualCharacterContact::m_distance), nullptr)
                ->Property("fraction", BehaviorValueGetter(&VirtualCharacterContact::m_fraction), nullptr)
                ->Property("motionType", BehaviorValueGetter(&VirtualCharacterContact::m_motionType), nullptr)
                ->Property("canPushCharacter", BehaviorValueGetter(&VirtualCharacterContact::m_canPushCharacter), nullptr)
                ->Property("hadCollision", BehaviorValueGetter(&VirtualCharacterContact::m_hadCollision), nullptr)
                ->Property("isBackFacing", BehaviorValueGetter(&VirtualCharacterContact::m_isBackFacing), nullptr)
                ->Property("isSensor", BehaviorValueGetter(&VirtualCharacterContact::m_isSensor), nullptr)
                ->Property("wasDiscarded", BehaviorValueGetter(&VirtualCharacterContact::m_wasDiscarded), nullptr);

            behaviorContext->Class<VirtualCharacterRuntimeConfiguration>("VirtualCharacterRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("shapeOffset", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_shapeOffset))
                ->Property(
                    "supportingPlaneNormal",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_supportingPlaneNormal))
                ->Property("up", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_up))
                ->Property(
                    "hitReductionCosMaximumAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_hitReductionCosMaximumAngle))
                ->Property("mass", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_mass))
                ->Property(
                    "maximumSlopeAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_maximumSlopeAngle))
                ->Property(
                    "maximumStrength",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_maximumStrength))
                ->Property(
                    "penetrationRecoverySpeed",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_penetrationRecoverySpeed))
                ->Property(
                    "supportingPlaneDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_supportingPlaneDistance))
                ->Property(
                    "maximumHitCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_maximumHitCount))
                ->Property(
                    "enhancedInternalEdgeRemoval",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterRuntimeConfiguration::m_enhancedInternalEdgeRemoval));

            behaviorContext->Class<VirtualCharacterStairConfiguration>("VirtualCharacterStairConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "stepDownExtra",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterStairConfiguration::m_stepDownExtra))
                ->Property(
                    "stepForward",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterStairConfiguration::m_stepForward))
                ->Property(
                    "stepForwardTest",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterStairConfiguration::m_stepForwardTest))
                ->Property("stepUp", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterStairConfiguration::m_stepUp))
                ->Property("deltaTime", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterStairConfiguration::m_deltaTime));

            behaviorContext->Class<VirtualCharacterUpdateConfiguration>("VirtualCharacterUpdateConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("gravity", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_gravity))
                ->Property(
                    "stickToFloorStepDown",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_stickToFloorStepDown))
                ->Property(
                    "walkStairsStepDownExtra",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_walkStairsStepDownExtra))
                ->Property("walkStairsStepUp", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_walkStairsStepUp))
                ->Property(
                    "walkStairsCosAngleForwardContact",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_walkStairsCosAngleForwardContact))
                ->Property(
                    "walkStairsMinimumStepForward",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_walkStairsMinimumStepForward))
                ->Property(
                    "walkStairsStepForwardTest",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_walkStairsStepForwardTest))
                ->Property("extended", JOLT_BEHAVIOR_VALUE_PROPERTY(&VirtualCharacterUpdateConfiguration::m_extended));

            behaviorContext->Class<VirtualCharacterState>("VirtualCharacterState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("transform", BehaviorValueGetter(&VirtualCharacterState::m_transform), nullptr)
                ->Property("groundPosition", BehaviorValueGetter(&VirtualCharacterState::m_groundPosition), nullptr)
                ->Property("groundNormal", BehaviorValueGetter(&VirtualCharacterState::m_groundNormal), nullptr)
                ->Property("groundVelocity", BehaviorValueGetter(&VirtualCharacterState::m_groundVelocity), nullptr)
                ->Property("linearVelocity", BehaviorValueGetter(&VirtualCharacterState::m_linearVelocity), nullptr)
                ->Property("groundBodyHandle", BehaviorValueGetter(&VirtualCharacterState::m_groundBodyHandle), nullptr)
                ->Property("innerBodyHandle", BehaviorValueGetter(&VirtualCharacterState::m_innerBodyHandle), nullptr)
                ->Property("groundMaterialHandle", BehaviorValueGetter(&VirtualCharacterState::m_groundMaterialHandle), nullptr)
                ->Property(
                    "innerBodyShapeHandle",
                    BehaviorValueGetter(&VirtualCharacterState::m_innerBodyShapeHandle),
                    nullptr)
                ->Property("shapeHandle", BehaviorValueGetter(&VirtualCharacterState::m_shapeHandle), nullptr)
                ->Property("groundSubShapeId", BehaviorValueGetter(&VirtualCharacterState::m_groundSubShapeId), nullptr)
                ->Property("entityId", BehaviorValueGetter(&VirtualCharacterState::m_entityId), nullptr)
                ->Property("name", BehaviorValueGetter(&VirtualCharacterState::m_name), nullptr)
                ->Property("userData", BehaviorValueGetter(&VirtualCharacterState::m_userData), nullptr)
                ->Property("groundState", BehaviorValueGetter(&VirtualCharacterState::m_groundState), nullptr)
                ->Property("isSupported", BehaviorValueGetter(&VirtualCharacterState::m_isSupported), nullptr)
                ->Property(
                    "maximumHitCountExceeded",
                    BehaviorValueGetter(&VirtualCharacterState::m_maximumHitCountExceeded),
                    nullptr);

            behaviorContext->EBus<VirtualCharacterRequestBus>("JoltVirtualCharacterRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &IVirtualCharacterRequests::EnableSimulation)
                ->Event("DisableSimulation", &IVirtualCharacterRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &IVirtualCharacterRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IVirtualCharacterRequests::GetWorldHandle)
                ->Event("GetCharacterHandle", &IVirtualCharacterRequests::GetCharacterHandle)
                ->Event("GetState", &IVirtualCharacterRequests::GetState)
                ->Event("GetUserData", &IVirtualCharacterRequests::GetUserData)
                ->Event("SetUserData", &IVirtualCharacterRequests::SetUserData)
                ->Event("GetRuntimeConfiguration", &IVirtualCharacterRequests::GetRuntimeConfiguration)
                ->Event("CheckCollision", &IVirtualCharacterRequests::CheckCollision)
                ->Event("UpdateRuntimeConfiguration", &IVirtualCharacterRequests::UpdateRuntimeConfiguration)
                ->Event("SetTransform", &IVirtualCharacterRequests::SetTransform)
                ->Event("SetVelocity", &IVirtualCharacterRequests::SetVelocity)
                ->Event(
                    "CancelVelocityTowardsSteepSlopes",
                    &IVirtualCharacterRequests::CancelVelocityTowardsSteepSlopes)
                ->Event("BeginContactTracking", &IVirtualCharacterRequests::BeginContactTracking)
                ->Event("EndContactTracking", &IVirtualCharacterRequests::EndContactTracking)
                ->Event("CanWalkStairs", &IVirtualCharacterRequests::CanWalkStairs)
                ->Event("WalkStairs", &IVirtualCharacterRequests::WalkStairs)
                ->Event("StickToFloor", &IVirtualCharacterRequests::StickToFloor)
                ->Event("RefreshContacts", &IVirtualCharacterRequests::RefreshContacts)
                ->Event("UpdateGroundVelocity", &IVirtualCharacterRequests::UpdateGroundVelocity)
                ->Event("GetContacts", &IVirtualCharacterRequests::GetContacts)
                ->Event("HasCollidedWithBody", &IVirtualCharacterRequests::HasCollidedWithBody)
                ->Event("HasCollidedWithCharacter", &IVirtualCharacterRequests::HasCollidedWithCharacter)
                ->Event("UpdateConfiguration", &IVirtualCharacterRequests::UpdateConfiguration);

            behaviorContext->EBus<VirtualCharacterNotificationBus>("JoltVirtualCharacterNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<VirtualCharacterNotificationBusBehaviorHandler>();

            behaviorContext->Class<VirtualCharacterControllerComponent>("Jolt::VirtualCharacterControllerComponent")
                ->RequestBus("JoltVirtualCharacterRequestBus");
        }
    }

    void VirtualCharacterControllerComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltVirtualCharacterService"));
    }

    void VirtualCharacterControllerComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltBodyService"));
        incompatible.push_back(AZ_CRC_CE("JoltVirtualCharacterService"));
    }

    void VirtualCharacterControllerComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
        required.push_back(AZ_CRC_CE("JoltColliderService"));
    }

    bool VirtualCharacterControllerComponent::EnableSimulation()
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

        VirtualCharacterConfiguration configuration;
        configuration.m_shapeHandle = m_collider->GetRootShapeHandle();
        if (m_configuration.m_createInnerBody)
        {
            configuration.m_innerBodyShapeHandle = m_collider->GetRootShapeHandle();
            configuration.m_innerBodyObjectLayer = m_configuration.m_innerBodyObjectLayer;
        }
        configuration.m_transform = Internal::ToWorldTransform(entityTransform);
        configuration.m_entityId = GetEntityId();
        configuration.m_name = AZ::Name(GetEntity()->GetName());
        configuration.m_userData = m_configuration.m_userData;
        configuration.m_objectLayer = m_configuration.m_objectLayer;
        configuration.m_shapeOffset = m_configuration.m_shapeOffset;
        configuration.m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal;
        configuration.m_up = m_configuration.m_up;
        configuration.m_characterPadding = m_configuration.m_characterPadding;
        configuration.m_collisionTolerance = m_configuration.m_collisionTolerance;
        configuration.m_hitReductionCosMaximumAngle = m_configuration.m_hitReductionCosMaximumAngle;
        configuration.m_mass = m_configuration.m_mass;
        configuration.m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle;
        configuration.m_maximumStrength = m_configuration.m_maximumStrength;
        configuration.m_minimumTimeRemaining = m_configuration.m_minimumTimeRemaining;
        configuration.m_penetrationRecoverySpeed = m_configuration.m_penetrationRecoverySpeed;
        configuration.m_predictiveContactDistance = m_configuration.m_predictiveContactDistance;
        configuration.m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance;
        configuration.m_maximumCollisionIterations = m_configuration.m_maximumCollisionIterations;
        configuration.m_maximumConstraintIterations = m_configuration.m_maximumConstraintIterations;
        configuration.m_maximumHitCount = m_configuration.m_maximumHitCount;
        configuration.m_collideWithBackFaces = m_configuration.m_collideWithBackFaces;
        configuration.m_enhancedInternalEdgeRemoval = m_configuration.m_enhancedInternalEdgeRemoval;

        m_characterHandle = m_system->CreateVirtualCharacter(m_worldHandle, configuration);
        if (!m_characterHandle)
        {
            m_collider->DestroyShapes();
            return false;
        }
        if (!m_system->EnableVirtualCharacterAutoUpdate(
            m_worldHandle,
            m_characterHandle,
            m_configuration.m_update))
        {
            [[maybe_unused]] const bool destroyed =
                m_system->DestroyVirtualCharacter(m_worldHandle, m_characterHandle);
            m_characterHandle = VirtualCharacterHandle::Invalid;
            m_collider->DestroyShapes();
            return false;
        }

        VirtualCharacterNotificationBus::Event(
            GetEntityId(),
            &IVirtualCharacterNotifications::OnCharacterCreated,
            m_characterHandle);
        return true;
    }

    bool VirtualCharacterControllerComponent::DisableSimulation()
    {
        if (!m_characterHandle)
        {
            return true;
        }

        const VirtualCharacterHandle characterHandle = m_characterHandle;
        VirtualCharacterNotificationBus::Event(
            GetEntityId(),
            &IVirtualCharacterNotifications::OnCharacterDestroying,
            characterHandle);
        if (!m_system->DestroyVirtualCharacter(m_worldHandle, characterHandle))
        {
            return false;
        }

        m_characterHandle = VirtualCharacterHandle::Invalid;
        m_collider->DestroyShapes();
        VirtualCharacterNotificationBus::Event(
            GetEntityId(),
            &IVirtualCharacterNotifications::OnCharacterDestroyed,
            characterHandle);
        return true;
    }

    bool VirtualCharacterControllerComponent::IsSimulationEnabled() const
    {
        return m_system && m_characterHandle;
    }

    WorldHandle VirtualCharacterControllerComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    VirtualCharacterHandle VirtualCharacterControllerComponent::GetCharacterHandle() const
    {
        return m_characterHandle;
    }

    VirtualCharacterState VirtualCharacterControllerComponent::GetState() const
    {
        VirtualCharacterState state;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetVirtualCharacterState(m_worldHandle, m_characterHandle, state);
        }
        return state;
    }

    AZ::u64 VirtualCharacterControllerComponent::GetUserData() const
    {
        AZ::u64 userData = m_configuration.m_userData;
        if (m_system && m_characterHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetVirtualCharacterUserData(m_worldHandle, m_characterHandle, userData);
        }
        return userData;
    }

    bool VirtualCharacterControllerComponent::SetUserData(
        const AZ::u64 userData)
    {
        if (m_characterHandle
            && (!m_system || !m_system->SetVirtualCharacterUserData(m_worldHandle, m_characterHandle, userData)))
        {
            return false;
        }

        m_configuration.m_userData = userData;
        return true;
    }

    VirtualCharacterRuntimeConfiguration VirtualCharacterControllerComponent::GetRuntimeConfiguration() const
    {
        VirtualCharacterRuntimeConfiguration configuration = {
            .m_shapeOffset = m_configuration.m_shapeOffset,
            .m_supportingPlaneNormal = m_configuration.m_supportingPlaneNormal,
            .m_up = m_configuration.m_up,
            .m_hitReductionCosMaximumAngle = m_configuration.m_hitReductionCosMaximumAngle,
            .m_mass = m_configuration.m_mass,
            .m_maximumSlopeAngle = m_configuration.m_maximumSlopeAngle,
            .m_maximumStrength = m_configuration.m_maximumStrength,
            .m_penetrationRecoverySpeed = m_configuration.m_penetrationRecoverySpeed,
            .m_supportingPlaneDistance = m_configuration.m_supportingPlaneDistance,
            .m_maximumHitCount = m_configuration.m_maximumHitCount,
            .m_enhancedInternalEdgeRemoval = m_configuration.m_enhancedInternalEdgeRemoval,
        };
        if (m_system && m_characterHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetVirtualCharacterRuntimeConfiguration(
                    m_worldHandle,
                    m_characterHandle,
                    configuration);
        }
        return configuration;
    }

    AZStd::vector<CharacterCollisionHit> VirtualCharacterControllerComponent::CheckCollision(
        const CharacterCollisionRequest& request) const
    {
        AZStd::vector<CharacterCollisionHit> hits;
        if (!m_system || !m_characterHandle)
        {
            return hits;
        }

        AZStd::array<CharacterCollisionHit, 8> inlineHits;
        QueryResult result = m_system->CheckVirtualCharacterCollision(
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
        result = m_system->CheckVirtualCharacterCollision(
            m_worldHandle,
            m_characterHandle,
            request,
            hits);
        hits.resize(result.m_hitCount);
        return hits;
    }

    bool VirtualCharacterControllerComponent::UpdateRuntimeConfiguration(
        const VirtualCharacterRuntimeConfiguration& configuration)
    {
        if (m_characterHandle
            && (!m_system
                || !m_system->UpdateVirtualCharacterRuntimeConfiguration(
                    m_worldHandle,
                    m_characterHandle,
                    configuration)))
        {
            return false;
        }

        m_configuration.m_shapeOffset = configuration.m_shapeOffset;
        m_configuration.m_supportingPlaneNormal = configuration.m_supportingPlaneNormal;
        m_configuration.m_up = configuration.m_up;
        m_configuration.m_hitReductionCosMaximumAngle = configuration.m_hitReductionCosMaximumAngle;
        m_configuration.m_mass = configuration.m_mass;
        m_configuration.m_maximumSlopeAngle = configuration.m_maximumSlopeAngle;
        m_configuration.m_maximumStrength = configuration.m_maximumStrength;
        m_configuration.m_penetrationRecoverySpeed = configuration.m_penetrationRecoverySpeed;
        m_configuration.m_supportingPlaneDistance = configuration.m_supportingPlaneDistance;
        m_configuration.m_maximumHitCount = configuration.m_maximumHitCount;
        m_configuration.m_enhancedInternalEdgeRemoval = configuration.m_enhancedInternalEdgeRemoval;
        return true;
    }

    bool VirtualCharacterControllerComponent::SetTransform(const WorldTransform& transform)
    {
        return m_system
            && m_system->SetVirtualCharacterTransform(m_worldHandle, m_characterHandle, transform);
    }

    bool VirtualCharacterControllerComponent::SetVelocity(const AZ::Vector3& velocity)
    {
        return m_system
            && m_system->SetVirtualCharacterVelocity(m_worldHandle, m_characterHandle, velocity);
    }

    AZ::Vector3 VirtualCharacterControllerComponent::CancelVelocityTowardsSteepSlopes(
        const AZ::Vector3& desiredVelocity) const
    {
        AZ::Vector3 adjustedVelocity = desiredVelocity;
        if (m_system)
        {
            [[maybe_unused]] const bool adjusted =
                m_system->CancelVirtualCharacterVelocityTowardsSteepSlopes(
                    m_worldHandle,
                    m_characterHandle,
                    desiredVelocity,
                    adjustedVelocity);
        }
        return adjustedVelocity;
    }

    bool VirtualCharacterControllerComponent::BeginContactTracking()
    {
        return m_system
            && m_system->BeginVirtualCharacterContactTracking(
                m_worldHandle,
                m_characterHandle);
    }

    bool VirtualCharacterControllerComponent::EndContactTracking()
    {
        return m_system
            && m_system->EndVirtualCharacterContactTracking(
                m_worldHandle,
                m_characterHandle);
    }

    bool VirtualCharacterControllerComponent::CanWalkStairs(
        const AZ::Vector3& desiredVelocity) const
    {
        return m_system
            && m_system->CanVirtualCharacterWalkStairs(
                m_worldHandle,
                m_characterHandle,
                desiredVelocity);
    }

    bool VirtualCharacterControllerComponent::WalkStairs(
        const VirtualCharacterStairConfiguration& configuration)
    {
        return m_system
            && m_system->WalkVirtualCharacterStairs(
                m_worldHandle,
                m_characterHandle,
                configuration);
    }

    bool VirtualCharacterControllerComponent::StickToFloor(
        const AZ::Vector3& stepDown)
    {
        return m_system
            && m_system->StickVirtualCharacterToFloor(
                m_worldHandle,
                m_characterHandle,
                stepDown);
    }

    bool VirtualCharacterControllerComponent::RefreshContacts()
    {
        return m_system
            && m_system->RefreshVirtualCharacterContacts(
                m_worldHandle,
                m_characterHandle);
    }

    bool VirtualCharacterControllerComponent::UpdateGroundVelocity()
    {
        return m_system
            && m_system->UpdateVirtualCharacterGroundVelocity(
                m_worldHandle,
                m_characterHandle);
    }

    AZStd::vector<VirtualCharacterContact> VirtualCharacterControllerComponent::GetContacts() const
    {
        AZStd::vector<VirtualCharacterContact> contacts;
        if (!m_system || !m_characterHandle)
        {
            return contacts;
        }

        QueryResult result = m_system->GetVirtualCharacterContacts(
            m_worldHandle,
            m_characterHandle,
            {});
        contacts.resize(result.m_requiredHitCount);
        if (contacts.empty())
        {
            return contacts;
        }

        result = m_system->GetVirtualCharacterContacts(
            m_worldHandle,
            m_characterHandle,
            contacts);
        contacts.resize(result.m_hitCount);
        return contacts;
    }

    bool VirtualCharacterControllerComponent::HasCollidedWithBody(
        const BodyHandle bodyHandle) const
    {
        return m_system
            && m_system->HasVirtualCharacterCollidedWith(
                m_worldHandle,
                m_characterHandle,
                bodyHandle);
    }

    bool VirtualCharacterControllerComponent::HasCollidedWithCharacter(
        const VirtualCharacterHandle characterHandle) const
    {
        return m_system
            && m_system->HaveVirtualCharactersCollided(
                m_worldHandle,
                m_characterHandle,
                characterHandle);
    }

    bool VirtualCharacterControllerComponent::UpdateConfiguration(
        const VirtualCharacterUpdateConfiguration& configuration)
    {
        if (!m_system
            || !m_system->EnableVirtualCharacterAutoUpdate(
                m_worldHandle,
                m_characterHandle,
                configuration))
        {
            return false;
        }

        m_configuration.m_update = configuration;
        return true;
    }

    void VirtualCharacterControllerComponent::Activate()
    {
        m_system = AZ::Interface<ISystem>::Get();
        m_collider = GetEntity()->FindComponent<ColliderComponent>();
        VirtualCharacterRequestBus::Handler::BusConnect(GetEntityId());
        VirtualCharacterNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        if (m_configuration.m_enabled)
        {
            [[maybe_unused]] const bool enabled = EnableSimulation();
        }
    }

    void VirtualCharacterControllerComponent::Deactivate()
    {
        [[maybe_unused]] const bool disabled = DisableSimulation();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        VirtualCharacterNotificationBus::Handler::BusDisconnect();
        VirtualCharacterRequestBus::Handler::BusDisconnect();
        m_collider = nullptr;
        m_system = nullptr;
    }

    void VirtualCharacterControllerComponent::OnCharacterMoved(
        const VirtualCharacterMoveEvent& event)
    {
        if (event.m_characterHandle != m_characterHandle)
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

    void VirtualCharacterControllerComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (m_syncingTransform || !m_system || !m_characterHandle)
        {
            return;
        }

        const float uniformScale = world.GetUniformScale();
        if (!m_collider->UpdateVirtualCharacterUniformScale(
            m_characterHandle,
            m_configuration.m_maximumPenetrationDepth,
            m_configuration.m_createInnerBody,
            uniformScale))
        {
            return;
        }
        m_uniformScale = uniformScale;
        [[maybe_unused]] const bool updated = m_system->SetVirtualCharacterTransform(
            m_worldHandle,
            m_characterHandle,
            Internal::ToWorldTransform(world));
    }
} // namespace Jolt
