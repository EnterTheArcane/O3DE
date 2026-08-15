/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/RigidBodyComponent.h>

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
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    class BodyNotificationBusBehaviorHandler final
        : public BodyNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            BodyNotificationBusBehaviorHandler,
            "{5C46BEA7-031D-4BF2-B63C-97A337A40615}",
            AZ::SystemAllocator,
            OnBodyCreated,
            OnBodyDestroying,
            OnBodyDestroyed,
            OnBodyMoved);

        void OnBodyCreated(
            const WorldHandle worldHandle,
            const BodyHandle bodyHandle) override
        {
            Call(FN_OnBodyCreated, worldHandle, bodyHandle);
        }

        void OnBodyDestroyed(
            const WorldHandle worldHandle,
            const BodyHandle bodyHandle) override
        {
            Call(FN_OnBodyDestroyed, worldHandle, bodyHandle);
        }

        void OnBodyDestroying(
            const WorldHandle worldHandle,
            const BodyHandle bodyHandle) override
        {
            Call(FN_OnBodyDestroying, worldHandle, bodyHandle);
        }

        void OnBodyMoved(
            const BodyMoveEvent& event) override
        {
            Call(FN_OnBodyMoved, event);
        }
    };

    RigidBodyComponent::RigidBodyComponent(
        RigidBodyConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void RigidBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        RigidBodyConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<RigidBodyComponent, AZ::Component>()
                ->Field("Configuration", &RigidBodyComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<RigidBodyComponent>("Jolt Rigid Body", "Simulates an entity as a Jolt rigid body.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RigidBodyComponent::m_configuration,
                        "Configuration",
                        "Rigid-body creation and runtime properties.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BodyKind, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BodyKind, Rigid);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, BodyKind, Soft);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionType, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionType, Dynamic);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionType, Kinematic);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionType, Static);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, All);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, RotationX);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, RotationY);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, RotationZ);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, TranslationX);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, TranslationY);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, AllowedDofs, TranslationZ);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionQuality, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionQuality, Continuous);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotionQuality, Discrete);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, MassPropertiesMode, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MassPropertiesMode, CalculateMassAndInertia);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MassPropertiesMode, CalculateInertia);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MassPropertiesMode, Provided);

            behaviorContext->Class<MassPropertiesConfiguration>("JoltMassPropertiesConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "MassPropertiesConfiguration")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "MassPropertiesConfiguration")
                ->Property("inertia", JOLT_BEHAVIOR_VALUE_PROPERTY(&MassPropertiesConfiguration::m_inertia))
                ->Property(
                    "inertiaMultiplier",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MassPropertiesConfiguration::m_inertiaMultiplier))
                ->Property("mass", JOLT_BEHAVIOR_VALUE_PROPERTY(&MassPropertiesConfiguration::m_mass))
                ->Property("mode", JOLT_BEHAVIOR_VALUE_PROPERTY(&MassPropertiesConfiguration::m_mode));

            behaviorContext->Class<BodyRuntimeConfiguration>("JoltBodyRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "BodyRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BodyRuntimeConfiguration")
                ->Property(
                    "massProperties",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_massProperties))
                ->Property("angularDamping", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_angularDamping))
                ->Property("friction", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_friction))
                ->Property("gravityFactor", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_gravityFactor))
                ->Property("linearDamping", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_linearDamping))
                ->Property(
                    "maximumAngularVelocity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_maximumAngularVelocity))
                ->Property(
                    "maximumLinearVelocity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_maximumLinearVelocity))
                ->Property("restitution", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_restitution))
                ->Property("allowedDofs", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_allowedDofs))
                ->Property("motionQuality", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_motionQuality))
                ->Property(
                    "positionStepCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_positionStepCount))
                ->Property(
                    "velocityStepCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_velocityStepCount))
                ->Property("allowSleeping", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_allowSleeping))
                ->Property(
                    "applyGyroscopicForce",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_applyGyroscopicForce))
                ->Property(
                    "collideKinematicVsNonDynamic",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_collideKinematicVsNonDynamic))
                ->Property(
                    "enhancedInternalEdgeRemoval",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_enhancedInternalEdgeRemoval))
                ->Property("isSensor", JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_isSensor))
                ->Property(
                    "useManifoldReduction",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&BodyRuntimeConfiguration::m_useManifoldReduction));

            behaviorContext->Class<BodyState>("JoltBodyState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "BodyState")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BodyState")
                ->Property("transform", BehaviorValueGetter(&BodyState::m_transform), nullptr)
                ->Property("linearVelocity", BehaviorValueGetter(&BodyState::m_linearVelocity), nullptr)
                ->Property("angularVelocity", BehaviorValueGetter(&BodyState::m_angularVelocity), nullptr)
                ->Property("entityId", BehaviorValueGetter(&BodyState::m_entityId), nullptr)
                ->Property("name", BehaviorValueGetter(&BodyState::m_name), nullptr)
                ->Property("userData", BehaviorValueGetter(&BodyState::m_userData), nullptr)
                ->Property("shapeHandle", BehaviorValueGetter(&BodyState::m_shapeHandle), nullptr)
                ->Property("kind", BehaviorValueGetter(&BodyState::m_kind), nullptr)
                ->Property("motionType", BehaviorValueGetter(&BodyState::m_motionType), nullptr)
                ->Property("isActive", BehaviorValueGetter(&BodyState::m_isActive), nullptr)
                ->Property("isInSimulation", BehaviorValueGetter(&BodyState::m_isInSimulation), nullptr);

            behaviorContext->Class<BodyMoveEvent>("JoltBodyMoveEvent")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "BodyMoveEvent")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BodyMoveEvent")
                ->Property("transform", BehaviorValueGetter(&BodyMoveEvent::m_transform), nullptr)
                ->Property("bodyHandle", BehaviorValueGetter(&BodyMoveEvent::m_bodyHandle), nullptr)
                ->Property("entityId", BehaviorValueGetter(&BodyMoveEvent::m_entityId), nullptr);

            behaviorContext->Class<BuoyancyConfiguration>("JoltBuoyancyConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "BuoyancyConfiguration")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BuoyancyConfiguration")
                ->Property("surfacePosition", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_surfacePosition))
                ->Property("fluidVelocity", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_fluidVelocity))
                ->Property("gravity", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_gravity))
                ->Property("surfaceNormal", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_surfaceNormal))
                ->Property("angularDrag", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_angularDrag))
                ->Property("buoyancy", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_buoyancy))
                ->Property("deltaTime", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_deltaTime))
                ->Property("linearDrag", JOLT_BEHAVIOR_VALUE_PROPERTY(&BuoyancyConfiguration::m_linearDrag));

            behaviorContext->EBus<RigidBodyRequestBus>("JoltRigidBodyRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &IRigidBodyRequests::EnableSimulation)
                ->Event("DisableSimulation", &IRigidBodyRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &IRigidBodyRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IRigidBodyRequests::GetWorldHandle)
                ->Event("GetBodyHandle", &IRigidBodyRequests::GetBodyHandle)
                ->Event("GetCenterOfMassTransform", &IRigidBodyRequests::GetCenterOfMassTransform)
                ->Event("GetState", &IRigidBodyRequests::GetState)
                ->Event("GetRuntimeConfiguration", &IRigidBodyRequests::GetRuntimeConfiguration)
                ->Event("GetFriction", &IRigidBodyRequests::GetFriction)
                ->Event("GetRestitution", &IRigidBodyRequests::GetRestitution)
                ->Event("GetGravityFactor", &IRigidBodyRequests::GetGravityFactor)
                ->Event("GetMaximumLinearVelocity", &IRigidBodyRequests::GetMaximumLinearVelocity)
                ->Event("GetMaximumAngularVelocity", &IRigidBodyRequests::GetMaximumAngularVelocity)
                ->Event("GetMotionQuality", &IRigidBodyRequests::GetMotionQuality)
                ->Event("IsManifoldReductionEnabled", &IRigidBodyRequests::IsManifoldReductionEnabled)
                ->Event("IsSensor", &IRigidBodyRequests::IsSensor)
                ->Event("GetLinearDamping", &IRigidBodyRequests::GetLinearDamping)
                ->Event("GetAngularDamping", &IRigidBodyRequests::GetAngularDamping)
                ->Event("IsSleepingAllowed", &IRigidBodyRequests::IsSleepingAllowed)
                ->Event("IsGyroscopicForceEnabled", &IRigidBodyRequests::IsGyroscopicForceEnabled)
                ->Event(
                    "IsKinematicVsNonDynamicCollisionEnabled",
                    &IRigidBodyRequests::IsKinematicVsNonDynamicCollisionEnabled)
                ->Event(
                    "IsEnhancedInternalEdgeRemovalEnabled",
                    &IRigidBodyRequests::IsEnhancedInternalEdgeRemovalEnabled)
                ->Event("GetSolverStepCounts", &IRigidBodyRequests::GetSolverStepCounts)
                ->Event("GetInverseInertia", &IRigidBodyRequests::GetInverseInertia)
                ->Event("GetInverseMass", &IRigidBodyRequests::GetInverseMass)
                ->Event("GetPointVelocity", &IRigidBodyRequests::GetPointVelocity)
                ->Event("GetMotionType", &IRigidBodyRequests::GetMotionType)
                ->Event("GetObjectLayer", &IRigidBodyRequests::GetObjectLayer)
                ->Event("GetCollisionGroup", &IRigidBodyRequests::GetCollisionGroup)
                ->Event("GetShapeHandle", &IRigidBodyRequests::GetShapeHandle)
                ->Event(
                    "GetAccumulatedForceAndTorque",
                    &IRigidBodyRequests::GetAccumulatedForceAndTorque)
                ->Event("ResetAccumulatedForce", &IRigidBodyRequests::ResetAccumulatedForce)
                ->Event("ResetAccumulatedTorque", &IRigidBodyRequests::ResetAccumulatedTorque)
                ->Event("ResetMotion", &IRigidBodyRequests::ResetMotion)
                ->Event("GetBounds", &IRigidBodyRequests::GetBounds)
                ->Event("GetSubmergedVolume", &IRigidBodyRequests::GetSubmergedVolume)
                ->Event("GetSurfaceNormal", &IRigidBodyRequests::GetSurfaceNormal)
                ->Event("GetMaterial", &IRigidBodyRequests::GetMaterial)
                ->Event("GetPosition", &IRigidBodyRequests::GetPosition)
                ->Event("GetRotation", &IRigidBodyRequests::GetRotation)
                ->Event("GetVelocities", &IRigidBodyRequests::GetVelocities)
                ->Event("GetLinearVelocity", &IRigidBodyRequests::GetLinearVelocity)
                ->Event("GetAngularVelocity", &IRigidBodyRequests::GetAngularVelocity)
                ->Event("ActivateBody", &IRigidBodyRequests::ActivateBody)
                ->Event("DeactivateBody", &IRigidBodyRequests::DeactivateBody)
                ->Event("ResetSleepTimer", &IRigidBodyRequests::ResetSleepTimer)
                ->Event("InvalidateContactCache", &IRigidBodyRequests::InvalidateContactCache)
                ->Event("AddForce", &IRigidBodyRequests::AddForce)
                ->Event("AddForceAtPosition", &IRigidBodyRequests::AddForceAtPosition)
                ->Event("AddForceAndTorque", &IRigidBodyRequests::AddForceAndTorque)
                ->Event("AddTorque", &IRigidBodyRequests::AddTorque)
                ->Event("AddImpulse", &IRigidBodyRequests::AddImpulse)
                ->Event("AddImpulseAtPosition", &IRigidBodyRequests::AddImpulseAtPosition)
                ->Event("AddAngularImpulse", &IRigidBodyRequests::AddAngularImpulse)
                ->Event("ApplyBuoyancyImpulse", &IRigidBodyRequests::ApplyBuoyancyImpulse)
                ->Event("SetFriction", &IRigidBodyRequests::SetFriction)
                ->Event("SetRestitution", &IRigidBodyRequests::SetRestitution)
                ->Event("SetGravityFactor", &IRigidBodyRequests::SetGravityFactor)
                ->Event("SetMaximumLinearVelocity", &IRigidBodyRequests::SetMaximumLinearVelocity)
                ->Event("SetMaximumAngularVelocity", &IRigidBodyRequests::SetMaximumAngularVelocity)
                ->Event("SetMotionQuality", &IRigidBodyRequests::SetMotionQuality)
                ->Event("SetManifoldReductionEnabled", &IRigidBodyRequests::SetManifoldReductionEnabled)
                ->Event("SetSensor", &IRigidBodyRequests::SetSensor)
                ->Event("SetLinearDamping", &IRigidBodyRequests::SetLinearDamping)
                ->Event("SetAngularDamping", &IRigidBodyRequests::SetAngularDamping)
                ->Event("SetSleepingAllowed", &IRigidBodyRequests::SetSleepingAllowed)
                ->Event("SetGyroscopicForceEnabled", &IRigidBodyRequests::SetGyroscopicForceEnabled)
                ->Event(
                    "SetKinematicVsNonDynamicCollisionEnabled",
                    &IRigidBodyRequests::SetKinematicVsNonDynamicCollisionEnabled)
                ->Event(
                    "SetEnhancedInternalEdgeRemovalEnabled",
                    &IRigidBodyRequests::SetEnhancedInternalEdgeRemovalEnabled)
                ->Event("SetSolverStepCounts", &IRigidBodyRequests::SetSolverStepCounts)
                ->Event("SetPosition", &IRigidBodyRequests::SetPosition)
                ->Event("SetRotation", &IRigidBodyRequests::SetRotation)
                ->Event("SetTransform", &IRigidBodyRequests::SetTransform)
                ->Event("SetTransformWhenChanged", &IRigidBodyRequests::SetTransformWhenChanged)
                ->Event("SetVelocities", &IRigidBodyRequests::SetVelocities)
                ->Event("SetLinearVelocity", &IRigidBodyRequests::SetLinearVelocity)
                ->Event("SetAngularVelocity", &IRigidBodyRequests::SetAngularVelocity)
                ->Event("AddVelocities", &IRigidBodyRequests::AddVelocities)
                ->Event("AddLinearVelocity", &IRigidBodyRequests::AddLinearVelocity)
                ->Event("MoveKinematically", &IRigidBodyRequests::MoveKinematically)
                ->Event("UpdateRuntimeConfiguration", &IRigidBodyRequests::UpdateRuntimeConfiguration);

            behaviorContext->EBus<BodyRequestBus>("JoltBodyRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("IsSimulationEnabled", &IBodyRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IBodyRequests::GetWorldHandle)
                ->Event("GetBodyHandle", &IBodyRequests::GetBodyHandle)
                ->Event("GetUserData", &IBodyRequests::GetUserData)
                ->Event("SetUserData", &IBodyRequests::SetUserData)
                ->Event("GetCenterOfMassTransform", &IBodyRequests::GetCenterOfMassTransform);

            behaviorContext->EBus<BodyNotificationBus>("JoltBodyNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<BodyNotificationBusBehaviorHandler>();

            behaviorContext->Class<RigidBodyComponent>("Jolt::RigidBodyComponent")
                ->RequestBus("JoltRigidBodyRequestBus");
        }
    }

    void RigidBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltBodyService"));
    }

    void RigidBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltBodyService"));
    }

    void RigidBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
        required.push_back(AZ_CRC_CE("JoltColliderService"));
    }

    bool RigidBodyComponent::EnableSimulation()
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

        const BodyRuntimeConfiguration& runtime = m_configuration.m_runtime;
        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = m_collider->GetRootShapeHandle();
        bodyConfiguration.m_transform = Internal::ToWorldTransform(entityTransform);
        bodyConfiguration.m_linearVelocity = m_configuration.m_initialLinearVelocity;
        bodyConfiguration.m_angularVelocity = m_configuration.m_initialAngularVelocity;
        bodyConfiguration.m_entityId = GetEntityId();
        bodyConfiguration.m_name = AZ::Name(GetEntity()->GetName());
        bodyConfiguration.m_userData = m_configuration.m_userData;
        bodyConfiguration.m_collisionGroup = m_configuration.m_collisionGroup;
        bodyConfiguration.m_objectLayer = m_configuration.m_objectLayer;
        bodyConfiguration.m_allowedDofs = runtime.m_allowedDofs;
        bodyConfiguration.m_massProperties = runtime.m_massProperties;
        bodyConfiguration.m_motionQuality = runtime.m_motionQuality;
        bodyConfiguration.m_motionType = m_configuration.m_motionType;
        bodyConfiguration.m_friction = runtime.m_friction;
        bodyConfiguration.m_restitution = runtime.m_restitution;
        bodyConfiguration.m_linearDamping = runtime.m_linearDamping;
        bodyConfiguration.m_angularDamping = runtime.m_angularDamping;
        bodyConfiguration.m_maximumLinearVelocity = runtime.m_maximumLinearVelocity;
        bodyConfiguration.m_maximumAngularVelocity = runtime.m_maximumAngularVelocity;
        bodyConfiguration.m_gravityFactor = runtime.m_gravityFactor;
        bodyConfiguration.m_velocityStepCount = runtime.m_velocityStepCount;
        bodyConfiguration.m_positionStepCount = runtime.m_positionStepCount;
        bodyConfiguration.m_activate = m_configuration.m_activate;
        bodyConfiguration.m_allowDynamicOrKinematic = m_configuration.m_allowDynamicOrKinematic;
        bodyConfiguration.m_allowSleeping = runtime.m_allowSleeping;
        bodyConfiguration.m_applyGyroscopicForce = runtime.m_applyGyroscopicForce;
        bodyConfiguration.m_collideKinematicVsNonDynamic = runtime.m_collideKinematicVsNonDynamic;
        bodyConfiguration.m_enhancedInternalEdgeRemoval = runtime.m_enhancedInternalEdgeRemoval;
        bodyConfiguration.m_isSensor = runtime.m_isSensor;
        bodyConfiguration.m_useManifoldReduction = runtime.m_useManifoldReduction;

        m_bodyHandle = m_system->CreateBody(m_worldHandle, bodyConfiguration);
        if (!m_bodyHandle)
        {
            m_collider->DestroyShapes();
            return false;
        }
        if (!m_system->SetBodyMoveEventsEnabled(m_worldHandle, m_bodyHandle, true))
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroyBody(m_worldHandle, m_bodyHandle);
            m_bodyHandle = {};
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

    bool RigidBodyComponent::DisableSimulation()
    {
        if (!m_bodyHandle)
        {
            return true;
        }

        const BodyHandle bodyHandle = m_bodyHandle;
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyDestroying,
            m_worldHandle,
            bodyHandle);
        if (!m_system->DestroyBody(m_worldHandle, bodyHandle))
        {
            return false;
        }
        m_bodyHandle = {};
        m_collider->DestroyShapes();
        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyDestroyed,
            m_worldHandle,
            bodyHandle);
        return true;
    }

    bool RigidBodyComponent::IsSimulationEnabled() const
    {
        return m_system && m_bodyHandle;
    }

    WorldHandle RigidBodyComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle RigidBodyComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    AZ::u64 RigidBodyComponent::GetUserData() const
    {
        AZ::u64 userData = m_configuration.m_userData;
        if (m_system && m_bodyHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyUserData(m_worldHandle, m_bodyHandle, userData);
        }
        return userData;
    }

    bool RigidBodyComponent::SetUserData(
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

    WorldTransform RigidBodyComponent::GetCenterOfMassTransform() const
    {
        WorldTransform transform;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyCenterOfMassTransform(m_worldHandle, m_bodyHandle, transform);
        }
        return transform;
    }

    BodyState RigidBodyComponent::GetState() const
    {
        BodyState state;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetBodyState(m_worldHandle, m_bodyHandle, state);
        }
        return state;
    }

    BodyRuntimeConfiguration RigidBodyComponent::GetRuntimeConfiguration() const
    {
        BodyRuntimeConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyRuntimeConfiguration(
                    m_worldHandle,
                    m_bodyHandle,
                    configuration);
        }
        return configuration;
    }

    bool RigidBodyComponent::GetInverseInertia(
        AZ::Matrix3x3& inverseInertia) const
    {
        return m_system
            && m_system->GetBodyInverseInertia(
                m_worldHandle,
                m_bodyHandle,
                inverseInertia);
    }

    bool RigidBodyComponent::GetInverseMass(
        float& inverseMass) const
    {
        return m_system
            && m_system->GetBodyInverseMass(
                m_worldHandle,
                m_bodyHandle,
                inverseMass);
    }

    bool RigidBodyComponent::GetPointVelocity(
        const WorldPosition& point,
        AZ::Vector3& velocity) const
    {
        return m_system
            && m_system->GetBodyPointVelocity(
                m_worldHandle,
                m_bodyHandle,
                point,
                velocity);
    }

    MotionType RigidBodyComponent::GetMotionType() const
    {
        MotionType motionType = MotionType::None;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyMotionType(m_worldHandle, m_bodyHandle, motionType);
        }
        return motionType;
    }

    ObjectLayer RigidBodyComponent::GetObjectLayer() const
    {
        ObjectLayer objectLayer = ObjectLayer::Invalid;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyObjectLayer(m_worldHandle, m_bodyHandle, objectLayer);
        }
        return objectLayer;
    }

    CollisionGroupConfiguration RigidBodyComponent::GetCollisionGroup() const
    {
        CollisionGroupConfiguration collisionGroup;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyCollisionGroup(m_worldHandle, m_bodyHandle, collisionGroup);
        }
        return collisionGroup;
    }

    ShapeHandle RigidBodyComponent::GetShapeHandle() const
    {
        ShapeHandle shapeHandle = ShapeHandle::Invalid;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyShape(m_worldHandle, m_bodyHandle, shapeHandle);
        }
        return shapeHandle;
    }

    bool RigidBodyComponent::GetAccumulatedForceAndTorque(
        AZ::Vector3& force,
        AZ::Vector3& torque) const
    {
        return m_system
            && m_system->GetBodyAccumulatedForceAndTorque(
                m_worldHandle,
                m_bodyHandle,
                force,
                torque);
    }

    bool RigidBodyComponent::ResetAccumulatedForce()
    {
        return m_system && m_system->ResetBodyAccumulatedForce(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::ResetAccumulatedTorque()
    {
        return m_system && m_system->ResetBodyAccumulatedTorque(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::ResetMotion()
    {
        return m_system && m_system->ResetBodyMotion(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::GetBounds(
        BroadPhaseAabb& bounds) const
    {
        return m_system && m_system->GetBodyBounds(m_worldHandle, m_bodyHandle, bounds);
    }

    bool RigidBodyComponent::GetSubmergedVolume(
        const WorldPosition& surfacePosition,
        const AZ::Vector3& surfaceNormal,
        SubmergedVolumeResult& result) const
    {
        return m_system
            && m_system->GetBodySubmergedVolume(
                m_worldHandle,
                m_bodyHandle,
                surfacePosition,
                surfaceNormal,
                result);
    }

    bool RigidBodyComponent::GetSurfaceNormal(
        const SubShapeId subShapeId,
        const WorldPosition& surfacePosition,
        AZ::Vector3& normal) const
    {
        return m_system
            && m_system->GetBodySurfaceNormal(
                m_worldHandle,
                m_bodyHandle,
                subShapeId,
                surfacePosition,
                normal);
    }

    bool RigidBodyComponent::GetMaterial(
        const SubShapeId subShapeId,
        MaterialHandle& materialHandle) const
    {
        return m_system
            && m_system->GetBodyMaterial(
                m_worldHandle,
                m_bodyHandle,
                subShapeId,
                materialHandle);
    }

    WorldPosition RigidBodyComponent::GetPosition() const
    {
        WorldPosition position;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyPosition(m_worldHandle, m_bodyHandle, position);
        }
        return position;
    }

    AZ::Quaternion RigidBodyComponent::GetRotation() const
    {
        AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyRotation(m_worldHandle, m_bodyHandle, rotation);
        }
        return rotation;
    }

    bool RigidBodyComponent::GetVelocities(
        AZ::Vector3& linearVelocity,
        AZ::Vector3& angularVelocity) const
    {
        return m_system
            && m_system->GetBodyVelocities(
                m_worldHandle,
                m_bodyHandle,
                linearVelocity,
                angularVelocity);
    }

    AZ::Vector3 RigidBodyComponent::GetLinearVelocity() const
    {
        AZ::Vector3 linearVelocity = AZ::Vector3::CreateZero();
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyLinearVelocity(m_worldHandle, m_bodyHandle, linearVelocity);
        }
        return linearVelocity;
    }

    AZ::Vector3 RigidBodyComponent::GetAngularVelocity() const
    {
        AZ::Vector3 angularVelocity = AZ::Vector3::CreateZero();
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyAngularVelocity(m_worldHandle, m_bodyHandle, angularVelocity);
        }
        return angularVelocity;
    }

    bool RigidBodyComponent::ActivateBody()
    {
        return m_system && m_system->ActivateBody(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::DeactivateBody()
    {
        return m_system && m_system->DeactivateBody(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::ResetSleepTimer()
    {
        return m_system && m_system->ResetBodySleepTimer(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::InvalidateContactCache()
    {
        return m_system && m_system->InvalidateBodyContactCache(m_worldHandle, m_bodyHandle);
    }

    bool RigidBodyComponent::AddForce(
        const AZ::Vector3& force,
        const bool activate)
    {
        return m_system && m_system->AddForce(m_worldHandle, m_bodyHandle, force, activate);
    }

    bool RigidBodyComponent::AddForceAtPosition(
        const AZ::Vector3& force,
        const WorldPosition& position,
        const bool activate)
    {
        return m_system
            && m_system->AddForceAtPosition(
                m_worldHandle,
                m_bodyHandle,
                force,
                position,
                activate);
    }

    bool RigidBodyComponent::AddForceAndTorque(
        const AZ::Vector3& force,
        const AZ::Vector3& torque,
        const bool activate)
    {
        return m_system
            && m_system->AddForceAndTorque(
                m_worldHandle,
                m_bodyHandle,
                force,
                torque,
                activate);
    }

    bool RigidBodyComponent::AddTorque(
        const AZ::Vector3& torque,
        const bool activate)
    {
        return m_system && m_system->AddTorque(m_worldHandle, m_bodyHandle, torque, activate);
    }

    bool RigidBodyComponent::AddImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->AddImpulse(m_worldHandle, m_bodyHandle, impulse);
    }

    bool RigidBodyComponent::AddImpulseAtPosition(
        const AZ::Vector3& impulse,
        const WorldPosition& position)
    {
        return m_system && m_system->AddImpulseAtPosition(m_worldHandle, m_bodyHandle, impulse, position);
    }

    bool RigidBodyComponent::AddAngularImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system && m_system->AddAngularImpulse(m_worldHandle, m_bodyHandle, impulse);
    }

    bool RigidBodyComponent::ApplyBuoyancyImpulse(
        const BuoyancyConfiguration& configuration)
    {
        return m_system
            && m_system->ApplyBuoyancyImpulse(
                m_worldHandle,
                m_bodyHandle,
                configuration);
    }

    float RigidBodyComponent::GetFriction() const
    {
        float friction = 0.2f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyFriction(m_worldHandle, m_bodyHandle, friction);
        }
        return friction;
    }

    bool RigidBodyComponent::SetFriction(
        const float friction)
    {
        return m_system && m_system->SetBodyFriction(m_worldHandle, m_bodyHandle, friction);
    }

    float RigidBodyComponent::GetRestitution() const
    {
        float restitution = 0.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyRestitution(m_worldHandle, m_bodyHandle, restitution);
        }
        return restitution;
    }

    bool RigidBodyComponent::SetRestitution(
        const float restitution)
    {
        return m_system && m_system->SetBodyRestitution(m_worldHandle, m_bodyHandle, restitution);
    }

    float RigidBodyComponent::GetGravityFactor() const
    {
        float gravityFactor = 1.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyGravityFactor(m_worldHandle, m_bodyHandle, gravityFactor);
        }
        return gravityFactor;
    }

    bool RigidBodyComponent::SetGravityFactor(
        const float gravityFactor)
    {
        return m_system && m_system->SetBodyGravityFactor(m_worldHandle, m_bodyHandle, gravityFactor);
    }

    float RigidBodyComponent::GetMaximumLinearVelocity() const
    {
        float maximumLinearVelocity = 500.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyMaximumLinearVelocity(
                    m_worldHandle,
                    m_bodyHandle,
                    maximumLinearVelocity);
        }
        return maximumLinearVelocity;
    }

    bool RigidBodyComponent::SetMaximumLinearVelocity(
        const float maximumLinearVelocity)
    {
        return m_system
            && m_system->SetBodyMaximumLinearVelocity(
                m_worldHandle,
                m_bodyHandle,
                maximumLinearVelocity);
    }

    float RigidBodyComponent::GetMaximumAngularVelocity() const
    {
        float maximumAngularVelocity = 0.25f * AZ::Constants::Pi * 60.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyMaximumAngularVelocity(
                    m_worldHandle,
                    m_bodyHandle,
                    maximumAngularVelocity);
        }
        return maximumAngularVelocity;
    }

    bool RigidBodyComponent::SetMaximumAngularVelocity(
        const float maximumAngularVelocity)
    {
        return m_system
            && m_system->SetBodyMaximumAngularVelocity(
                m_worldHandle,
                m_bodyHandle,
                maximumAngularVelocity);
    }

    MotionQuality RigidBodyComponent::GetMotionQuality() const
    {
        MotionQuality motionQuality = MotionQuality::Discrete;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyMotionQuality(m_worldHandle, m_bodyHandle, motionQuality);
        }
        return motionQuality;
    }

    bool RigidBodyComponent::SetMotionQuality(
        const MotionQuality motionQuality)
    {
        return m_system && m_system->SetBodyMotionQuality(m_worldHandle, m_bodyHandle, motionQuality);
    }

    bool RigidBodyComponent::IsManifoldReductionEnabled() const
    {
        bool enabled = true;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->IsBodyManifoldReductionEnabled(m_worldHandle, m_bodyHandle, enabled);
        }
        return enabled;
    }

    bool RigidBodyComponent::SetManifoldReductionEnabled(
        const bool enabled)
    {
        return m_system
            && m_system->SetBodyManifoldReductionEnabled(
                m_worldHandle,
                m_bodyHandle,
                enabled);
    }

    bool RigidBodyComponent::IsSensor() const
    {
        bool sensor = false;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->IsBodySensor(m_worldHandle, m_bodyHandle, sensor);
        }
        return sensor;
    }

    bool RigidBodyComponent::SetSensor(
        const bool sensor)
    {
        return m_system && m_system->SetBodySensor(m_worldHandle, m_bodyHandle, sensor);
    }

    float RigidBodyComponent::GetLinearDamping() const
    {
        float linearDamping = 0.05f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyLinearDamping(m_worldHandle, m_bodyHandle, linearDamping);
        }
        return linearDamping;
    }

    bool RigidBodyComponent::SetLinearDamping(
        const float linearDamping)
    {
        return m_system && m_system->SetBodyLinearDamping(m_worldHandle, m_bodyHandle, linearDamping);
    }

    float RigidBodyComponent::GetAngularDamping() const
    {
        float angularDamping = 0.05f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyAngularDamping(m_worldHandle, m_bodyHandle, angularDamping);
        }
        return angularDamping;
    }

    bool RigidBodyComponent::SetAngularDamping(
        const float angularDamping)
    {
        return m_system && m_system->SetBodyAngularDamping(m_worldHandle, m_bodyHandle, angularDamping);
    }

    bool RigidBodyComponent::IsSleepingAllowed() const
    {
        bool sleepingAllowed = true;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->IsBodySleepingAllowed(m_worldHandle, m_bodyHandle, sleepingAllowed);
        }
        return sleepingAllowed;
    }

    bool RigidBodyComponent::SetSleepingAllowed(
        const bool sleepingAllowed)
    {
        return m_system && m_system->SetBodySleepingAllowed(m_worldHandle, m_bodyHandle, sleepingAllowed);
    }

    bool RigidBodyComponent::IsGyroscopicForceEnabled() const
    {
        bool enabled = false;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->IsBodyGyroscopicForceEnabled(m_worldHandle, m_bodyHandle, enabled);
        }
        return enabled;
    }

    bool RigidBodyComponent::SetGyroscopicForceEnabled(
        const bool enabled)
    {
        return m_system && m_system->SetBodyGyroscopicForceEnabled(m_worldHandle, m_bodyHandle, enabled);
    }

    bool RigidBodyComponent::IsKinematicVsNonDynamicCollisionEnabled() const
    {
        bool enabled = false;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->IsBodyKinematicVsNonDynamicCollisionEnabled(
                    m_worldHandle,
                    m_bodyHandle,
                    enabled);
        }
        return enabled;
    }

    bool RigidBodyComponent::SetKinematicVsNonDynamicCollisionEnabled(
        const bool enabled)
    {
        return m_system
            && m_system->SetBodyKinematicVsNonDynamicCollisionEnabled(
                m_worldHandle,
                m_bodyHandle,
                enabled);
    }

    bool RigidBodyComponent::IsEnhancedInternalEdgeRemovalEnabled() const
    {
        bool enabled = false;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->IsBodyEnhancedInternalEdgeRemovalEnabled(
                    m_worldHandle,
                    m_bodyHandle,
                    enabled);
        }
        return enabled;
    }

    bool RigidBodyComponent::SetEnhancedInternalEdgeRemovalEnabled(
        const bool enabled)
    {
        return m_system
            && m_system->SetBodyEnhancedInternalEdgeRemovalEnabled(
                m_worldHandle,
                m_bodyHandle,
                enabled);
    }

    bool RigidBodyComponent::GetSolverStepCounts(
        AZ::u8& velocityStepCount,
        AZ::u8& positionStepCount) const
    {
        if (!m_system)
        {
            return false;
        }

        return m_system->GetBodySolverStepCounts(
            m_worldHandle,
            m_bodyHandle,
            velocityStepCount,
            positionStepCount);
    }

    bool RigidBodyComponent::SetSolverStepCounts(
        const AZ::u8 velocityStepCount,
        const AZ::u8 positionStepCount)
    {
        return m_system
            && m_system->SetBodySolverStepCounts(
                m_worldHandle,
                m_bodyHandle,
                velocityStepCount,
                positionStepCount);
    }

    bool RigidBodyComponent::SetPosition(
        const WorldPosition& position,
        const bool activate)
    {
        return m_system
            && m_system->SetBodyPosition(
                m_worldHandle,
                m_bodyHandle,
                position,
                activate);
    }

    bool RigidBodyComponent::SetRotation(
        const AZ::Quaternion& rotation,
        const bool activate)
    {
        return m_system
            && m_system->SetBodyRotation(
                m_worldHandle,
                m_bodyHandle,
                rotation,
                activate);
    }

    bool RigidBodyComponent::SetTransform(
        const WorldTransform& transform,
        const bool activate)
    {
        return m_system && m_system->SetBodyTransform(m_worldHandle, m_bodyHandle, transform, activate);
    }

    bool RigidBodyComponent::SetTransformWhenChanged(
        const WorldTransform& transform,
        const bool activate)
    {
        return m_system
            && m_system->SetBodyTransformWhenChanged(
                m_worldHandle,
                m_bodyHandle,
                transform,
                activate);
    }

    bool RigidBodyComponent::SetVelocities(
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        return m_system
            && m_system->SetBodyVelocities(
                m_worldHandle,
                m_bodyHandle,
                linearVelocity,
                angularVelocity);
    }

    bool RigidBodyComponent::SetLinearVelocity(
        const AZ::Vector3& linearVelocity)
    {
        return m_system
            && m_system->SetBodyLinearVelocity(
                m_worldHandle,
                m_bodyHandle,
                linearVelocity);
    }

    bool RigidBodyComponent::SetAngularVelocity(
        const AZ::Vector3& angularVelocity)
    {
        return m_system
            && m_system->SetBodyAngularVelocity(
                m_worldHandle,
                m_bodyHandle,
                angularVelocity);
    }

    bool RigidBodyComponent::AddVelocities(
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        return m_system
            && m_system->AddBodyVelocities(
                m_worldHandle,
                m_bodyHandle,
                linearVelocity,
                angularVelocity);
    }

    bool RigidBodyComponent::AddLinearVelocity(
        const AZ::Vector3& linearVelocity)
    {
        return m_system
            && m_system->AddBodyLinearVelocity(
                m_worldHandle,
                m_bodyHandle,
                linearVelocity);
    }

    bool RigidBodyComponent::MoveKinematically(
        const WorldTransform& target,
        const float duration)
    {
        return m_system && m_system->MoveBodyKinematically(m_worldHandle, m_bodyHandle, target, duration);
    }

    bool RigidBodyComponent::UpdateRuntimeConfiguration(
        const BodyRuntimeConfiguration& configuration,
        const bool activate)
    {
        return m_system
            && m_system->UpdateBodyRuntimeConfiguration(
                m_worldHandle,
                m_bodyHandle,
                configuration,
                activate);
    }

    void RigidBodyComponent::Activate()
    {
        m_system = AZ::Interface<ISystem>::Get();
        m_collider = GetEntity()->FindComponent<ColliderComponent>();
        RigidBodyRequestBus::Handler::BusConnect(GetEntityId());
        BodyRequestBus::Handler::BusConnect(GetEntityId());
        BodyNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    void RigidBodyComponent::Deactivate()
    {
        [[maybe_unused]] const bool disabled = DisableSimulation();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        BodyNotificationBus::Handler::BusDisconnect();
        BodyRequestBus::Handler::BusDisconnect();
        RigidBodyRequestBus::Handler::BusDisconnect();
        m_collider = nullptr;
        m_system = nullptr;
    }

    void RigidBodyComponent::OnBodyMoved(
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

    void RigidBodyComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (m_syncingTransform || !m_system || !m_bodyHandle)
        {
            return;
        }

        const float uniformScale = world.GetUniformScale();
        if (!m_collider->UpdateUniformScale(m_bodyHandle, uniformScale))
        {
            return;
        }
        m_uniformScale = uniformScale;
        [[maybe_unused]] const bool updated = m_system->SetBodyTransformWhenChanged(
            m_worldHandle,
            m_bodyHandle,
            Internal::ToWorldTransform(world),
            m_configuration.m_activate);
    }
} // namespace Jolt
