/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Rollback.h>

#include <Jolt/Constraint.h>
#include <Jolt/Handle.h>
#include <Jolt/Query.h>
#include <Jolt/Simulation.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace Jolt
{
    enum class VehicleKind : AZ::u8
    {
        None = 0,
        Motorcycle,
        Tracked,
        Wheeled,
    };

    enum class TransmissionMode : AZ::u8
    {
        None = 0,
        Automatic,
        Manual,
    };

    enum class VehicleCollisionTestMode : AZ::u8
    {
        None = 0,
        Cylinder,
        Ray,
        Sphere,
    };

    struct WheelConfiguration final
    {
        AZ_TYPE_INFO(WheelConfiguration, WheelConfigurationTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_steeringAxis = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_suspensionDirection = -AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_suspensionForcePoint = AZ::Vector3::CreateZero();
        AZ::Vector3 m_wheelForward = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_wheelUp = AZ::Vector3::CreateAxisZ();

        SpringConfiguration m_suspensionSpring = {
            .m_mode = SpringMode::FrequencyAndDamping,
            .m_frequencyOrStiffness = 1.5f,
            .m_damping = 0.5f,
        };
        AZStd::vector<AZ::Vector2> m_lateralFriction;
        AZStd::vector<AZ::Vector2> m_longitudinalFriction;

        float m_angularDamping = 0.2f;
        float m_inertia = 0.9f;
        float m_maximumBrakeTorque = 1'500.0f;
        float m_maximumHandBrakeTorque = 4'000.0f;
        float m_maximumSteerAngle = 1.22173048f;
        float m_radius = 0.3f;
        float m_suspensionMaximumLength = 0.5f;
        float m_suspensionMinimumLength = 0.3f;
        float m_suspensionPreloadLength = 0.0f;
        float m_width = 0.1f;

        bool m_enableSuspensionForcePoint = false;
    };

    struct VehicleAntiRollBarConfiguration final
    {
        AZ_TYPE_INFO(VehicleAntiRollBarConfiguration, VehicleAntiRollBarConfigurationTypeId);

        AZ::u32 m_leftWheel = 0;
        AZ::u32 m_rightWheel = 1;
        float m_stiffness = 1'000.0f;
    };

    struct VehicleDifferentialConfiguration final
    {
        AZ_TYPE_INFO(VehicleDifferentialConfiguration, VehicleDifferentialConfigurationTypeId);

        AZ::s32 m_leftWheel = -1;
        AZ::s32 m_rightWheel = -1;
        float m_differentialRatio = 3.42f;
        float m_engineTorqueRatio = 1.0f;
        float m_leftRightSplit = 0.5f;
        float m_limitedSlipRatio = 1.4f;
    };

    struct VehicleEngineConfiguration final
    {
        AZ_TYPE_INFO(VehicleEngineConfiguration, VehicleEngineConfigurationTypeId);

        AZStd::vector<AZ::Vector2> m_normalizedTorque;
        float m_angularDamping = 0.2f;
        float m_inertia = 0.5f;
        float m_maximumRpm = 6'000.0f;
        float m_maximumTorque = 500.0f;
        float m_minimumRpm = 1'000.0f;
    };

    struct VehicleTransmissionConfiguration final
    {
        AZ_TYPE_INFO(VehicleTransmissionConfiguration, VehicleTransmissionConfigurationTypeId);

        AZStd::vector<float> m_forwardGearRatios = {2.66f, 1.78f, 1.3f, 1.0f, 0.74f};
        AZStd::vector<float> m_reverseGearRatios = {-2.9f};
        TransmissionMode m_mode = TransmissionMode::Automatic;
        float m_clutchReleaseTime = 0.3f;
        float m_clutchStrength = 10.0f;
        float m_shiftDownRpm = 2'000.0f;
        float m_shiftUpRpm = 4'000.0f;
        float m_switchLatency = 0.5f;
        float m_switchTime = 0.5f;
    };

    struct WheeledVehicleConfiguration final
    {
        AZ_TYPE_INFO(WheeledVehicleConfiguration, WheeledVehicleConfigurationTypeId);

        BodyHandle m_bodyHandle;
        AZStd::vector<WheelConfiguration> m_wheels;
        AZStd::vector<VehicleAntiRollBarConfiguration> m_antiRollBars;
        AZStd::vector<VehicleDifferentialConfiguration> m_differentials;
        VehicleEngineConfiguration m_engine;
        VehicleTransmissionConfiguration m_transmission;

        ObjectLayer m_collisionLayer = DefaultLayers::Moving;
        AZ::Vector3 m_forward = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_gravityOverride = AZ::Vector3::CreateZero();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_collisionSphereRadius = 0.3f;
        float m_collisionCylinderConvexRadiusFraction = 0.1f;
        float m_collisionMaximumSlopeAngle = 1.3962634f;
        float m_differentialLimitedSlipRatio = 1.4f;
        float m_maximumPitchRollAngle = AZ::Constants::Pi;

        AZ::u32 m_collisionTestIntervalActive = 1;
        AZ::u32 m_collisionTestIntervalInactive = 1;
        VehicleCollisionTestMode m_collisionTestMode = VehicleCollisionTestMode::Ray;
        bool m_overrideGravity = false;
    };

    struct WheeledVehicleInput final
    {
        AZ_TYPE_INFO(WheeledVehicleInput, WheeledVehicleInputTypeId);

        float m_brake = 0.0f;
        float m_forward = 0.0f;
        float m_handBrake = 0.0f;
        float m_right = 0.0f;
    };

    struct MotorcycleControllerConfiguration final
    {
        AZ_TYPE_INFO(MotorcycleControllerConfiguration, MotorcycleControllerConfigurationTypeId);

        float m_leanSmoothingFactor = 0.8f;
        float m_maximumLeanAngle = AZ::Constants::QuarterPi;
        float m_springConstant = 5'000.0f;
        float m_springDamping = 1'000.0f;
        float m_springIntegrationCoefficient = 0.0f;
        float m_springIntegrationCoefficientDecay = 4.0f;

        bool m_enableLeanController = true;
        bool m_enableLeanSteeringLimit = true;
    };

    struct MotorcycleConfiguration final
    {
        AZ_TYPE_INFO(MotorcycleConfiguration, MotorcycleConfigurationTypeId);

        WheeledVehicleConfiguration m_wheeled;
        MotorcycleControllerConfiguration m_controller;
    };

    struct MotorcycleControllerUpdateConfiguration final
    {
        AZ_TYPE_INFO(MotorcycleControllerUpdateConfiguration, MotorcycleControllerUpdateConfigurationTypeId);

        float m_leanSmoothingFactor = 0.8f;
        float m_springConstant = 5'000.0f;
        float m_springDamping = 1'000.0f;
        float m_springIntegrationCoefficient = 0.0f;
        float m_springIntegrationCoefficientDecay = 4.0f;

        bool m_enableLeanController = true;
        bool m_enableLeanSteeringLimit = true;
    };

    struct VehicleRuntimeConfiguration final
    {
        AZ_TYPE_INFO(VehicleRuntimeConfiguration, VehicleRuntimeConfigurationTypeId);

        AZ::Vector3 m_gravityOverride = AZ::Vector3::CreateZero();

        float m_maximumPitchRollAngle = AZ::Constants::Pi;

        AZ::u32 m_collisionTestIntervalActive = 1;
        AZ::u32 m_collisionTestIntervalInactive = 1;

        bool m_overrideGravity = false;
    };

    struct VehicleCollisionConfiguration final
    {
        AZ_TYPE_INFO(VehicleCollisionConfiguration, VehicleCollisionConfigurationTypeId);

        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_cylinderConvexRadiusFraction = 0.1f;
        float m_maximumSlopeAngle = 1.3962634f;
        float m_sphereRadius = 0.3f;

        ObjectLayer m_collisionLayer = DefaultLayers::Moving;
        VehicleCollisionTestMode m_mode = VehicleCollisionTestMode::Ray;
    };

    struct VehiclePowertrainControl final
    {
        AZ_TYPE_INFO(VehiclePowertrainControl, VehiclePowertrainControlTypeId);

        AZ::s32 m_currentGear = 0;
        float m_clutchFriction = 1.0f;
        float m_engineRpm = 1'000.0f;
    };

    struct VehiclePowertrainState final
    {
        AZ_TYPE_INFO(VehiclePowertrainState, VehiclePowertrainStateTypeId);

        AZ::s32 m_currentGear = 0;
        float m_clutchFriction = 0.0f;
        float m_currentRatio = 0.0f;
        float m_engineAngularVelocity = 0.0f;
        float m_engineRpm = 0.0f;
        float m_wheelSpeedAtClutch = 0.0f;
        bool m_isSwitchingGear = false;
    };

    struct WheelMotion final
    {
        AZ_TYPE_INFO(WheelMotion, WheelMotionTypeId);

        float m_angularVelocity = 0.0f;
        float m_rotationAngle = 0.0f;
        float m_steerAngle = 0.0f;
    };

    struct WheelBasis final
    {
        AZ_TYPE_INFO(WheelBasis, WheelBasisTypeId);

        AZ::Vector3 m_forward = AZ::Vector3::CreateZero();
        AZ::Vector3 m_right = AZ::Vector3::CreateZero();
        AZ::Vector3 m_up = AZ::Vector3::CreateZero();
    };

    struct WheelState final
    {
        AZ_TYPE_INFO(WheelState, WheelStateTypeId);

        WorldPosition m_contactPosition;
        AZ::Vector3 m_contactLateral = AZ::Vector3::CreateZero();
        AZ::Vector3 m_contactLongitudinal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_contactNormal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_contactVelocity = AZ::Vector3::CreateZero();
        BodyHandle m_contactBodyHandle;
        SubShapeId m_contactSubShapeId;

        float m_angularVelocity = 0.0f;
        float m_lateralImpulse = 0.0f;
        float m_longitudinalImpulse = 0.0f;
        float m_rotationAngle = 0.0f;
        float m_steerAngle = 0.0f;
        float m_suspensionImpulse = 0.0f;
        float m_suspensionLength = 0.0f;

        bool m_hasContact = false;
        bool m_hasHitHardPoint = false;
    };

    struct WheeledVehicleState final
    {
        AZ_TYPE_INFO(WheeledVehicleState, WheeledVehicleStateTypeId);

        BodyHandle m_bodyHandle;
        VehicleKind m_kind = VehicleKind::None;
        AZ::u32 m_wheelCount = 0;
        AZ::s32 m_currentGear = 0;
        float m_clutchFriction = 0.0f;
        float m_engineRpm = 0.0f;
        bool m_isSwitchingGear = false;
    };

    struct MotorcycleState final
    {
        AZ_TYPE_INFO(MotorcycleState, MotorcycleStateTypeId);

        WheeledVehicleState m_wheeled;
        MotorcycleControllerConfiguration m_controller;
        float m_wheelBase = 0.0f;
    };

    struct TrackedWheelConfiguration final
    {
        AZ_TYPE_INFO(TrackedWheelConfiguration, TrackedWheelConfigurationTypeId);

        WheelConfiguration m_common;
        float m_lateralFriction = 2.0f;
        float m_longitudinalFriction = 4.0f;
    };

    struct VehicleTrackConfiguration final
    {
        AZ_TYPE_INFO(VehicleTrackConfiguration, VehicleTrackConfigurationTypeId);

        AZStd::vector<AZ::u32> m_wheels;
        AZ::u32 m_drivenWheel = 0;
        float m_angularDamping = 0.5f;
        float m_differentialRatio = 6.0f;
        float m_inertia = 10.0f;
        float m_maximumBrakeTorque = 15'000.0f;
    };

    struct TrackedVehicleConfiguration final
    {
        AZ_TYPE_INFO(TrackedVehicleConfiguration, TrackedVehicleConfigurationTypeId);

        BodyHandle m_bodyHandle;
        AZStd::vector<TrackedWheelConfiguration> m_wheels;
        AZStd::vector<VehicleAntiRollBarConfiguration> m_antiRollBars;
        AZStd::array<VehicleTrackConfiguration, 2> m_tracks;
        VehicleEngineConfiguration m_engine;
        VehicleTransmissionConfiguration m_transmission;

        ObjectLayer m_collisionLayer = DefaultLayers::Moving;
        AZ::Vector3 m_forward = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_gravityOverride = AZ::Vector3::CreateZero();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_collisionSphereRadius = 0.3f;
        float m_collisionCylinderConvexRadiusFraction = 0.1f;
        float m_collisionMaximumSlopeAngle = 1.3962634f;
        float m_maximumPitchRollAngle = AZ::Constants::Pi;

        AZ::u32 m_collisionTestIntervalActive = 1;
        AZ::u32 m_collisionTestIntervalInactive = 1;
        VehicleCollisionTestMode m_collisionTestMode = VehicleCollisionTestMode::Ray;
        bool m_overrideGravity = false;
    };

    struct TrackedVehicleInput final
    {
        AZ_TYPE_INFO(TrackedVehicleInput, TrackedVehicleInputTypeId);

        float m_brake = 0.0f;
        float m_forward = 0.0f;
        float m_leftRatio = 1.0f;
        float m_rightRatio = 1.0f;
    };

    struct VehicleTrackState final
    {
        AZ_TYPE_INFO(VehicleTrackState, VehicleTrackStateTypeId);

        float m_angularVelocity = 0.0f;
    };

    struct TrackedVehicleState final
    {
        AZ_TYPE_INFO(TrackedVehicleState, TrackedVehicleStateTypeId);

        BodyHandle m_bodyHandle;
        AZStd::array<VehicleTrackState, 2> m_tracks;
        VehicleKind m_kind = VehicleKind::None;
        AZ::u32 m_wheelCount = 0;
        AZ::s32 m_currentGear = 0;
        float m_clutchFriction = 0.0f;
        float m_engineRpm = 0.0f;
        bool m_isSwitchingGear = false;
    };

    struct VehicleFrictionCalculation final
    {
        AZ_TYPE_INFO(VehicleFrictionCalculation, VehicleFrictionCalculationTypeId);

        BodyHandle m_contactBodyHandle;
        VehicleHandle m_vehicleHandle;
        SubShapeId m_contactSubShapeId;
        AZ::u32 m_wheelIndex = 0;

        float m_bodyFriction = 0.0f;
        float m_lateralFriction = 0.0f;
        float m_longitudinalFriction = 0.0f;
    };

    struct VehicleTireImpulseCalculation final
    {
        AZ_TYPE_INFO(VehicleTireImpulseCalculation, VehicleTireImpulseCalculationTypeId);

        VehicleHandle m_vehicleHandle;
        AZ::u32 m_wheelIndex = 0;

        float m_deltaTime = 0.0f;
        float m_lateralFriction = 0.0f;
        float m_lateralImpulse = 0.0f;
        float m_lateralSlip = 0.0f;
        float m_longitudinalFriction = 0.0f;
        float m_longitudinalImpulse = 0.0f;
        float m_longitudinalSlip = 0.0f;
        float m_suspensionImpulse = 0.0f;
    };

    class IVehicleStepContext
        : public IStepContext
    {
    public:
        ~IVehicleStepContext() override = default;

        [[nodiscard]]
        virtual QueryResult GetMotorcycleState(
            VehicleHandle vehicleHandle,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual QueryResult GetTrackedVehicleState(
            VehicleHandle vehicleHandle,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        [[nodiscard]]
        virtual QueryResult GetWheeledVehicleState(
            VehicleHandle vehicleHandle,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const = 0;

        virtual bool SetTrackedVehicleInput(
            VehicleHandle vehicleHandle,
            const TrackedVehicleInput& input) = 0;

        virtual bool SetVehiclePowertrainControl(
            VehicleHandle vehicleHandle,
            const VehiclePowertrainControl& control) = 0;

        virtual bool SetWheelMotion(
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const WheelMotion& motion) = 0;

        virtual bool SetWheeledVehicleInput(
            VehicleHandle vehicleHandle,
            const WheeledVehicleInput& input) = 0;
    };

    class IVehicleCallbacks
        : public IRollbackParticipant
    {
    public:
        virtual ~IVehicleCallbacks() = default;

        //! Called concurrently under simulation locks. The caller owns this object and its snapshot-external state.
        //! It must remain alive while registered, be thread-safe and deterministic, and must not call runtime capabilities.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        virtual void OnCalculateTireImpulses(VehicleTireImpulseCalculation& calculation)
        {
            AZ_UNUSED(calculation);
        }

        virtual void OnCombineFriction(VehicleFrictionCalculation& calculation)
        {
            AZ_UNUSED(calculation);
        }

        virtual void OnPreStep(
            VehicleHandle vehicleHandle,
            const StepInformation& information,
            IVehicleStepContext& context)
        {
            AZ_UNUSED(vehicleHandle);
            AZ_UNUSED(information);
            AZ_UNUSED(context);
        }

        virtual void OnPostCollide(
            VehicleHandle vehicleHandle,
            const StepInformation& information,
            IVehicleStepContext& context)
        {
            AZ_UNUSED(vehicleHandle);
            AZ_UNUSED(information);
            AZ_UNUSED(context);
        }

        virtual void OnPostStep(
            VehicleHandle vehicleHandle,
            const StepInformation& information,
            IVehicleStepContext& context)
        {
            AZ_UNUSED(vehicleHandle);
            AZ_UNUSED(information);
            AZ_UNUSED(context);
        }
    };

    class IVehicleCollisionFilter
        : public IRollbackParticipant
    {
    public:
        virtual ~IVehicleCollisionFilter() = default;

        //! Called concurrently under simulation locks. The caller owns this object and its snapshot-external state.
        //! It must remain alive while registered, be thread-safe and deterministic, and must not call runtime capabilities.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        [[nodiscard]]
        virtual bool ShouldCollide([[maybe_unused]] BodyHandle bodyHandle) const
        {
            return true;
        }

        [[nodiscard]]
        virtual bool ShouldCollide([[maybe_unused]] BroadPhaseLayer broadPhaseLayer) const
        {
            return true;
        }

        [[nodiscard]]
        virtual bool ShouldCollide([[maybe_unused]] ObjectLayer objectLayer) const
        {
            return true;
        }
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::TransmissionMode, "{3C77860F-5B7A-49A1-B0EC-796F0B020609}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::VehicleCollisionTestMode, "{7AC6BE24-1137-45E6-8CBA-52E0FC5977D7}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::VehicleKind, "{543F1152-A8DF-4584-A245-81FCFBCA98BE}");
