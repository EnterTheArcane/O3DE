/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Extension.h>
#include <Jolt/Vehicle.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Vehicles
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Vehicles* Get();

        bool ApplyVehicleEngineDamping(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float deltaTime);

        bool ApplyVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float torque,
            float deltaTime);

        [[nodiscard]]
        bool CalculateVehicleEngineTorque(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float acceleration,
            float& torque) const;

        [[nodiscard]]
        VehicleHandle CreateWheeledVehicle(
            WorldHandle worldHandle,
            const WheeledVehicleConfiguration& configuration);

        [[nodiscard]]
        VehicleHandle CreateMotorcycle(
            WorldHandle worldHandle,
            const MotorcycleConfiguration& configuration);

        [[nodiscard]]
        VehicleHandle CreateTrackedVehicle(
            WorldHandle worldHandle,
            const TrackedVehicleConfiguration& configuration);

        bool DestroyVehicle(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle) const;

        [[nodiscard]]
        QueryResult GetWheeledVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        QueryResult GetMotorcycleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        QueryResult GetTrackedVehicleState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        bool GetVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleCollisionConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float& ratio) const;

        [[nodiscard]]
        bool GetVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleEngineConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehiclePowertrainState(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehiclePowertrainState& state) const;

        [[nodiscard]]
        bool GetVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            VehicleTransmissionConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            VehicleTrackConfiguration& configuration) const;

        [[nodiscard]]
        bool GetWheelLocalBasis(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            WheelBasis& basis) const;

        [[nodiscard]]
        bool GetWheelLocalTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            AZ::Transform& transform) const;

        [[nodiscard]]
        bool GetWheelWorldTransform(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            WorldTransform& transform) const;

        [[nodiscard]]
        QueryResult QueryVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const;

        [[nodiscard]]
        QueryResult QueryVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleDifferentialConfiguration> differentials) const;

        bool SetTrackedVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const TrackedVehicleInput& input);

        bool SetVehicleCallbacks(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            ExtensionHandle extensionHandle);

        bool SetVehicleCollisionFilter(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            ExtensionHandle extensionHandle);

        bool SetVehicleDifferentialLimitedSlipRatio(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            float ratio);

        bool SetVehiclePowertrainControl(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehiclePowertrainControl& control);

        bool SetVehicleTrackAngularVelocity(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            float angularVelocity);

        bool SetWheelMotion(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const WheelMotion& motion);

        bool SetWheeledVehicleInput(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const WheeledVehicleInput& input);

        bool UpdateMotorcycleController(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const MotorcycleControllerUpdateConfiguration& configuration);

        bool UpdateVehicleAntiRollBars(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars);

        bool UpdateVehicleCollisionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleCollisionConfiguration& configuration);

        bool UpdateVehicleDifferentials(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleDifferentialConfiguration> differentials);

        bool UpdateVehicleEngineConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleEngineConfiguration& configuration);

        bool UpdateVehicleRuntimeConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleRuntimeConfiguration& configuration);

        bool UpdateVehicleTransmissionConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            const VehicleTransmissionConfiguration& configuration);

        bool UpdateVehicleTrackConfiguration(
            WorldHandle worldHandle,
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration);

    private:
        friend class Runtime;

        Vehicles() = default;
        ~Vehicles() = default;
    };
} // namespace Jolt
