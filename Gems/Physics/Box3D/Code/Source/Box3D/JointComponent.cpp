/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/JointComponent.h>

#include <Box3D/SystemInternal.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Box3D
{
    namespace
    {
        template<typename Value, class Variant>
        Value GetVariantValue(
            const Variant& variant)
        {
            const Value* value = AZStd::get_if<Value>(&variant);
            if (value)
            {
                return *value;
            }

            return {};
        }
    } // namespace

    class JointNotificationBusBehaviorHandler final
        : public JointNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            JointNotificationBusBehaviorHandler, "{F1FEC305-1898-48FC-9F37-D3F976026517}", AZ::SystemAllocator, OnThresholdExceeded);

        void OnThresholdExceeded(
            const JointThresholdEvent& event) override
        {
            Call(FN_OnThresholdExceeded, event);
        }
    };

    JointComponent::JointComponent(
        JointConfiguration configuration,
        const AZ::EntityId parentEntityId)
        : m_configuration(AZStd::move(configuration))
        , m_parentEntityId(parentEntityId)
    {
    }

    void JointComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<JointComponent, AZ::Component>()
                ->Field("Configuration", &JointComponent::m_configuration)
                ->Field("ParentEntityId", &JointComponent::m_parentEntityId);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<JointComponent>("Box3D Joint", "Constrains this body to another body or the world")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &JointComponent::m_parentEntityId, "Parent entity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &JointComponent::m_configuration, "Configuration", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {

            behaviorContext->EBus<JointRequestBus>("Box3DJointRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("EnableSimulation", &JointRequests::EnableSimulation)
                ->Event("DisableSimulation", &JointRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &JointRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &JointRequests::GetWorldHandle)
                ->Event("GetJointHandle", &JointRequests::GetJointHandle)
                ->Event("GetMeasurements", &JointRequests::GetMeasurements)
                ->Event("WakeBodies", &JointRequests::WakeBodies)
                ->Event("GetParallelConfiguration", &JointRequests::GetParallelConfiguration)
                ->Event("UpdateParallelConfiguration", &JointRequests::UpdateParallelConfiguration)
                ->Event("GetDistanceConfiguration", &JointRequests::GetDistanceConfiguration)
                ->Event("UpdateDistanceConfiguration", &JointRequests::UpdateDistanceConfiguration)
                ->Event("GetFilterConfiguration", &JointRequests::GetFilterConfiguration)
                ->Event("UpdateFilterConfiguration", &JointRequests::UpdateFilterConfiguration)
                ->Event("GetMotorConfiguration", &JointRequests::GetMotorConfiguration)
                ->Event("UpdateMotorConfiguration", &JointRequests::UpdateMotorConfiguration)
                ->Event("GetPrismaticConfiguration", &JointRequests::GetPrismaticConfiguration)
                ->Event("UpdatePrismaticConfiguration", &JointRequests::UpdatePrismaticConfiguration)
                ->Event("GetRevoluteConfiguration", &JointRequests::GetRevoluteConfiguration)
                ->Event("UpdateRevoluteConfiguration", &JointRequests::UpdateRevoluteConfiguration)
                ->Event("GetSphericalConfiguration", &JointRequests::GetSphericalConfiguration)
                ->Event("UpdateSphericalConfiguration", &JointRequests::UpdateSphericalConfiguration)
                ->Event("GetWeldConfiguration", &JointRequests::GetWeldConfiguration)
                ->Event("UpdateWeldConfiguration", &JointRequests::UpdateWeldConfiguration)
                ->Event("GetWheelConfiguration", &JointRequests::GetWheelConfiguration)
                ->Event("UpdateWheelConfiguration", &JointRequests::UpdateWheelConfiguration)
                ->Event("GetDistanceState", &JointRequests::GetDistanceState)
                ->Event("GetPrismaticState", &JointRequests::GetPrismaticState)
                ->Event("GetRevoluteState", &JointRequests::GetRevoluteState)
                ->Event("GetSphericalState", &JointRequests::GetSphericalState)
                ->Event("GetWheelState", &JointRequests::GetWheelState);

            behaviorContext->EBus<JointNotificationBus>("Box3DJointNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Handler<JointNotificationBusBehaviorHandler>();

            behaviorContext->Class<JointComponent>("Box3D::JointComponent")->RequestBus("Box3DJointRequestBus");
        }
    }

    void JointComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DJointService"));
    }

    void JointComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DJointService"));
    }

    void JointComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("Box3DBodyService"));
    }

    bool JointComponent::EnableSimulation()
    {
        if (m_jointHandle.IsValid())
        {
            return true;
        }
        if (!m_system)
        {
            return false;
        }
        JointConfiguration resolved;
        if (!ResolveConfiguration(m_configuration, resolved, m_worldHandle))
        {
            return false;
        }
        m_jointHandle = m_system->CreateJoint(m_worldHandle, resolved);
        if (!m_jointHandle.IsValid())
        {
            return false;
        }
        if (m_system->SetJointEntityId(m_worldHandle, m_jointHandle, GetEntityId()))
        {
            return true;
        }
        [[maybe_unused]] const bool destroyed = m_system->DestroyJoint(m_worldHandle, m_jointHandle);
        m_jointHandle = {};
        return false;
    }

    bool JointComponent::DisableSimulation()
    {
        if (!m_jointHandle.IsValid())
        {
            return true;
        }
        if (!m_system->DestroyJoint(m_worldHandle, m_jointHandle))
        {
            return false;
        }
        m_jointHandle = {};
        return true;
    }

    bool JointComponent::IsSimulationEnabled() const
    {
        return m_system && m_jointHandle.IsValid();
    }

    WorldHandle JointComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    JointHandle JointComponent::GetJointHandle() const
    {
        return m_jointHandle;
    }

    JointConfiguration JointComponent::GetConfiguration() const
    {
        return m_configuration;
    }

    bool JointComponent::UpdateConfiguration(
        const JointConfiguration& configuration)
    {
        if (m_system && m_jointHandle.IsValid())
        {
            JointConfiguration resolved;
            WorldHandle worldHandle;
            if (!ResolveConfiguration(configuration, resolved, worldHandle)
                || worldHandle != m_worldHandle
                || !m_system->UpdateJoint(m_worldHandle, m_jointHandle, resolved))
            {
                return false;
            }
        }
        m_configuration = configuration;
        return true;
    }

    JointMeasurements JointComponent::GetMeasurements() const
    {
        JointMeasurements measurements;
        if (m_system)
        {
            [[maybe_unused]] const bool measurementsFound = m_system->GetJointMeasurements(m_worldHandle, m_jointHandle, measurements);
        }
        return measurements;
    }

    bool JointComponent::WakeBodies()
    {
        return m_system && m_system->WakeJointBodies(m_worldHandle, m_jointHandle);
    }

    ParallelJointConfiguration JointComponent::GetParallelConfiguration() const
    {
        return GetVariantValue<ParallelJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateParallelConfiguration(
        const ParallelJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    DistanceJointConfiguration JointComponent::GetDistanceConfiguration() const
    {
        return GetVariantValue<DistanceJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateDistanceConfiguration(
        const DistanceJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    FilterJointConfiguration JointComponent::GetFilterConfiguration() const
    {
        return GetVariantValue<FilterJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateFilterConfiguration(
        const FilterJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    MotorJointConfiguration JointComponent::GetMotorConfiguration() const
    {
        return GetVariantValue<MotorJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateMotorConfiguration(
        const MotorJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    PrismaticJointConfiguration JointComponent::GetPrismaticConfiguration() const
    {
        return GetVariantValue<PrismaticJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdatePrismaticConfiguration(
        const PrismaticJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    RevoluteJointConfiguration JointComponent::GetRevoluteConfiguration() const
    {
        return GetVariantValue<RevoluteJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateRevoluteConfiguration(
        const RevoluteJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    SphericalJointConfiguration JointComponent::GetSphericalConfiguration() const
    {
        return GetVariantValue<SphericalJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateSphericalConfiguration(
        const SphericalJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    WeldJointConfiguration JointComponent::GetWeldConfiguration() const
    {
        return GetVariantValue<WeldJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateWeldConfiguration(
        const WeldJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    WheelJointConfiguration JointComponent::GetWheelConfiguration() const
    {
        return GetVariantValue<WheelJointConfiguration>(m_configuration);
    }

    bool JointComponent::UpdateWheelConfiguration(
        const WheelJointConfiguration& configuration)
    {
        return UpdateConfiguration(configuration);
    }

    DistanceJointState JointComponent::GetDistanceState() const
    {
        return GetVariantValue<DistanceJointState>(GetMeasurements().m_state);
    }

    PrismaticJointState JointComponent::GetPrismaticState() const
    {
        return GetVariantValue<PrismaticJointState>(GetMeasurements().m_state);
    }

    RevoluteJointState JointComponent::GetRevoluteState() const
    {
        return GetVariantValue<RevoluteJointState>(GetMeasurements().m_state);
    }

    SphericalJointState JointComponent::GetSphericalState() const
    {
        return GetVariantValue<SphericalJointState>(GetMeasurements().m_state);
    }

    WheelJointState JointComponent::GetWheelState() const
    {
        return GetVariantValue<WheelJointState>(GetMeasurements().m_state);
    }

    void JointComponent::Activate()
    {
        m_system = azrtti_cast<System*>(AZ::Interface<ISystem>::Get());
        AZ_Error("Box3D", m_system, "Box3D SystemComponent must be active before joint components.");
        JointRequestBus::Handler::BusConnect(GetEntityId());
        RigidBodyNotificationBus::MultiHandler::BusConnect(GetEntityId());
        if (m_parentEntityId.IsValid() && m_parentEntityId != GetEntityId())
        {
            RigidBodyNotificationBus::MultiHandler::BusConnect(m_parentEntityId);
        }
        EnableSimulation();
    }

    void JointComponent::Deactivate()
    {
        RigidBodyNotificationBus::MultiHandler::BusDisconnect();
        JointRequestBus::Handler::BusDisconnect();
        DisableSimulation();
        m_system = nullptr;
        m_worldHandle = {};
    }

    void JointComponent::OnBodyCreated(
        WorldHandle,
        BodyHandle)
    {
        DisableSimulation();
        EnableSimulation();
    }

    void JointComponent::OnBodyDestroyed(
        WorldHandle,
        BodyHandle)
    {
        DisableSimulation();
    }

    bool JointComponent::ResolveConfiguration(
        const JointConfiguration& source,
        JointConfiguration& resolved,
        WorldHandle& worldHandle) const
    {
        WorldHandle childWorldHandle;
        BodyHandle childBodyHandle;
        RigidBodyRequestBus::EventResult(childWorldHandle, GetEntityId(), &RigidBodyRequests::GetWorldHandle);
        RigidBodyRequestBus::EventResult(childBodyHandle, GetEntityId(), &RigidBodyRequests::GetBodyHandle);
        if (!childWorldHandle.IsValid() || !childBodyHandle.IsValid())
        {
            return false;
        }

        BodyHandle parentBodyHandle;
        if (m_parentEntityId.IsValid())
        {
            WorldHandle parentWorldHandle;
            RigidBodyRequestBus::EventResult(parentWorldHandle, m_parentEntityId, &RigidBodyRequests::GetWorldHandle);
            RigidBodyRequestBus::EventResult(parentBodyHandle, m_parentEntityId, &RigidBodyRequests::GetBodyHandle);
            if (parentWorldHandle != childWorldHandle || !parentBodyHandle.IsValid())
            {
                return false;
            }
        }

        resolved = source;
        AZStd::visit(
            [parentBodyHandle, childBodyHandle](auto& configuration)
            {
                configuration.m_common.m_parentBody = parentBodyHandle;
                configuration.m_common.m_childBody = childBodyHandle;
            },
            resolved);
        worldHandle = childWorldHandle;
        return true;
    }
} // namespace Box3D
