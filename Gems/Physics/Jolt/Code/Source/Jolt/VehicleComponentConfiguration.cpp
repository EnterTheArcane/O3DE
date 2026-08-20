/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/VehicleComponentConfiguration.h>

#include <Jolt/Reflection.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    namespace
    {
        void ReflectCommonVehicleTypes(AZ::SerializeContext& serializeContext)
        {
            if (!ShouldReflect<WheelConfiguration>(serializeContext))
            {
                return;
            }

            serializeContext
                .Class<WheelConfiguration>()
                ->Field("Position", &WheelConfiguration::m_position)
                ->Field("SteeringAxis", &WheelConfiguration::m_steeringAxis)
                ->Field("SuspensionDirection", &WheelConfiguration::m_suspensionDirection)
                ->Field("SuspensionForcePoint", &WheelConfiguration::m_suspensionForcePoint)
                ->Field("WheelForward", &WheelConfiguration::m_wheelForward)
                ->Field("WheelUp", &WheelConfiguration::m_wheelUp)
                ->Field("SuspensionSpring", &WheelConfiguration::m_suspensionSpring)
                ->Field("LateralFriction", &WheelConfiguration::m_lateralFriction)
                ->Field("LongitudinalFriction", &WheelConfiguration::m_longitudinalFriction)
                ->Field("AngularDamping", &WheelConfiguration::m_angularDamping)
                ->Field("Inertia", &WheelConfiguration::m_inertia)
                ->Field("MaximumBrakeTorque", &WheelConfiguration::m_maximumBrakeTorque)
                ->Field("MaximumHandBrakeTorque", &WheelConfiguration::m_maximumHandBrakeTorque)
                ->Field("MaximumSteerAngle", &WheelConfiguration::m_maximumSteerAngle)
                ->Field("Radius", &WheelConfiguration::m_radius)
                ->Field("SuspensionMaximumLength", &WheelConfiguration::m_suspensionMaximumLength)
                ->Field("SuspensionMinimumLength", &WheelConfiguration::m_suspensionMinimumLength)
                ->Field("SuspensionPreloadLength", &WheelConfiguration::m_suspensionPreloadLength)
                ->Field("Width", &WheelConfiguration::m_width)
                ->Field("EnableSuspensionForcePoint", &WheelConfiguration::m_enableSuspensionForcePoint);

            serializeContext
                .Class<VehicleAntiRollBarConfiguration>()
                ->Field("LeftWheel", &VehicleAntiRollBarConfiguration::m_leftWheel)
                ->Field("RightWheel", &VehicleAntiRollBarConfiguration::m_rightWheel)
                ->Field("Stiffness", &VehicleAntiRollBarConfiguration::m_stiffness);

            serializeContext
                .Class<VehicleCollisionConfiguration>()
                ->Field("Up", &VehicleCollisionConfiguration::m_up)
                ->Field("CylinderConvexRadiusFraction", &VehicleCollisionConfiguration::m_cylinderConvexRadiusFraction)
                ->Field("MaximumSlopeAngle", &VehicleCollisionConfiguration::m_maximumSlopeAngle)
                ->Field("SphereRadius", &VehicleCollisionConfiguration::m_sphereRadius)
                ->Field("CollisionLayer", &VehicleCollisionConfiguration::m_collisionLayer)
                ->Field("Mode", &VehicleCollisionConfiguration::m_mode);

            serializeContext
                .Class<VehiclePowertrainControl>()
                ->Field("CurrentGear", &VehiclePowertrainControl::m_currentGear)
                ->Field("ClutchFriction", &VehiclePowertrainControl::m_clutchFriction)
                ->Field("EngineRpm", &VehiclePowertrainControl::m_engineRpm);

            serializeContext
                .Class<VehicleRuntimeConfiguration>()
                ->Field("GravityOverride", &VehicleRuntimeConfiguration::m_gravityOverride)
                ->Field("MaximumPitchRollAngle", &VehicleRuntimeConfiguration::m_maximumPitchRollAngle)
                ->Field("CollisionTestIntervalActive", &VehicleRuntimeConfiguration::m_collisionTestIntervalActive)
                ->Field("CollisionTestIntervalInactive", &VehicleRuntimeConfiguration::m_collisionTestIntervalInactive)
                ->Field("OverrideGravity", &VehicleRuntimeConfiguration::m_overrideGravity);

            serializeContext
                .Class<VehicleEngineConfiguration>()
                ->Field("NormalizedTorque", &VehicleEngineConfiguration::m_normalizedTorque)
                ->Field("AngularDamping", &VehicleEngineConfiguration::m_angularDamping)
                ->Field("Inertia", &VehicleEngineConfiguration::m_inertia)
                ->Field("MaximumRpm", &VehicleEngineConfiguration::m_maximumRpm)
                ->Field("MaximumTorque", &VehicleEngineConfiguration::m_maximumTorque)
                ->Field("MinimumRpm", &VehicleEngineConfiguration::m_minimumRpm);

            serializeContext
                .Class<VehicleTransmissionConfiguration>()
                ->Field("ForwardGearRatios", &VehicleTransmissionConfiguration::m_forwardGearRatios)
                ->Field("ReverseGearRatios", &VehicleTransmissionConfiguration::m_reverseGearRatios)
                ->Field("Mode", &VehicleTransmissionConfiguration::m_mode)
                ->Field("ClutchReleaseTime", &VehicleTransmissionConfiguration::m_clutchReleaseTime)
                ->Field("ClutchStrength", &VehicleTransmissionConfiguration::m_clutchStrength)
                ->Field("ShiftDownRpm", &VehicleTransmissionConfiguration::m_shiftDownRpm)
                ->Field("ShiftUpRpm", &VehicleTransmissionConfiguration::m_shiftUpRpm)
                ->Field("SwitchLatency", &VehicleTransmissionConfiguration::m_switchLatency)
                ->Field("SwitchTime", &VehicleTransmissionConfiguration::m_switchTime);

            serializeContext
                .Class<WheelMotion>()
                ->Field("AngularVelocity", &WheelMotion::m_angularVelocity)
                ->Field("RotationAngle", &WheelMotion::m_rotationAngle)
                ->Field("SteerAngle", &WheelMotion::m_steerAngle);
        }

        void ReflectWheeledVehicleTypes(AZ::SerializeContext& serializeContext)
        {
            if (!ShouldReflect<WheeledVehicleConfiguration>(serializeContext))
            {
                return;
            }

            serializeContext
                .Class<VehicleDifferentialConfiguration>()
                ->Field("LeftWheel", &VehicleDifferentialConfiguration::m_leftWheel)
                ->Field("RightWheel", &VehicleDifferentialConfiguration::m_rightWheel)
                ->Field("DifferentialRatio", &VehicleDifferentialConfiguration::m_differentialRatio)
                ->Field("EngineTorqueRatio", &VehicleDifferentialConfiguration::m_engineTorqueRatio)
                ->Field("LeftRightSplit", &VehicleDifferentialConfiguration::m_leftRightSplit)
                ->Field("LimitedSlipRatio", &VehicleDifferentialConfiguration::m_limitedSlipRatio);

            serializeContext
                .Class<WheeledVehicleConfiguration>()
                ->Field("Wheels", &WheeledVehicleConfiguration::m_wheels)
                ->Field("AntiRollBars", &WheeledVehicleConfiguration::m_antiRollBars)
                ->Field("Differentials", &WheeledVehicleConfiguration::m_differentials)
                ->Field("Engine", &WheeledVehicleConfiguration::m_engine)
                ->Field("Transmission", &WheeledVehicleConfiguration::m_transmission)
                ->Field("CollisionLayer", &WheeledVehicleConfiguration::m_collisionLayer)
                ->Field("Forward", &WheeledVehicleConfiguration::m_forward)
                ->Field("GravityOverride", &WheeledVehicleConfiguration::m_gravityOverride)
                ->Field("Up", &WheeledVehicleConfiguration::m_up)
                ->Field("CollisionSphereRadius", &WheeledVehicleConfiguration::m_collisionSphereRadius)
                ->Field(
                    "CollisionCylinderConvexRadiusFraction",
                    &WheeledVehicleConfiguration::m_collisionCylinderConvexRadiusFraction)
                ->Field("CollisionMaximumSlopeAngle", &WheeledVehicleConfiguration::m_collisionMaximumSlopeAngle)
                ->Field("DifferentialLimitedSlipRatio", &WheeledVehicleConfiguration::m_differentialLimitedSlipRatio)
                ->Field("MaximumPitchRollAngle", &WheeledVehicleConfiguration::m_maximumPitchRollAngle)
                ->Field("CollisionTestIntervalActive", &WheeledVehicleConfiguration::m_collisionTestIntervalActive)
                ->Field("CollisionTestIntervalInactive", &WheeledVehicleConfiguration::m_collisionTestIntervalInactive)
                ->Field("CollisionTestMode", &WheeledVehicleConfiguration::m_collisionTestMode)
                ->Field("OverrideGravity", &WheeledVehicleConfiguration::m_overrideGravity);
        }

        void ReflectCommonVehicleEditTypes(AZ::EditContext& editContext)
        {
            editContext
                .Class<WheelConfiguration>("Wheel", "Wheel geometry, suspension, steering, braking, and friction.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_position, "Position", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_steeringAxis, "Steering axis", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_suspensionDirection,
                    "Suspension direction",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_suspensionForcePoint,
                    "Suspension force point",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_wheelForward, "Wheel forward", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_wheelUp, "Wheel up", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_suspensionSpring,
                    "Suspension spring",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_lateralFriction, "Lateral friction", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_longitudinalFriction,
                    "Longitudinal friction",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_angularDamping, "Angular damping", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_inertia, "Inertia", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_maximumBrakeTorque,
                    "Maximum brake torque",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_maximumHandBrakeTorque,
                    "Maximum hand brake torque",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_maximumSteerAngle,
                    "Maximum steer angle",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_radius, "Radius", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_suspensionMaximumLength,
                    "Maximum suspension length",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_suspensionMinimumLength,
                    "Minimum suspension length",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_suspensionPreloadLength,
                    "Suspension preload length",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheelConfiguration::m_width, "Width", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheelConfiguration::m_enableSuspensionForcePoint,
                    "Use suspension force point",
                    "");

            editContext
                .Class<VehicleAntiRollBarConfiguration>("Anti-roll bar", "Couples suspension forces between two wheels.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleAntiRollBarConfiguration::m_leftWheel, "Left wheel", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleAntiRollBarConfiguration::m_rightWheel, "Right wheel", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleAntiRollBarConfiguration::m_stiffness, "Stiffness", "");

            editContext
                .Class<VehicleEngineConfiguration>("Engine", "Engine torque and rotational properties.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleEngineConfiguration::m_normalizedTorque, "Torque curve", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleEngineConfiguration::m_angularDamping, "Angular damping", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleEngineConfiguration::m_inertia, "Inertia", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleEngineConfiguration::m_maximumRpm, "Maximum RPM", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleEngineConfiguration::m_maximumTorque, "Maximum torque", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleEngineConfiguration::m_minimumRpm, "Minimum RPM", "");

            editContext
                .Class<VehicleTransmissionConfiguration>("Transmission", "Gear ratios, clutch, and automatic shifting.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_forwardGearRatios,
                    "Forward gear ratios",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_reverseGearRatios,
                    "Reverse gear ratios",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleTransmissionConfiguration::m_mode, "Mode", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_clutchReleaseTime,
                    "Clutch release time",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_clutchStrength,
                    "Clutch strength",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_shiftDownRpm,
                    "Shift down RPM",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_shiftUpRpm,
                    "Shift up RPM",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_switchLatency,
                    "Switch latency",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleTransmissionConfiguration::m_switchTime,
                    "Switch time",
                    "");
        }

        void ReflectWheeledVehicleEditTypes(AZ::EditContext& editContext)
        {
            editContext
                .Class<VehicleDifferentialConfiguration>("Differential", "Distributes engine torque between two wheels.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleDifferentialConfiguration::m_leftWheel, "Left wheel", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleDifferentialConfiguration::m_rightWheel, "Right wheel", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleDifferentialConfiguration::m_differentialRatio,
                    "Differential ratio",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleDifferentialConfiguration::m_engineTorqueRatio,
                    "Engine torque ratio",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleDifferentialConfiguration::m_leftRightSplit,
                    "Left-right split",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &VehicleDifferentialConfiguration::m_limitedSlipRatio,
                    "Limited-slip ratio",
                    "");

            editContext
                .Class<WheeledVehicleConfiguration>("Vehicle", "Wheels, suspension, collision, and powertrain.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_wheels, "Wheels", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_antiRollBars, "Anti-roll bars", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_differentials, "Differentials", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_engine, "Engine", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_transmission, "Transmission", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_collisionLayer, "Collision layer", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_forward, "Forward", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_gravityOverride,
                    "Gravity override",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &WheeledVehicleConfiguration::m_up, "Up", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_collisionSphereRadius,
                    "Collision sphere radius",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_collisionCylinderConvexRadiusFraction,
                    "Cylinder convex radius fraction",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_collisionMaximumSlopeAngle,
                    "Maximum collision slope angle",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_differentialLimitedSlipRatio,
                    "Differential limited-slip ratio",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_maximumPitchRollAngle,
                    "Maximum pitch-roll angle",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_collisionTestIntervalActive,
                    "Active collision interval",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_collisionTestIntervalInactive,
                    "Inactive collision interval",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_collisionTestMode,
                    "Collision test mode",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &WheeledVehicleConfiguration::m_overrideGravity,
                    "Override gravity",
                    "");
        }
    } // namespace

    WheeledVehicleComponentConfiguration WheeledVehicleComponentConfiguration::CreateDefault()
    {
        WheeledVehicleComponentConfiguration configuration;
        WheeledVehicleConfiguration& vehicle = configuration.m_vehicle;
        vehicle.m_wheels = {
            WheelConfiguration{
                .m_position = AZ::Vector3(-0.8f, 1.2f, 0.0f),
            },
            WheelConfiguration{
                .m_position = AZ::Vector3(0.8f, 1.2f, 0.0f),
            },
            WheelConfiguration{
                .m_position = AZ::Vector3(-0.8f, -1.2f, 0.0f),
                .m_maximumSteerAngle = 0.0f,
            },
            WheelConfiguration{
                .m_position = AZ::Vector3(0.8f, -1.2f, 0.0f),
                .m_maximumSteerAngle = 0.0f,
            },
        };
        vehicle.m_antiRollBars = {
            VehicleAntiRollBarConfiguration{
                .m_leftWheel = 0,
                .m_rightWheel = 1,
            },
            VehicleAntiRollBarConfiguration{
                .m_leftWheel = 2,
                .m_rightWheel = 3,
            },
        };
        vehicle.m_differentials = {
            VehicleDifferentialConfiguration{
                .m_leftWheel = 2,
                .m_rightWheel = 3,
            },
        };
        return configuration;
    }

    MotorcycleComponentConfiguration MotorcycleComponentConfiguration::CreateDefault()
    {
        MotorcycleComponentConfiguration configuration;
        WheeledVehicleConfiguration& vehicle = configuration.m_motorcycle.m_wheeled;
        vehicle.m_wheels = {
            WheelConfiguration{
                .m_position = AZ::Vector3(0.0f, 1.0f, 0.0f),
            },
            WheelConfiguration{
                .m_position = AZ::Vector3(0.0f, -1.0f, 0.0f),
                .m_maximumSteerAngle = 0.0f,
            },
        };
        vehicle.m_differentials = {
            VehicleDifferentialConfiguration{
                .m_leftWheel = 1,
                .m_leftRightSplit = 1.0f,
            },
        };
        return configuration;
    }

    TrackedVehicleComponentConfiguration TrackedVehicleComponentConfiguration::CreateDefault()
    {
        TrackedVehicleComponentConfiguration configuration;
        TrackedVehicleConfiguration& vehicle = configuration.m_vehicle;
        vehicle.m_wheels = {
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(-0.8f, 1.0f, 0.0f),
                    .m_maximumSteerAngle = 0.0f,
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(-0.8f, 0.0f, 0.0f),
                    .m_maximumSteerAngle = 0.0f,
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(-0.8f, -1.0f, 0.0f),
                    .m_maximumSteerAngle = 0.0f,
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(0.8f, 1.0f, 0.0f),
                    .m_maximumSteerAngle = 0.0f,
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(0.8f, 0.0f, 0.0f),
                    .m_maximumSteerAngle = 0.0f,
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(0.8f, -1.0f, 0.0f),
                    .m_maximumSteerAngle = 0.0f,
                },
            },
        };
        vehicle.m_tracks = {
            VehicleTrackConfiguration{
                .m_wheels = {0, 1, 2},
                .m_drivenWheel = 2,
            },
            VehicleTrackConfiguration{
                .m_wheels = {3, 4, 5},
                .m_drivenWheel = 5,
            },
        };
        return configuration;
    }

    void WheeledVehicleComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            const bool reflectCommon = ShouldReflect<WheelConfiguration>(*serializeContext);
            const bool reflectWheeled = ShouldReflect<WheeledVehicleConfiguration>(*serializeContext);
            const bool reflectComponent = ShouldReflect<WheeledVehicleComponentConfiguration>(*serializeContext);
            ReflectCommonVehicleTypes(*serializeContext);
            ReflectWheeledVehicleTypes(*serializeContext);

            if (reflectComponent)
            {
                serializeContext
                    ->Class<WheeledVehicleComponentConfiguration>()
                    ->Field("Vehicle", &WheeledVehicleComponentConfiguration::m_vehicle)
                    ->Field("Enabled", &WheeledVehicleComponentConfiguration::m_enabled);
            }

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                if (reflectCommon)
                {
                    ReflectCommonVehicleEditTypes(*editContext);
                }
                if (reflectWheeled)
                {
                    ReflectWheeledVehicleEditTypes(*editContext);
                }
                if (reflectComponent)
                {
                    editContext
                        ->Class<WheeledVehicleComponentConfiguration>("Configuration", "Wheeled vehicle settings.")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &WheeledVehicleComponentConfiguration::m_vehicle,
                            "Vehicle",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &WheeledVehicleComponentConfiguration::m_enabled,
                            "Enabled",
                            "");
                }
            }
        }
    }

    void MotorcycleComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            const bool reflectCommon = ShouldReflect<WheelConfiguration>(*serializeContext);
            const bool reflectWheeled = ShouldReflect<WheeledVehicleConfiguration>(*serializeContext);
            const bool reflectController = ShouldReflect<MotorcycleControllerConfiguration>(*serializeContext);
            const bool reflectMotorcycle = ShouldReflect<MotorcycleConfiguration>(*serializeContext);
            const bool reflectComponent = ShouldReflect<MotorcycleComponentConfiguration>(*serializeContext);
            ReflectCommonVehicleTypes(*serializeContext);
            ReflectWheeledVehicleTypes(*serializeContext);

            if (reflectController)
            {
                serializeContext
                    ->Class<MotorcycleControllerConfiguration>()
                    ->Field("LeanSmoothingFactor", &MotorcycleControllerConfiguration::m_leanSmoothingFactor)
                    ->Field("MaximumLeanAngle", &MotorcycleControllerConfiguration::m_maximumLeanAngle)
                    ->Field("SpringConstant", &MotorcycleControllerConfiguration::m_springConstant)
                    ->Field("SpringDamping", &MotorcycleControllerConfiguration::m_springDamping)
                    ->Field(
                        "SpringIntegrationCoefficient",
                        &MotorcycleControllerConfiguration::m_springIntegrationCoefficient)
                    ->Field(
                        "SpringIntegrationCoefficientDecay",
                        &MotorcycleControllerConfiguration::m_springIntegrationCoefficientDecay)
                    ->Field("EnableLeanController", &MotorcycleControllerConfiguration::m_enableLeanController)
                    ->Field("EnableLeanSteeringLimit", &MotorcycleControllerConfiguration::m_enableLeanSteeringLimit);
            }
            if (reflectMotorcycle)
            {
                serializeContext
                    ->Class<MotorcycleConfiguration>()
                    ->Field("Wheeled", &MotorcycleConfiguration::m_wheeled)
                    ->Field("Controller", &MotorcycleConfiguration::m_controller);
            }
            if (reflectComponent)
            {
                serializeContext
                    ->Class<MotorcycleComponentConfiguration>()
                    ->Field("Motorcycle", &MotorcycleComponentConfiguration::m_motorcycle)
                    ->Field("Enabled", &MotorcycleComponentConfiguration::m_enabled);
            }

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                if (reflectCommon)
                {
                    ReflectCommonVehicleEditTypes(*editContext);
                }
                if (reflectWheeled)
                {
                    ReflectWheeledVehicleEditTypes(*editContext);
                }
                if (reflectController)
                {
                    editContext
                        ->Class<MotorcycleControllerConfiguration>("Lean controller", "Motorcycle balance and steering.")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_leanSmoothingFactor,
                            "Lean smoothing factor",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_maximumLeanAngle,
                            "Maximum lean angle",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_springConstant,
                            "Spring constant",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_springDamping,
                            "Spring damping",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_springIntegrationCoefficient,
                            "Spring integration coefficient",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_springIntegrationCoefficientDecay,
                            "Spring integration coefficient decay",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_enableLeanController,
                            "Enable lean controller",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleControllerConfiguration::m_enableLeanSteeringLimit,
                            "Enable lean steering limit",
                            "");
                }
                if (reflectMotorcycle)
                {
                    editContext
                        ->Class<MotorcycleConfiguration>("Motorcycle", "Wheeled vehicle and lean-controller settings.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MotorcycleConfiguration::m_wheeled, "Wheeled", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MotorcycleConfiguration::m_controller, "Controller", "");
                }
                if (reflectComponent)
                {
                    editContext
                        ->Class<MotorcycleComponentConfiguration>("Configuration", "Motorcycle settings.")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleComponentConfiguration::m_motorcycle,
                            "Motorcycle",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &MotorcycleComponentConfiguration::m_enabled,
                            "Enabled",
                            "");
                }
            }
        }
    }

    void TrackedVehicleComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            const bool reflectCommon = ShouldReflect<WheelConfiguration>(*serializeContext);
            const bool reflectTrackedWheel = ShouldReflect<TrackedWheelConfiguration>(*serializeContext);
            const bool reflectTrack = ShouldReflect<VehicleTrackConfiguration>(*serializeContext);
            const bool reflectVehicle = ShouldReflect<TrackedVehicleConfiguration>(*serializeContext);
            const bool reflectComponent = ShouldReflect<TrackedVehicleComponentConfiguration>(*serializeContext);
            ReflectCommonVehicleTypes(*serializeContext);

            if (reflectTrackedWheel)
            {
                serializeContext
                    ->Class<TrackedWheelConfiguration>()
                    ->Field("Common", &TrackedWheelConfiguration::m_common)
                    ->Field("LateralFriction", &TrackedWheelConfiguration::m_lateralFriction)
                    ->Field("LongitudinalFriction", &TrackedWheelConfiguration::m_longitudinalFriction);
            }
            if (reflectTrack)
            {
                serializeContext
                    ->Class<VehicleTrackConfiguration>()
                    ->Field("Wheels", &VehicleTrackConfiguration::m_wheels)
                    ->Field("DrivenWheel", &VehicleTrackConfiguration::m_drivenWheel)
                    ->Field("AngularDamping", &VehicleTrackConfiguration::m_angularDamping)
                    ->Field("DifferentialRatio", &VehicleTrackConfiguration::m_differentialRatio)
                    ->Field("Inertia", &VehicleTrackConfiguration::m_inertia)
                    ->Field("MaximumBrakeTorque", &VehicleTrackConfiguration::m_maximumBrakeTorque);
            }
            if (reflectVehicle)
            {
                serializeContext
                    ->Class<TrackedVehicleConfiguration>()
                    ->Field("Wheels", &TrackedVehicleConfiguration::m_wheels)
                    ->Field("AntiRollBars", &TrackedVehicleConfiguration::m_antiRollBars)
                    ->Field("Tracks", &TrackedVehicleConfiguration::m_tracks)
                    ->Field("Engine", &TrackedVehicleConfiguration::m_engine)
                    ->Field("Transmission", &TrackedVehicleConfiguration::m_transmission)
                    ->Field("CollisionLayer", &TrackedVehicleConfiguration::m_collisionLayer)
                    ->Field("Forward", &TrackedVehicleConfiguration::m_forward)
                    ->Field("GravityOverride", &TrackedVehicleConfiguration::m_gravityOverride)
                    ->Field("Up", &TrackedVehicleConfiguration::m_up)
                    ->Field("CollisionSphereRadius", &TrackedVehicleConfiguration::m_collisionSphereRadius)
                    ->Field(
                        "CollisionCylinderConvexRadiusFraction",
                        &TrackedVehicleConfiguration::m_collisionCylinderConvexRadiusFraction)
                    ->Field("CollisionMaximumSlopeAngle", &TrackedVehicleConfiguration::m_collisionMaximumSlopeAngle)
                    ->Field("MaximumPitchRollAngle", &TrackedVehicleConfiguration::m_maximumPitchRollAngle)
                    ->Field("CollisionTestIntervalActive", &TrackedVehicleConfiguration::m_collisionTestIntervalActive)
                    ->Field("CollisionTestIntervalInactive", &TrackedVehicleConfiguration::m_collisionTestIntervalInactive)
                    ->Field("CollisionTestMode", &TrackedVehicleConfiguration::m_collisionTestMode)
                    ->Field("OverrideGravity", &TrackedVehicleConfiguration::m_overrideGravity);
            }
            if (reflectComponent)
            {
                serializeContext
                    ->Class<TrackedVehicleComponentConfiguration>()
                    ->Field("Vehicle", &TrackedVehicleComponentConfiguration::m_vehicle)
                    ->Field("Enabled", &TrackedVehicleComponentConfiguration::m_enabled);
            }

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                if (reflectCommon)
                {
                    ReflectCommonVehicleEditTypes(*editContext);
                }
                if (reflectTrackedWheel)
                {
                    editContext
                        ->Class<TrackedWheelConfiguration>("Tracked wheel", "Wheel and track-friction settings.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedWheelConfiguration::m_common, "Wheel", "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedWheelConfiguration::m_lateralFriction,
                            "Lateral friction",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedWheelConfiguration::m_longitudinalFriction,
                            "Longitudinal friction",
                            "");
                }
                if (reflectTrack)
                {
                    editContext
                        ->Class<VehicleTrackConfiguration>("Track", "Wheel membership and track drivetrain.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleTrackConfiguration::m_wheels, "Wheels", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleTrackConfiguration::m_drivenWheel, "Driven wheel", "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VehicleTrackConfiguration::m_angularDamping,
                            "Angular damping",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VehicleTrackConfiguration::m_differentialRatio,
                            "Differential ratio",
                            "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VehicleTrackConfiguration::m_inertia, "Inertia", "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VehicleTrackConfiguration::m_maximumBrakeTorque,
                            "Maximum brake torque",
                            "");
                }
                if (reflectVehicle)
                {
                    editContext
                        ->Class<TrackedVehicleConfiguration>("Tracked vehicle", "Tracks, wheels, collision, and powertrain.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_wheels, "Wheels", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_antiRollBars, "Anti-roll bars", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_tracks, "Tracks", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_engine, "Engine", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_transmission, "Transmission", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_collisionLayer, "Collision layer", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_forward, "Forward", "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_gravityOverride,
                            "Gravity override",
                            "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &TrackedVehicleConfiguration::m_up, "Up", "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_collisionSphereRadius,
                            "Collision sphere radius",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_collisionCylinderConvexRadiusFraction,
                            "Cylinder convex radius fraction",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_collisionMaximumSlopeAngle,
                            "Maximum collision slope angle",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_maximumPitchRollAngle,
                            "Maximum pitch-roll angle",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_collisionTestIntervalActive,
                            "Active collision interval",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_collisionTestIntervalInactive,
                            "Inactive collision interval",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_collisionTestMode,
                            "Collision test mode",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleConfiguration::m_overrideGravity,
                            "Override gravity",
                            "");
                }
                if (reflectComponent)
                {
                    editContext
                        ->Class<TrackedVehicleComponentConfiguration>("Configuration", "Tracked vehicle settings.")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleComponentConfiguration::m_vehicle,
                            "Vehicle",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &TrackedVehicleComponentConfiguration::m_enabled,
                            "Enabled",
                            "");
                }
            }
        }
    }
} // namespace Jolt
