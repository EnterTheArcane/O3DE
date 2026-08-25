/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ConstraintComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    class ConstraintNotificationBusBehaviorHandler final
        : public ConstraintNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            ConstraintNotificationBusBehaviorHandler,
            "{938122A0-70EA-4471-8227-4D3FD2DA1CEC}",
            AZ::SystemAllocator,
            OnConstraintCreated,
            OnConstraintDestroying,
            OnConstraintDestroyed);

        void OnConstraintCreated(
            const WorldHandle worldHandle,
            const ConstraintHandle constraintHandle) override
        {
            Call(FN_OnConstraintCreated, worldHandle, constraintHandle);
        }

        void OnConstraintDestroying(
            const WorldHandle worldHandle,
            const ConstraintHandle constraintHandle) override
        {
            Call(FN_OnConstraintDestroying, worldHandle, constraintHandle);
        }

        void OnConstraintDestroyed(
            const WorldHandle worldHandle,
            const ConstraintHandle constraintHandle) override
        {
            Call(FN_OnConstraintDestroyed, worldHandle, constraintHandle);
        }
    };

    template<class Geometry>
    class ConstraintRuntimeGeometry final
        : public IConstraintRuntimeGeometry
    {
    public:
        AZ_RTTI(
            (ConstraintRuntimeGeometry, ConstraintRuntimeGeometryValueTypeId, Geometry),
            IConstraintRuntimeGeometry);
        AZ_CLASS_ALLOCATOR(ConstraintRuntimeGeometry, AZ::SystemAllocator);

        ConstraintRuntimeGeometry() = default;
        explicit ConstraintRuntimeGeometry(Geometry geometry)
            : m_geometry(AZStd::move(geometry))
        {
        }

        [[nodiscard]]
        bool Resolve(
            ConstraintConfiguration& configuration,
            ConstraintHandle& firstDependencyHandle,
            ConstraintHandle& secondDependencyHandle,
            PathHandle& pathHandle) const override
        {
            if constexpr (AZStd::is_same_v<Geometry, GearConstraintComponentConfiguration>)
            {
                ConstraintRequestBus::EventResult(
                    firstDependencyHandle,
                    m_geometry.m_firstHingeEntityId,
                    &IConstraintRequests::GetConstraintHandle);
                ConstraintRequestBus::EventResult(
                    secondDependencyHandle,
                    m_geometry.m_secondHingeEntityId,
                    &IConstraintRequests::GetConstraintHandle);
                if (!firstDependencyHandle || !secondDependencyHandle)
                {
                    return false;
                }

                configuration.m_geometry = GearConstraintConfiguration{
                    .m_firstHingeConstraintHandle = firstDependencyHandle,
                    .m_secondHingeConstraintHandle = secondDependencyHandle,
                    .m_firstHingeAxis = m_geometry.m_firstHingeAxis,
                    .m_secondHingeAxis = m_geometry.m_secondHingeAxis,
                    .m_ratio = m_geometry.m_ratio,
                    .m_space = m_geometry.m_space,
                };
            }
            else if constexpr (AZStd::is_same_v<Geometry, PathConstraintComponentConfiguration>)
            {
                PathRequestBus::EventResult(
                    pathHandle,
                    m_geometry.m_pathEntityId,
                    &IPathRequests::GetPathHandle);
                if (!pathHandle)
                {
                    return false;
                }

                configuration.m_geometry = PathConstraintConfiguration{
                    .m_pathHandle = pathHandle,
                    .m_pathPosition = m_geometry.m_pathPosition,
                    .m_pathRotation = m_geometry.m_pathRotation,
                    .m_positionMotor = m_geometry.m_positionMotor,
                    .m_maximumFrictionForce = m_geometry.m_maximumFrictionForce,
                    .m_pathFraction = m_geometry.m_pathFraction,
                    .m_targetPathFraction = m_geometry.m_targetPathFraction,
                    .m_targetVelocity = m_geometry.m_targetVelocity,
                    .m_rotationConstraint = m_geometry.m_rotationConstraint,
                };
            }
            else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintComponentConfiguration>)
            {
                ConstraintRequestBus::EventResult(
                    firstDependencyHandle,
                    m_geometry.m_pinionConstraintEntityId,
                    &IConstraintRequests::GetConstraintHandle);
                ConstraintRequestBus::EventResult(
                    secondDependencyHandle,
                    m_geometry.m_rackConstraintEntityId,
                    &IConstraintRequests::GetConstraintHandle);
                if (!firstDependencyHandle || !secondDependencyHandle)
                {
                    return false;
                }

                configuration.m_geometry = RackAndPinionConstraintConfiguration{
                    .m_pinionConstraintHandle = firstDependencyHandle,
                    .m_rackConstraintHandle = secondDependencyHandle,
                    .m_hingeAxis = m_geometry.m_hingeAxis,
                    .m_sliderAxis = m_geometry.m_sliderAxis,
                    .m_ratio = m_geometry.m_ratio,
                    .m_space = m_geometry.m_space,
                };
            }
            else
            {
                configuration.m_geometry = m_geometry;
            }

            return true;
        }

        void RegisterDependencies(
            IComponentDependencyManager& dependencyManager,
            IConstraintDependencyClient& client) const override
        {
            if constexpr (AZStd::is_same_v<Geometry, GearConstraintComponentConfiguration>)
            {
                if (m_geometry.m_firstHingeEntityId.IsValid())
                {
                    dependencyManager.RegisterConstraint(m_geometry.m_firstHingeEntityId, client);
                }
                if (m_geometry.m_secondHingeEntityId.IsValid()
                    && m_geometry.m_secondHingeEntityId != m_geometry.m_firstHingeEntityId)
                {
                    dependencyManager.RegisterConstraint(m_geometry.m_secondHingeEntityId, client);
                }
            }
            else if constexpr (AZStd::is_same_v<Geometry, PathConstraintComponentConfiguration>)
            {
                if (m_geometry.m_pathEntityId.IsValid())
                {
                    dependencyManager.RegisterPath(m_geometry.m_pathEntityId, client);
                }
            }
            else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintComponentConfiguration>)
            {
                if (m_geometry.m_pinionConstraintEntityId.IsValid())
                {
                    dependencyManager.RegisterConstraint(m_geometry.m_pinionConstraintEntityId, client);
                }
                if (m_geometry.m_rackConstraintEntityId.IsValid()
                    && m_geometry.m_rackConstraintEntityId != m_geometry.m_pinionConstraintEntityId)
                {
                    dependencyManager.RegisterConstraint(m_geometry.m_rackConstraintEntityId, client);
                }
            }
        }

        void UnregisterDependencies(
            IComponentDependencyManager& dependencyManager,
            IConstraintDependencyClient& client) const override
        {
            if constexpr (AZStd::is_same_v<Geometry, GearConstraintComponentConfiguration>)
            {
                dependencyManager.UnregisterConstraint(m_geometry.m_firstHingeEntityId, client);
                if (m_geometry.m_secondHingeEntityId.IsValid()
                    && m_geometry.m_secondHingeEntityId != m_geometry.m_firstHingeEntityId)
                {
                    dependencyManager.UnregisterConstraint(m_geometry.m_secondHingeEntityId, client);
                }
            }
            else if constexpr (AZStd::is_same_v<Geometry, PathConstraintComponentConfiguration>)
            {
                dependencyManager.UnregisterPath(m_geometry.m_pathEntityId, client);
            }
            else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintComponentConfiguration>)
            {
                dependencyManager.UnregisterConstraint(m_geometry.m_pinionConstraintEntityId, client);
                if (m_geometry.m_rackConstraintEntityId.IsValid()
                    && m_geometry.m_rackConstraintEntityId != m_geometry.m_pinionConstraintEntityId)
                {
                    dependencyManager.UnregisterConstraint(m_geometry.m_rackConstraintEntityId, client);
                }
            }
        }

        Geometry m_geometry;
    };

    ConstraintComponent::ConstraintComponent(
        ConstraintComponentConfiguration configuration)
    {
        m_configuration.m_firstBodyEntityId = configuration.m_firstBodyEntityId;
        m_configuration.m_secondBodyEntityId = configuration.m_secondBodyEntityId;
        m_configuration.m_userData = configuration.m_userData;
        m_configuration.m_priority = configuration.m_priority;
        m_configuration.m_positionStepCount = configuration.m_positionStepCount;
        m_configuration.m_velocityStepCount = configuration.m_velocityStepCount;
        m_configuration.m_enabled = configuration.m_enabled;
        AZStd::visit(
            [&](auto& geometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                m_configuration.m_geometry =
                    AZStd::make_unique<ConstraintRuntimeGeometry<Geometry>>(AZStd::move(geometry));
            },
            configuration.m_geometry);
    }

    void ConstraintComponent::Reflect(
        AZ::ReflectContext* context)
    {
        ConstraintComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<ConstraintComponent>(*serializeContext))
            {
                return;
            }

            serializeContext->Class<IConstraintRuntimeGeometry>();

            const auto reflectGeometry = [serializeContext]<class Geometry>()
            {
                using RuntimeGeometry = ConstraintRuntimeGeometry<Geometry>;
                serializeContext
                    ->Class<RuntimeGeometry, IConstraintRuntimeGeometry>()
                    ->Field("Geometry", &RuntimeGeometry::m_geometry);
            };
            reflectGeometry.operator()<ConeConstraintConfiguration>();
            reflectGeometry.operator()<CustomConstraintConfiguration>();
            reflectGeometry.operator()<DistanceConstraintConfiguration>();
            reflectGeometry.operator()<FixedConstraintConfiguration>();
            reflectGeometry.operator()<GearConstraintComponentConfiguration>();
            reflectGeometry.operator()<HingeConstraintConfiguration>();
            reflectGeometry.operator()<PathConstraintComponentConfiguration>();
            reflectGeometry.operator()<PointConstraintConfiguration>();
            reflectGeometry.operator()<PulleyConstraintConfiguration>();
            reflectGeometry.operator()<RackAndPinionConstraintComponentConfiguration>();
            reflectGeometry.operator()<SixDofConstraintConfiguration>();
            reflectGeometry.operator()<SliderConstraintConfiguration>();
            reflectGeometry.operator()<SwingTwistConstraintConfiguration>();

            serializeContext
                ->Class<RuntimeConfiguration>()
                ->Field("FirstBodyEntityId", &RuntimeConfiguration::m_firstBodyEntityId)
                ->Field("SecondBodyEntityId", &RuntimeConfiguration::m_secondBodyEntityId)
                ->Field("Geometry", &RuntimeConfiguration::m_geometry)
                ->Field("UserData", &RuntimeConfiguration::m_userData)
                ->Field("Priority", &RuntimeConfiguration::m_priority)
                ->Field("PositionStepCount", &RuntimeConfiguration::m_positionStepCount)
                ->Field("VelocityStepCount", &RuntimeConfiguration::m_velocityStepCount)
                ->Field("Enabled", &RuntimeConfiguration::m_enabled);

            serializeContext
                ->Class<ConstraintComponent, AZ::Component>()
                ->Field("Configuration", &ConstraintComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ConstraintComponent>("Jolt Constraint", "Constrains two Jolt body entities.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ConstraintComponent::m_configuration,
                        "Configuration",
                        "Endpoint entities, geometry, motors, limits, and solver overrides.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintSpace, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintSpace, LocalToCenterOfMass);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintSpace, World);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Cone);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Custom);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Distance);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Fixed);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Gear);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Hinge);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Path);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Point);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Pulley);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, RackAndPinion);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, SixDof);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, Slider);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ConstraintKind, SwingTwist);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotorState, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotorState, Off);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotorState, Position);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotorState, PositionAndVelocity);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MotorState, Velocity);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, ConstrainAroundBinormal);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, ConstrainAroundNormal);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, ConstrainAroundTangent);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, ConstrainToPath);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, Free);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, PathRotationConstraint, FullyConstrained);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, SixDofAxisMode, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SixDofAxisMode, Fixed);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SixDofAxisMode, Free);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SixDofAxisMode, Limited);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, SpringMode, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SpringMode, FrequencyAndDamping);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SpringMode, MassNormalizedStiffnessAndDamping);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SpringMode, StiffnessAndDamping);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, SwingType, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SwingType, Cone);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SwingType, Pyramid);

            behaviorContext->Class<CustomConstraintInfo>("CustomConstraintInfo")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("providerId", BehaviorValueGetter(&CustomConstraintInfo::m_providerId), nullptr)
                ->Property("providerVersion", BehaviorValueGetter(&CustomConstraintInfo::m_providerVersion), nullptr)
                ->Property("maximumRowCount", BehaviorValueGetter(&CustomConstraintInfo::m_maximumRowCount), nullptr)
                ->Property("stateByteCount", BehaviorValueGetter(&CustomConstraintInfo::m_stateByteCount), nullptr);

            behaviorContext->Class<ConstraintSolverConfiguration>("ConstraintSolverConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("priority", JOLT_BEHAVIOR_VALUE_PROPERTY(&ConstraintSolverConfiguration::m_priority))
                ->Property("positionStepCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&ConstraintSolverConfiguration::m_positionStepCount))
                ->Property("velocityStepCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&ConstraintSolverConfiguration::m_velocityStepCount));

            behaviorContext->Class<ConstraintState>("ConstraintState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("firstBodyHandle", BehaviorValueGetter(&ConstraintState::m_firstBodyHandle), nullptr)
                ->Property("secondBodyHandle", BehaviorValueGetter(&ConstraintState::m_secondBodyHandle), nullptr)
                ->Property("entityId", BehaviorValueGetter(&ConstraintState::m_entityId), nullptr)
                ->Property("name", BehaviorValueGetter(&ConstraintState::m_name), nullptr)
                ->Property("userData", BehaviorValueGetter(&ConstraintState::m_userData), nullptr)
                ->Property("kind", BehaviorValueGetter(&ConstraintState::m_kind), nullptr)
                ->Property("solver", BehaviorValueGetter(&ConstraintState::m_solver), nullptr)
                ->Property("active", BehaviorValueGetter(&ConstraintState::m_active), nullptr)
                ->Property("enabled", BehaviorValueGetter(&ConstraintState::m_enabled), nullptr)
                ->Property("isInSimulation", BehaviorValueGetter(&ConstraintState::m_isInSimulation), nullptr);

            behaviorContext->EBus<ConstraintRequestBus>("JoltConstraintRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &IConstraintRequests::EnableSimulation)
                ->Event("DisableSimulation", &IConstraintRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &IConstraintRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IConstraintRequests::GetWorldHandle)
                ->Event("GetConstraintHandle", &IConstraintRequests::GetConstraintHandle)
                ->Event("GetState", &IConstraintRequests::GetState)
                ->Event("GetUserData", &IConstraintRequests::GetUserData)
                ->Event("SetUserData", &IConstraintRequests::SetUserData)
                ->Event("GetDebugDrawSize", &IConstraintRequests::GetDebugDrawSize)
                ->Event("SetDebugDrawSize", &IConstraintRequests::SetDebugDrawSize)
                ->Event("SetEnabled", &IConstraintRequests::SetEnabled)
                ->Event("ResetWarmStart", &IConstraintRequests::ResetWarmStart)
                ->Event("SetHingeTargetOrientation", &IConstraintRequests::SetHingeTargetOrientation);

            behaviorContext->EBus<ConstraintNotificationBus>("JoltConstraintNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<ConstraintNotificationBusBehaviorHandler>();

            behaviorContext->Class<ConstraintComponent>("Jolt::ConstraintComponent")
                ->RequestBus("JoltConstraintRequestBus");
        }
    }

    void ConstraintComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltConstraintService"));
    }

    void ConstraintComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltConstraintService"));
    }

    bool ConstraintComponent::EnableSimulation()
    {
        if (m_constraintHandle)
        {
            return true;
        }
        if (!m_system
            || !m_configuration.m_firstBodyEntityId.IsValid()
            || !m_configuration.m_secondBodyEntityId.IsValid()
            || m_configuration.m_firstBodyEntityId == m_configuration.m_secondBodyEntityId)
        {
            return false;
        }

        WorldHandle firstWorldHandle;
        WorldHandle secondWorldHandle;
        BodyRequestBus::EventResult(
            firstWorldHandle,
            m_configuration.m_firstBodyEntityId,
            &IBodyRequests::GetWorldHandle);
        BodyRequestBus::EventResult(
            secondWorldHandle,
            m_configuration.m_secondBodyEntityId,
            &IBodyRequests::GetWorldHandle);
        BodyRequestBus::EventResult(
            m_firstBodyHandle,
            m_configuration.m_firstBodyEntityId,
            &IBodyRequests::GetBodyHandle);
        BodyRequestBus::EventResult(
            m_secondBodyHandle,
            m_configuration.m_secondBodyEntityId,
            &IBodyRequests::GetBodyHandle);
        if (!m_firstBodyHandle
            || !m_secondBodyHandle
            || firstWorldHandle != secondWorldHandle)
        {
            return false;
        }

        ConstraintConfiguration configuration;
        configuration.m_firstBodyHandle = m_firstBodyHandle;
        configuration.m_secondBodyHandle = m_secondBodyHandle;
        configuration.m_entityId = GetEntityId();
        configuration.m_name = AZ::Name(GetEntity()->GetName());
        configuration.m_userData = m_configuration.m_userData;
        configuration.m_priority = m_configuration.m_priority;
        configuration.m_positionStepCount = m_configuration.m_positionStepCount;
        configuration.m_velocityStepCount = m_configuration.m_velocityStepCount;
        configuration.m_enabled = m_configuration.m_enabled;

        if (!m_configuration.m_geometry
            || !m_configuration.m_geometry->Resolve(
                configuration,
                m_firstDependencyHandle,
                m_secondDependencyHandle,
                m_pathHandle))
        {
            return false;
        }

        m_worldHandle = firstWorldHandle;
        m_constraintHandle = m_system->CreateConstraint(m_worldHandle, configuration);
        if (!m_constraintHandle)
        {
            return false;
        }
        ConstraintNotificationBus::Event(
            GetEntityId(),
            &IConstraintNotifications::OnConstraintCreated,
            m_worldHandle,
            m_constraintHandle);
        return true;
    }

    bool ConstraintComponent::DisableSimulation()
    {
        return DestroySimulation(false);
    }

    bool ConstraintComponent::DestroySimulation(const bool mandatory)
    {
        if (!m_constraintHandle)
        {
            return true;
        }

        RuntimeImplementation* system = m_system;
        const WorldHandle worldHandle = m_worldHandle;
        const ConstraintHandle constraintHandle = m_constraintHandle;
        ResourceDestructionPlan plan;
        const bool prepared = PrepareSimulationDestruction(plan);
        if (!prepared && !mandatory)
        {
            return false;
        }
        if (!system->ReserveConstraintDestruction(plan))
        {
            if (!mandatory)
            {
                return false;
            }
            if (system->IsConstraintDestructionReserved(worldHandle, constraintHandle))
            {
                return true;
            }

            system->DeferConstraintDestruction(
                AZStd::move(plan),
                ResourceDestructionReservation::Pending);
            return true;
        }

        plan.NotifyDestroying();
        if (!system->DestroyReservedConstraints(plan))
        {
            system->DeferConstraintDestruction(
                AZStd::move(plan),
                ResourceDestructionReservation::Complete);
            return true;
        }
        plan.NotifyDestroyed();
        return true;
    }

    bool ConstraintComponent::IsSimulationEnabled() const
    {
        return m_system && m_constraintHandle;
    }

    WorldHandle ConstraintComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    ConstraintHandle ConstraintComponent::GetConstraintHandle() const
    {
        return m_constraintHandle;
    }

    ConstraintState ConstraintComponent::GetState() const
    {
        ConstraintState state;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetConstraintState(m_worldHandle, m_constraintHandle, state);
        }
        return state;
    }

    AZ::u64 ConstraintComponent::GetUserData() const
    {
        AZ::u64 userData = m_configuration.m_userData;
        if (m_system && m_constraintHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetConstraintUserData(m_worldHandle, m_constraintHandle, userData);
        }
        return userData;
    }

    bool ConstraintComponent::SetUserData(
        const AZ::u64 userData)
    {
        if (m_constraintHandle
            && (!m_system || !m_system->SetConstraintUserData(m_worldHandle, m_constraintHandle, userData)))
        {
            return false;
        }

        m_configuration.m_userData = userData;
        return true;
    }

    float ConstraintComponent::GetDebugDrawSize() const
    {
        float debugDrawSize = 1.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetConstraintDebugDrawSize(
                    m_worldHandle,
                    m_constraintHandle,
                    debugDrawSize);
        }
        return debugDrawSize;
    }

    bool ConstraintComponent::SetDebugDrawSize(
        const float debugDrawSize)
    {
        return m_system
            && m_system->SetConstraintDebugDrawSize(
                m_worldHandle,
                m_constraintHandle,
                debugDrawSize);
    }

    bool ConstraintComponent::SetEnabled(
        const bool enabled)
    {
        if (!m_system || !m_system->SetConstraintEnabled(m_worldHandle, m_constraintHandle, enabled))
        {
            return false;
        }

        m_configuration.m_enabled = enabled;
        return true;
    }

    bool ConstraintComponent::ResetWarmStart()
    {
        return m_system && m_system->ResetConstraintWarmStart(m_worldHandle, m_constraintHandle);
    }

    bool ConstraintComponent::SetHingeTargetOrientation(
        const AZ::Quaternion& targetOrientation)
    {
        return m_system
            && m_system->SetHingeTargetOrientation(
                m_worldHandle,
                m_constraintHandle,
                targetOrientation);
    }

    void ConstraintComponent::Activate()
    {
        m_system = GetRuntime();
        m_dependencyManager = AZ::Interface<IComponentDependencyManager>::Get();
        ConstraintRequestBus::Handler::BusConnect(GetEntityId());
        if (!m_dependencyManager)
        {
            return;
        }

        m_dependencyManager->RegisterBody(m_configuration.m_firstBodyEntityId, *this);
        if (m_configuration.m_secondBodyEntityId.IsValid()
            && m_configuration.m_secondBodyEntityId != m_configuration.m_firstBodyEntityId)
        {
            m_dependencyManager->RegisterBody(m_configuration.m_secondBodyEntityId, *this);
        }

        if (m_configuration.m_geometry)
        {
            m_configuration.m_geometry->RegisterDependencies(*m_dependencyManager, *this);
        }
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    void ConstraintComponent::Deactivate()
    {
        [[maybe_unused]] const bool destroyed = DestroySimulation(true);
        AZ_Assert(destroyed, "Mandatory constraint teardown must retain or release every native resource.");
        if (m_dependencyManager)
        {
            m_dependencyManager->UnregisterBody(m_configuration.m_firstBodyEntityId, *this);
            if (m_configuration.m_secondBodyEntityId.IsValid()
                && m_configuration.m_secondBodyEntityId != m_configuration.m_firstBodyEntityId)
            {
                m_dependencyManager->UnregisterBody(m_configuration.m_secondBodyEntityId, *this);
            }
            if (m_configuration.m_geometry)
            {
                m_configuration.m_geometry->UnregisterDependencies(*m_dependencyManager, *this);
            }
        }
        ConstraintRequestBus::Handler::BusDisconnect();
        m_dependencyManager = nullptr;
        m_system = nullptr;
    }

    void ConstraintComponent::OnBodyDependencyCreated(
        [[maybe_unused]] const WorldHandle worldHandle,
        [[maybe_unused]] const BodyHandle bodyHandle)
    {
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    bool ConstraintComponent::PrepareBodyDependencyDestruction(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        ResourceDestructionPlan& plan)
    {
        if (worldHandle == m_worldHandle
            && (bodyHandle == m_firstBodyHandle || bodyHandle == m_secondBodyHandle))
        {
            return PrepareSimulationDestruction(plan);
        }
        return true;
    }

    void ConstraintComponent::OnConstraintDependencyCreated(
        [[maybe_unused]] const WorldHandle worldHandle,
        [[maybe_unused]] const ConstraintHandle constraintHandle)
    {
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    bool ConstraintComponent::PrepareConstraintDependencyDestruction(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        ResourceDestructionPlan& plan)
    {
        if (worldHandle == m_worldHandle
            && (constraintHandle == m_firstDependencyHandle || constraintHandle == m_secondDependencyHandle))
        {
            return PrepareSimulationDestruction(plan);
        }
        return true;
    }

    void ConstraintComponent::OnPathDependencyCreated(
        [[maybe_unused]] const PathHandle pathHandle)
    {
        [[maybe_unused]] const bool enabled = EnableSimulation();
    }

    bool ConstraintComponent::PreparePathDependencyDestruction(
        const PathHandle pathHandle,
        ResourceDestructionPlan& plan)
    {
        if (pathHandle == m_pathHandle)
        {
            return PrepareSimulationDestruction(plan);
        }
        return true;
    }

    bool ConstraintComponent::PrepareSimulationDestruction(ResourceDestructionPlan& plan)
    {
        if (!m_constraintHandle)
        {
            return true;
        }
        const ResourceDestructionObserver observer{
            .m_context = this,
            .m_entityId = GetEntityId(),
            .m_componentId = GetId(),
            .m_notify = &ConstraintComponent::NotifyResourceDestruction,
        };
        if (!plan.AddConstraint(observer, m_worldHandle, m_constraintHandle))
        {
            return false;
        }
        return !m_dependencyManager
            || m_dependencyManager->PrepareConstraintDestruction(
                GetEntityId(),
                m_worldHandle,
                m_constraintHandle,
                plan);
    }

    void ConstraintComponent::NotifyResourceDestruction(
        void* context,
        const AZ::EntityId entityId,
        const AZ::ComponentId componentId,
        const ResourceDestructionPhase phase)
    {
        auto* componentApplication = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        AZ::Entity* entity = nullptr;
        if (componentApplication)
        {
            entity = componentApplication->FindEntity(entityId);
        }
        ConstraintComponent* component = nullptr;
        if (entity)
        {
            component = azrtti_cast<ConstraintComponent*>(entity->FindComponent(componentId));
        }
        if (!component || component != context)
        {
            return;
        }

        if (phase == ResourceDestructionPhase::Destroying)
        {
            ConstraintNotificationBus::Event(
                component->GetEntityId(),
                &IConstraintNotifications::OnConstraintDestroying,
                component->m_worldHandle,
                component->m_constraintHandle);
            return;
        }

        const ConstraintHandle constraintHandle = component->m_constraintHandle;
        component->m_constraintHandle = {};
        component->m_firstBodyHandle = {};
        component->m_secondBodyHandle = {};
        component->m_firstDependencyHandle = {};
        component->m_secondDependencyHandle = {};
        component->m_pathHandle = {};
        ConstraintNotificationBus::Event(
            component->GetEntityId(),
            &IConstraintNotifications::OnConstraintDestroyed,
            component->m_worldHandle,
            constraintHandle);
    }
} // namespace Jolt
