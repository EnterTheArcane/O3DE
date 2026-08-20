/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>
#include <Jolt/Query.h>
#include <Jolt/Vehicle.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class VehicleRequestBusTraits
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        using BusIdType = AZ::EntityId;
    };

    class IVehicleRequests
        : public VehicleRequestBusTraits
    {
    public:
        virtual bool ApplyEngineDamping(float deltaTime) = 0;

        virtual bool ApplyEngineTorque(
            float torque,
            float deltaTime) = 0;

        [[nodiscard]]
        virtual float CalculateEngineTorque(float acceleration) const = 0;

        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual VehicleHandle GetVehicleHandle() const = 0;

        [[nodiscard]]
        virtual VehicleCollisionConfiguration GetCollisionConfiguration() const = 0;

        [[nodiscard]]
        virtual float GetDifferentialLimitedSlipRatio() const = 0;

        [[nodiscard]]
        virtual VehicleEngineConfiguration GetEngineConfiguration() const = 0;

        [[nodiscard]]
        virtual VehiclePowertrainState GetPowertrainState() const = 0;

        [[nodiscard]]
        virtual VehicleRuntimeConfiguration GetRuntimeConfiguration() const = 0;

        [[nodiscard]]
        virtual VehicleTransmissionConfiguration GetTransmissionConfiguration() const = 0;

        [[nodiscard]]
        virtual WheelBasis GetWheelLocalBasis(AZ::u32 wheelIndex) const = 0;

        [[nodiscard]]
        virtual AZ::Transform GetWheelLocalTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const = 0;

        [[nodiscard]]
        virtual WorldTransform GetWheelWorldTransform(
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<VehicleAntiRollBarConfiguration> CopyAntiRollBars() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<VehicleDifferentialConfiguration> CopyDifferentials() const = 0;

        virtual bool SetCallbacks(ExtensionHandle extensionHandle) = 0;

        virtual bool SetCollisionFilter(ExtensionHandle extensionHandle) = 0;

        virtual bool SetDifferentialLimitedSlipRatio(float ratio) = 0;

        virtual bool SetPowertrainControl(const VehiclePowertrainControl& control) = 0;

        virtual bool SetWheelMotion(
            AZ::u32 wheelIndex,
            const WheelMotion& motion) = 0;

        virtual bool UpdateAntiRollBars(const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars) = 0;

        virtual bool UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration) = 0;

        virtual bool UpdateDifferentials(const AZStd::vector<VehicleDifferentialConfiguration>& differentials) = 0;

        virtual bool UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration) = 0;

        virtual bool UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration) = 0;

        virtual bool UpdateTransmissionConfiguration(const VehicleTransmissionConfiguration& configuration) = 0;
    };

    class IWheeledVehicleRequests
        : public IVehicleRequests
    {
    public:
        [[nodiscard]]
        virtual QueryResult QueryState(
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual WheeledVehicleState GetState() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<WheelState> CopyWheelStates() const = 0;

        virtual bool SetInput(const WheeledVehicleInput& input) = 0;
    };

    using WheeledVehicleRequestBus = AZ::EBus<IWheeledVehicleRequests>;

    class IMotorcycleRequests
        : public IVehicleRequests
    {
    public:
        [[nodiscard]]
        virtual QueryResult QueryState(
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual MotorcycleState GetState() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<WheelState> CopyWheelStates() const = 0;

        virtual bool SetInput(const WheeledVehicleInput& input) = 0;

        virtual bool UpdateController(
            const MotorcycleControllerUpdateConfiguration& configuration) = 0;
    };

    using MotorcycleRequestBus = AZ::EBus<IMotorcycleRequests>;

    class ITrackedVehicleRequests
        : public IVehicleRequests
    {
    public:
        [[nodiscard]]
        virtual QueryResult QueryState(
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual TrackedVehicleState GetState() const = 0;

        [[nodiscard]]
        virtual VehicleTrackConfiguration GetTrackConfiguration(AZ::u32 trackIndex) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<WheelState> CopyWheelStates() const = 0;

        virtual bool SetInput(const TrackedVehicleInput& input) = 0;

        virtual bool SetTrackAngularVelocity(
            AZ::u32 trackIndex,
            float angularVelocity) = 0;

        virtual bool UpdateTrackConfiguration(
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration) = 0;
    };

    using TrackedVehicleRequestBus = AZ::EBus<ITrackedVehicleRequests>;

    class IVehicleNotifications
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        using BusIdType = AZ::EntityId;

        virtual void OnVehicleCreated(VehicleHandle vehicleHandle)
        {
            AZ_UNUSED(vehicleHandle);
        }

        virtual void OnVehicleDestroyed(VehicleHandle vehicleHandle)
        {
            AZ_UNUSED(vehicleHandle);
        }

        virtual void OnVehicleDestroying(VehicleHandle vehicleHandle)
        {
            AZ_UNUSED(vehicleHandle);
        }
    };

    using VehicleNotificationBus = AZ::EBus<IVehicleNotifications>;
} // namespace Jolt
