/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/ComponentDependencyManager.h>
#include <Jolt/TypeIds.h>
#include <Jolt/VehicleBus.h>
#include <Jolt/VehicleComponentConfiguration.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class RuntimeImplementation;

    class JOLT_API VehicleComponentBase
        : private IBodyDependencyClient
    {
    public:
        virtual ~VehicleComponentBase() = default;

        bool ApplyEngineDamping(float deltaTime);

        bool ApplyEngineTorque(
            float torque,
            float deltaTime);

        [[nodiscard]]
        float CalculateEngineTorque(float acceleration) const;

        bool EnableSimulation();

        bool DisableSimulation();

        [[nodiscard]]
        bool IsSimulationEnabled() const;

        [[nodiscard]]
        VehicleHandle GetVehicleHandle() const;

        [[nodiscard]]
        VehicleCollisionConfiguration GetCollisionConfiguration() const;

        [[nodiscard]]
        float GetDifferentialLimitedSlipRatio() const;

        [[nodiscard]]
        VehicleEngineConfiguration GetEngineConfiguration() const;

        [[nodiscard]]
        VehiclePowertrainState GetPowertrainState() const;

        [[nodiscard]]
        VehicleRuntimeConfiguration GetRuntimeConfiguration() const;

        [[nodiscard]]
        VehicleTransmissionConfiguration GetTransmissionConfiguration() const;

        [[nodiscard]]
        WheelBasis GetWheelLocalBasis(AZ::u32 wheelIndex) const;

        [[nodiscard]]
        AZ::Transform GetWheelLocalTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const;

        [[nodiscard]]
        WorldTransform GetWheelWorldTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const;

        [[nodiscard]]
        AZStd::vector<VehicleAntiRollBarConfiguration> CopyAntiRollBars() const;

        [[nodiscard]]
        AZStd::vector<VehicleDifferentialConfiguration> CopyDifferentials() const;

        bool SetCallbacks(ExtensionHandle extensionHandle);

        bool SetCollisionFilter(ExtensionHandle extensionHandle);

        bool SetDifferentialLimitedSlipRatio(float ratio);

        bool SetPowertrainControl(const VehiclePowertrainControl& control);

        bool SetWheelMotion(
            AZ::u32 wheelIndex,
            const WheelMotion& motion);

        bool UpdateAntiRollBars(const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars);

        bool UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration);

        bool UpdateDifferentials(const AZStd::vector<VehicleDifferentialConfiguration>& differentials);

        bool UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration);

        bool UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration);

        bool UpdateTransmissionConfiguration(const VehicleTransmissionConfiguration& configuration);

    protected:
        void ActivateVehicle(
            AZ::EntityId entityId,
            bool enabled);

        void DeactivateVehicle();

        [[nodiscard]]
        RuntimeImplementation* GetSystem() const;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const;

    private:
        struct CallbackBindings final
        {
            ExtensionHandle m_callbacks;
            ExtensionHandle m_collisionFilter;
        };

        virtual VehicleHandle CreateVehicle(
            RuntimeImplementation& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        bool DestroySimulation(bool mandatory);

        [[nodiscard]]
        virtual AZ::ComponentId GetComponentId() const = 0;

        void OnBodyDependencyCreated(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool PrepareBodyDependencyDestruction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ResourceDestructionPlan& plan) override;

        static void NotifyResourceDestruction(
            void* context,
            AZ::EntityId entityId,
            AZ::ComponentId componentId,
            ResourceDestructionPhase phase);

        AZ::EntityId m_entityId;

        RuntimeImplementation* m_system = nullptr;
        IComponentDependencyManager* m_dependencyManager = nullptr;
        AZStd::unique_ptr<CallbackBindings> m_callbackBindings;

        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        VehicleHandle m_vehicleHandle;

        bool m_enabled = true;
    };

    class JOLT_API WheeledVehicleComponent final
        : public AZ::Component
        , public WheeledVehicleRequestBus::Handler
        , private VehicleComponentBase
    {
    public:
        AZ_COMPONENT(WheeledVehicleComponent, WheeledVehicleComponentTypeId);

        WheeledVehicleComponent();
        explicit WheeledVehicleComponent(WheeledVehicleComponentConfiguration configuration);
        ~WheeledVehicleComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool ApplyEngineDamping(float deltaTime) override;

        bool ApplyEngineTorque(
            float torque,
            float deltaTime) override;

        [[nodiscard]]
        float CalculateEngineTorque(float acceleration) const override;

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        VehicleHandle GetVehicleHandle() const override;

        [[nodiscard]]
        VehicleCollisionConfiguration GetCollisionConfiguration() const override;

        [[nodiscard]]
        float GetDifferentialLimitedSlipRatio() const override;

        [[nodiscard]]
        VehicleEngineConfiguration GetEngineConfiguration() const override;

        [[nodiscard]]
        VehiclePowertrainState GetPowertrainState() const override;

        [[nodiscard]]
        VehicleRuntimeConfiguration GetRuntimeConfiguration() const override;

        [[nodiscard]]
        VehicleTransmissionConfiguration GetTransmissionConfiguration() const override;

        [[nodiscard]]
        WheelBasis GetWheelLocalBasis(AZ::u32 wheelIndex) const override;

        [[nodiscard]]
        AZ::Transform GetWheelLocalTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const override;

        [[nodiscard]]
        WorldTransform GetWheelWorldTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const override;

        [[nodiscard]]
        AZStd::vector<VehicleAntiRollBarConfiguration> CopyAntiRollBars() const override;

        [[nodiscard]]
        AZStd::vector<VehicleDifferentialConfiguration> CopyDifferentials() const override;

        bool SetCallbacks(ExtensionHandle extensionHandle) override;

        bool SetCollisionFilter(ExtensionHandle extensionHandle) override;

        bool SetDifferentialLimitedSlipRatio(float ratio) override;

        bool SetPowertrainControl(const VehiclePowertrainControl& control) override;

        [[nodiscard]]
        QueryResult QueryState(
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const override;

        [[nodiscard]]
        WheeledVehicleState GetState() const override;

        [[nodiscard]]
        AZStd::vector<WheelState> CopyWheelStates() const override;

        bool SetInput(const WheeledVehicleInput& input) override;

        bool SetWheelMotion(
            AZ::u32 wheelIndex,
            const WheelMotion& motion) override;

        bool UpdateAntiRollBars(const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars) override;

        bool UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration) override;

        bool UpdateDifferentials(const AZStd::vector<VehicleDifferentialConfiguration>& differentials) override;

        bool UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration) override;

        bool UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration) override;

        bool UpdateTransmissionConfiguration(const VehicleTransmissionConfiguration& configuration) override;

    private:
        friend class VehicleComponentBase;

        void Activate() override;

        void Deactivate() override;

        VehicleHandle CreateVehicle(
            RuntimeImplementation& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        AZ::ComponentId GetComponentId() const override
        {
            return GetId();
        }

        AZStd::unique_ptr<WheeledVehicleComponentConfiguration> m_configuration;
    };

    class JOLT_API MotorcycleComponent final
        : public AZ::Component
        , public MotorcycleRequestBus::Handler
        , private VehicleComponentBase
    {
    public:
        AZ_COMPONENT(MotorcycleComponent, MotorcycleComponentTypeId);

        MotorcycleComponent();
        explicit MotorcycleComponent(MotorcycleComponentConfiguration configuration);
        ~MotorcycleComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool ApplyEngineDamping(float deltaTime) override;

        bool ApplyEngineTorque(
            float torque,
            float deltaTime) override;

        [[nodiscard]]
        float CalculateEngineTorque(float acceleration) const override;

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        VehicleHandle GetVehicleHandle() const override;

        [[nodiscard]]
        VehicleCollisionConfiguration GetCollisionConfiguration() const override;

        [[nodiscard]]
        float GetDifferentialLimitedSlipRatio() const override;

        [[nodiscard]]
        VehicleEngineConfiguration GetEngineConfiguration() const override;

        [[nodiscard]]
        VehiclePowertrainState GetPowertrainState() const override;

        [[nodiscard]]
        VehicleRuntimeConfiguration GetRuntimeConfiguration() const override;

        [[nodiscard]]
        VehicleTransmissionConfiguration GetTransmissionConfiguration() const override;

        [[nodiscard]]
        WheelBasis GetWheelLocalBasis(AZ::u32 wheelIndex) const override;

        [[nodiscard]]
        AZ::Transform GetWheelLocalTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const override;

        [[nodiscard]]
        WorldTransform GetWheelWorldTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const override;

        [[nodiscard]]
        AZStd::vector<VehicleAntiRollBarConfiguration> CopyAntiRollBars() const override;

        [[nodiscard]]
        AZStd::vector<VehicleDifferentialConfiguration> CopyDifferentials() const override;

        bool SetCallbacks(ExtensionHandle extensionHandle) override;

        bool SetCollisionFilter(ExtensionHandle extensionHandle) override;

        bool SetDifferentialLimitedSlipRatio(float ratio) override;

        bool SetPowertrainControl(const VehiclePowertrainControl& control) override;

        [[nodiscard]]
        QueryResult QueryState(
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const override;

        [[nodiscard]]
        MotorcycleState GetState() const override;

        [[nodiscard]]
        AZStd::vector<WheelState> CopyWheelStates() const override;

        bool SetInput(const WheeledVehicleInput& input) override;

        bool SetWheelMotion(
            AZ::u32 wheelIndex,
            const WheelMotion& motion) override;

        bool UpdateController(
            const MotorcycleControllerUpdateConfiguration& configuration) override;

        bool UpdateAntiRollBars(const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars) override;

        bool UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration) override;

        bool UpdateDifferentials(const AZStd::vector<VehicleDifferentialConfiguration>& differentials) override;

        bool UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration) override;

        bool UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration) override;

        bool UpdateTransmissionConfiguration(const VehicleTransmissionConfiguration& configuration) override;

    private:
        friend class VehicleComponentBase;

        void Activate() override;

        void Deactivate() override;

        VehicleHandle CreateVehicle(
            RuntimeImplementation& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        AZ::ComponentId GetComponentId() const override
        {
            return GetId();
        }

        AZStd::unique_ptr<MotorcycleComponentConfiguration> m_configuration;
    };

    class JOLT_API TrackedVehicleComponent final
        : public AZ::Component
        , public TrackedVehicleRequestBus::Handler
        , private VehicleComponentBase
    {
    public:
        AZ_COMPONENT(TrackedVehicleComponent, TrackedVehicleComponentTypeId);

        TrackedVehicleComponent();
        explicit TrackedVehicleComponent(TrackedVehicleComponentConfiguration configuration);
        ~TrackedVehicleComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool ApplyEngineDamping(float deltaTime) override;

        bool ApplyEngineTorque(
            float torque,
            float deltaTime) override;

        [[nodiscard]]
        float CalculateEngineTorque(float acceleration) const override;

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        VehicleHandle GetVehicleHandle() const override;

        [[nodiscard]]
        VehicleCollisionConfiguration GetCollisionConfiguration() const override;

        [[nodiscard]]
        float GetDifferentialLimitedSlipRatio() const override;

        [[nodiscard]]
        VehicleEngineConfiguration GetEngineConfiguration() const override;

        [[nodiscard]]
        VehiclePowertrainState GetPowertrainState() const override;

        [[nodiscard]]
        VehicleRuntimeConfiguration GetRuntimeConfiguration() const override;

        [[nodiscard]]
        VehicleTransmissionConfiguration GetTransmissionConfiguration() const override;

        [[nodiscard]]
        VehicleTrackConfiguration GetTrackConfiguration(AZ::u32 trackIndex) const override;

        [[nodiscard]]
        WheelBasis GetWheelLocalBasis(AZ::u32 wheelIndex) const override;

        [[nodiscard]]
        AZ::Transform GetWheelLocalTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const override;

        [[nodiscard]]
        WorldTransform GetWheelWorldTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const override;

        [[nodiscard]]
        AZStd::vector<VehicleAntiRollBarConfiguration> CopyAntiRollBars() const override;

        [[nodiscard]]
        AZStd::vector<VehicleDifferentialConfiguration> CopyDifferentials() const override;

        bool SetCallbacks(ExtensionHandle extensionHandle) override;

        bool SetCollisionFilter(ExtensionHandle extensionHandle) override;

        bool SetDifferentialLimitedSlipRatio(float ratio) override;

        bool SetPowertrainControl(const VehiclePowertrainControl& control) override;

        [[nodiscard]]
        QueryResult QueryState(
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const override;

        [[nodiscard]]
        TrackedVehicleState GetState() const override;

        [[nodiscard]]
        AZStd::vector<WheelState> CopyWheelStates() const override;

        bool SetInput(const TrackedVehicleInput& input) override;

        bool SetTrackAngularVelocity(
            AZ::u32 trackIndex,
            float angularVelocity) override;

        bool SetWheelMotion(
            AZ::u32 wheelIndex,
            const WheelMotion& motion) override;

        bool UpdateAntiRollBars(const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars) override;

        bool UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration) override;

        bool UpdateDifferentials(const AZStd::vector<VehicleDifferentialConfiguration>& differentials) override;

        bool UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration) override;

        bool UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration) override;

        bool UpdateTransmissionConfiguration(const VehicleTransmissionConfiguration& configuration) override;

        bool UpdateTrackConfiguration(
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration) override;

    private:
        friend class VehicleComponentBase;

        void Activate() override;

        void Deactivate() override;

        VehicleHandle CreateVehicle(
            RuntimeImplementation& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        AZ::ComponentId GetComponentId() const override
        {
            return GetId();
        }

        AZStd::unique_ptr<TrackedVehicleComponentConfiguration> m_configuration;
    };
} // namespace Jolt
