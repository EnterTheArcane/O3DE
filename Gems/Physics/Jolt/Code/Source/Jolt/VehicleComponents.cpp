/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/VehicleComponents.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/BodyBus.h>
#include <Jolt/Reflection.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    class VehicleNotificationBusBehaviorHandler final
        : public VehicleNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            VehicleNotificationBusBehaviorHandler,
            "{9D187C85-DA17-4A96-92D0-0231D3B64C47}",
            AZ::SystemAllocator,
            OnVehicleCreated,
            OnVehicleDestroyed,
            OnVehicleDestroying);

        void OnVehicleCreated(const VehicleHandle vehicleHandle) override
        {
            Call(FN_OnVehicleCreated, vehicleHandle);
        }

        void OnVehicleDestroyed(const VehicleHandle vehicleHandle) override
        {
            Call(FN_OnVehicleDestroyed, vehicleHandle);
        }

        void OnVehicleDestroying(const VehicleHandle vehicleHandle) override
        {
            Call(FN_OnVehicleDestroying, vehicleHandle);
        }
    };

    namespace
    {
        void GetVehicleProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC_CE("JoltVehicleService"));
        }

        void GetVehicleIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC_CE("JoltVehicleService"));
        }

        void GetVehicleRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC_CE("JoltBodyService"));
        }

        void ApplyCollisionConfiguration(
            WheeledVehicleConfiguration& destination,
            const VehicleCollisionConfiguration& source)
        {
            destination.m_up = source.m_up;
            destination.m_collisionCylinderConvexRadiusFraction = source.m_cylinderConvexRadiusFraction;
            destination.m_collisionMaximumSlopeAngle = source.m_maximumSlopeAngle;
            destination.m_collisionSphereRadius = source.m_sphereRadius;
            destination.m_collisionLayer = source.m_collisionLayer;
            destination.m_collisionTestMode = source.m_mode;
        }

        void ApplyCollisionConfiguration(
            TrackedVehicleConfiguration& destination,
            const VehicleCollisionConfiguration& source)
        {
            destination.m_up = source.m_up;
            destination.m_collisionCylinderConvexRadiusFraction = source.m_cylinderConvexRadiusFraction;
            destination.m_collisionMaximumSlopeAngle = source.m_maximumSlopeAngle;
            destination.m_collisionSphereRadius = source.m_sphereRadius;
            destination.m_collisionLayer = source.m_collisionLayer;
            destination.m_collisionTestMode = source.m_mode;
        }

        template<class Configuration>
        void ApplyRuntimeConfiguration(
            Configuration& destination,
            const VehicleRuntimeConfiguration& source)
        {
            destination.m_gravityOverride = source.m_gravityOverride;
            destination.m_maximumPitchRollAngle = source.m_maximumPitchRollAngle;
            destination.m_collisionTestIntervalActive = source.m_collisionTestIntervalActive;
            destination.m_collisionTestIntervalInactive = source.m_collisionTestIntervalInactive;
            destination.m_overrideGravity = source.m_overrideGravity;
        }

        void ReflectVehicleBehavior(AZ::BehaviorContext& behaviorContext)
        {
            if (!ShouldReflect(
                behaviorContext,
                behaviorContext.m_ebuses.contains("JoltVehicleNotificationBus")))
            {
                return;
            }

            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleKind, None);
            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleKind, Motorcycle);
            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleKind, Tracked);
            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleKind, Wheeled);

            JOLT_BEHAVIOR_ENUM(behaviorContext, TransmissionMode, None);
            JOLT_BEHAVIOR_ENUM(behaviorContext, TransmissionMode, Automatic);
            JOLT_BEHAVIOR_ENUM(behaviorContext, TransmissionMode, Manual);

            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleCollisionTestMode, None);
            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleCollisionTestMode, Cylinder);
            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleCollisionTestMode, Ray);
            JOLT_BEHAVIOR_ENUM(behaviorContext, VehicleCollisionTestMode, Sphere);

            behaviorContext.Class<WheeledVehicleInput>("WheeledVehicleInput")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("brake", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheeledVehicleInput::m_brake))
                ->Property("forward", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheeledVehicleInput::m_forward))
                ->Property("handBrake", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheeledVehicleInput::m_handBrake))
                ->Property("right", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheeledVehicleInput::m_right));

            behaviorContext.Class<TrackedVehicleInput>("TrackedVehicleInput")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("brake", JOLT_BEHAVIOR_VALUE_PROPERTY(&TrackedVehicleInput::m_brake))
                ->Property("forward", JOLT_BEHAVIOR_VALUE_PROPERTY(&TrackedVehicleInput::m_forward))
                ->Property("leftRatio", JOLT_BEHAVIOR_VALUE_PROPERTY(&TrackedVehicleInput::m_leftRatio))
                ->Property("rightRatio", JOLT_BEHAVIOR_VALUE_PROPERTY(&TrackedVehicleInput::m_rightRatio));

            behaviorContext.Class<MotorcycleControllerUpdateConfiguration>("MotorcycleControllerUpdateConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "leanSmoothingFactor",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_leanSmoothingFactor))
                ->Property(
                    "springConstant",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_springConstant))
                ->Property(
                    "springDamping",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_springDamping))
                ->Property(
                    "springIntegrationCoefficient",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_springIntegrationCoefficient))
                ->Property(
                    "springIntegrationCoefficientDecay",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_springIntegrationCoefficientDecay))
                ->Property(
                    "enableLeanController",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_enableLeanController))
                ->Property(
                    "enableLeanSteeringLimit",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&MotorcycleControllerUpdateConfiguration::m_enableLeanSteeringLimit));

            behaviorContext.Class<VehicleRuntimeConfiguration>("VehicleRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "gravityOverride",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleRuntimeConfiguration::m_gravityOverride))
                ->Property(
                    "maximumPitchRollAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleRuntimeConfiguration::m_maximumPitchRollAngle))
                ->Property(
                    "collisionTestIntervalActive",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleRuntimeConfiguration::m_collisionTestIntervalActive))
                ->Property(
                    "collisionTestIntervalInactive",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleRuntimeConfiguration::m_collisionTestIntervalInactive))
                ->Property(
                    "overrideGravity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleRuntimeConfiguration::m_overrideGravity));

            behaviorContext.Class<VehicleAntiRollBarConfiguration>("VehicleAntiRollBarConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("leftWheel", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleAntiRollBarConfiguration::m_leftWheel))
                ->Property("rightWheel", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleAntiRollBarConfiguration::m_rightWheel))
                ->Property("stiffness", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleAntiRollBarConfiguration::m_stiffness));

            behaviorContext.Class<VehicleCollisionConfiguration>("VehicleCollisionConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("up", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleCollisionConfiguration::m_up))
                ->Property(
                    "cylinderConvexRadiusFraction",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleCollisionConfiguration::m_cylinderConvexRadiusFraction))
                ->Property(
                    "maximumSlopeAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleCollisionConfiguration::m_maximumSlopeAngle))
                ->Property("sphereRadius", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleCollisionConfiguration::m_sphereRadius))
                ->Property(
                    "collisionLayer",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleCollisionConfiguration::m_collisionLayer))
                ->Property("mode", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleCollisionConfiguration::m_mode));

            behaviorContext.Class<VehicleDifferentialConfiguration>("VehicleDifferentialConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("leftWheel", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleDifferentialConfiguration::m_leftWheel))
                ->Property("rightWheel", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleDifferentialConfiguration::m_rightWheel))
                ->Property(
                    "differentialRatio",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleDifferentialConfiguration::m_differentialRatio))
                ->Property(
                    "engineTorqueRatio",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleDifferentialConfiguration::m_engineTorqueRatio))
                ->Property(
                    "leftRightSplit",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleDifferentialConfiguration::m_leftRightSplit))
                ->Property(
                    "limitedSlipRatio",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleDifferentialConfiguration::m_limitedSlipRatio));

            behaviorContext.Class<VehicleEngineConfiguration>("VehicleEngineConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "normalizedTorque",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleEngineConfiguration::m_normalizedTorque))
                ->Property(
                    "angularDamping",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleEngineConfiguration::m_angularDamping))
                ->Property("inertia", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleEngineConfiguration::m_inertia))
                ->Property(
                    "maximumRpm",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleEngineConfiguration::m_maximumRpm))
                ->Property(
                    "maximumTorque",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleEngineConfiguration::m_maximumTorque))
                ->Property(
                    "minimumRpm",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleEngineConfiguration::m_minimumRpm));

            behaviorContext.Class<VehiclePowertrainControl>("VehiclePowertrainControl")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("currentGear", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehiclePowertrainControl::m_currentGear))
                ->Property(
                    "clutchFriction",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehiclePowertrainControl::m_clutchFriction))
                ->Property("engineRpm", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehiclePowertrainControl::m_engineRpm));

            behaviorContext.Class<VehiclePowertrainState>("VehiclePowertrainState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("currentGear", BehaviorValueGetter(&VehiclePowertrainState::m_currentGear), nullptr)
                ->Property("clutchFriction", BehaviorValueGetter(&VehiclePowertrainState::m_clutchFriction), nullptr)
                ->Property("currentRatio", BehaviorValueGetter(&VehiclePowertrainState::m_currentRatio), nullptr)
                ->Property(
                    "engineAngularVelocity",
                    BehaviorValueGetter(&VehiclePowertrainState::m_engineAngularVelocity),
                    nullptr)
                ->Property("engineRpm", BehaviorValueGetter(&VehiclePowertrainState::m_engineRpm), nullptr)
                ->Property(
                    "wheelSpeedAtClutch",
                    BehaviorValueGetter(&VehiclePowertrainState::m_wheelSpeedAtClutch),
                    nullptr)
                ->Property("isSwitchingGear", BehaviorValueGetter(&VehiclePowertrainState::m_isSwitchingGear), nullptr);

            behaviorContext.Class<VehicleTransmissionConfiguration>("VehicleTransmissionConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "forwardGearRatios",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_forwardGearRatios))
                ->Property(
                    "reverseGearRatios",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_reverseGearRatios))
                ->Property("mode", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_mode))
                ->Property(
                    "clutchReleaseTime",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_clutchReleaseTime))
                ->Property(
                    "clutchStrength",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_clutchStrength))
                ->Property(
                    "shiftDownRpm",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_shiftDownRpm))
                ->Property(
                    "shiftUpRpm",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_shiftUpRpm))
                ->Property(
                    "switchLatency",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_switchLatency))
                ->Property(
                    "switchTime",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTransmissionConfiguration::m_switchTime));

            behaviorContext.Class<VehicleTrackConfiguration>("VehicleTrackConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("wheels", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTrackConfiguration::m_wheels))
                ->Property("drivenWheel", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTrackConfiguration::m_drivenWheel))
                ->Property(
                    "angularDamping",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTrackConfiguration::m_angularDamping))
                ->Property(
                    "differentialRatio",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTrackConfiguration::m_differentialRatio))
                ->Property("inertia", JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTrackConfiguration::m_inertia))
                ->Property(
                    "maximumBrakeTorque",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&VehicleTrackConfiguration::m_maximumBrakeTorque));

            behaviorContext.Class<WheelBasis>("WheelBasis")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("forward", BehaviorValueGetter(&WheelBasis::m_forward), nullptr)
                ->Property("right", BehaviorValueGetter(&WheelBasis::m_right), nullptr)
                ->Property("up", BehaviorValueGetter(&WheelBasis::m_up), nullptr);

            behaviorContext.Class<WheelMotion>("WheelMotion")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("angularVelocity", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheelMotion::m_angularVelocity))
                ->Property("rotationAngle", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheelMotion::m_rotationAngle))
                ->Property("steerAngle", JOLT_BEHAVIOR_VALUE_PROPERTY(&WheelMotion::m_steerAngle));

            behaviorContext.Class<WheelState>("WheelState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("contactPosition", BehaviorValueGetter(&WheelState::m_contactPosition), nullptr)
                ->Property("contactLateral", BehaviorValueGetter(&WheelState::m_contactLateral), nullptr)
                ->Property("contactLongitudinal", BehaviorValueGetter(&WheelState::m_contactLongitudinal), nullptr)
                ->Property("contactNormal", BehaviorValueGetter(&WheelState::m_contactNormal), nullptr)
                ->Property("contactVelocity", BehaviorValueGetter(&WheelState::m_contactVelocity), nullptr)
                ->Property("contactBodyHandle", BehaviorValueGetter(&WheelState::m_contactBodyHandle), nullptr)
                ->Property("contactSubShapeId", BehaviorValueGetter(&WheelState::m_contactSubShapeId), nullptr)
                ->Property("angularVelocity", BehaviorValueGetter(&WheelState::m_angularVelocity), nullptr)
                ->Property("lateralImpulse", BehaviorValueGetter(&WheelState::m_lateralImpulse), nullptr)
                ->Property("longitudinalImpulse", BehaviorValueGetter(&WheelState::m_longitudinalImpulse), nullptr)
                ->Property("rotationAngle", BehaviorValueGetter(&WheelState::m_rotationAngle), nullptr)
                ->Property("steerAngle", BehaviorValueGetter(&WheelState::m_steerAngle), nullptr)
                ->Property("suspensionImpulse", BehaviorValueGetter(&WheelState::m_suspensionImpulse), nullptr)
                ->Property("suspensionLength", BehaviorValueGetter(&WheelState::m_suspensionLength), nullptr)
                ->Property("hasContact", BehaviorValueGetter(&WheelState::m_hasContact), nullptr)
                ->Property("hasHitHardPoint", BehaviorValueGetter(&WheelState::m_hasHitHardPoint), nullptr);

            behaviorContext.Class<WheeledVehicleState>("WheeledVehicleState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("bodyHandle", BehaviorValueGetter(&WheeledVehicleState::m_bodyHandle), nullptr)
                ->Property("kind", BehaviorValueGetter(&WheeledVehicleState::m_kind), nullptr)
                ->Property("wheelCount", BehaviorValueGetter(&WheeledVehicleState::m_wheelCount), nullptr)
                ->Property("currentGear", BehaviorValueGetter(&WheeledVehicleState::m_currentGear), nullptr)
                ->Property("clutchFriction", BehaviorValueGetter(&WheeledVehicleState::m_clutchFriction), nullptr)
                ->Property("engineRpm", BehaviorValueGetter(&WheeledVehicleState::m_engineRpm), nullptr)
                ->Property("isSwitchingGear", BehaviorValueGetter(&WheeledVehicleState::m_isSwitchingGear), nullptr);

            behaviorContext.Class<MotorcycleState>("MotorcycleState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("wheeled", BehaviorValueGetter(&MotorcycleState::m_wheeled), nullptr)
                ->Property("wheelBase", BehaviorValueGetter(&MotorcycleState::m_wheelBase), nullptr);

            behaviorContext.Class<VehicleTrackState>("VehicleTrackState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("angularVelocity", BehaviorValueGetter(&VehicleTrackState::m_angularVelocity), nullptr);

            behaviorContext.Class<TrackedVehicleState>("TrackedVehicleState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("bodyHandle", BehaviorValueGetter(&TrackedVehicleState::m_bodyHandle), nullptr)
                ->Property("tracks", BehaviorValueGetter(&TrackedVehicleState::m_tracks), nullptr)
                ->Property("kind", BehaviorValueGetter(&TrackedVehicleState::m_kind), nullptr)
                ->Property("wheelCount", BehaviorValueGetter(&TrackedVehicleState::m_wheelCount), nullptr)
                ->Property("currentGear", BehaviorValueGetter(&TrackedVehicleState::m_currentGear), nullptr)
                ->Property("clutchFriction", BehaviorValueGetter(&TrackedVehicleState::m_clutchFriction), nullptr)
                ->Property("engineRpm", BehaviorValueGetter(&TrackedVehicleState::m_engineRpm), nullptr)
                ->Property("isSwitchingGear", BehaviorValueGetter(&TrackedVehicleState::m_isSwitchingGear), nullptr);

            behaviorContext.EBus<WheeledVehicleRequestBus>("JoltWheeledVehicleRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("ApplyEngineDamping", &IVehicleRequests::ApplyEngineDamping)
                ->Event("ApplyEngineTorque", &IVehicleRequests::ApplyEngineTorque)
                ->Event("CalculateEngineTorque", &IVehicleRequests::CalculateEngineTorque)
                ->Event("CopyAntiRollBars", &IVehicleRequests::CopyAntiRollBars)
                ->Event("CopyDifferentials", &IVehicleRequests::CopyDifferentials)
                ->Event("CopyWheelStates", &IWheeledVehicleRequests::CopyWheelStates)
                ->Event("DisableSimulation", &IVehicleRequests::DisableSimulation)
                ->Event("EnableSimulation", &IVehicleRequests::EnableSimulation)
                ->Event("GetCollisionConfiguration", &IVehicleRequests::GetCollisionConfiguration)
                ->Event("GetDifferentialLimitedSlipRatio", &IVehicleRequests::GetDifferentialLimitedSlipRatio)
                ->Event("GetEngineConfiguration", &IVehicleRequests::GetEngineConfiguration)
                ->Event("GetPowertrainState", &IVehicleRequests::GetPowertrainState)
                ->Event("GetRuntimeConfiguration", &IVehicleRequests::GetRuntimeConfiguration)
                ->Event("GetState", &IWheeledVehicleRequests::GetState)
                ->Event("GetTransmissionConfiguration", &IVehicleRequests::GetTransmissionConfiguration)
                ->Event("GetVehicleHandle", &IVehicleRequests::GetVehicleHandle)
                ->Event("GetWheelLocalBasis", &IVehicleRequests::GetWheelLocalBasis)
                ->Event("GetWheelLocalTransform", &IVehicleRequests::GetWheelLocalTransform)
                ->Event("GetWheelWorldTransform", &IVehicleRequests::GetWheelWorldTransform)
                ->Event("IsSimulationEnabled", &IVehicleRequests::IsSimulationEnabled)
                ->Event("SetInput", &IWheeledVehicleRequests::SetInput)
                ->Event("SetDifferentialLimitedSlipRatio", &IVehicleRequests::SetDifferentialLimitedSlipRatio)
                ->Event("SetPowertrainControl", &IVehicleRequests::SetPowertrainControl)
                ->Event("SetWheelMotion", &IVehicleRequests::SetWheelMotion)
                ->Event("UpdateAntiRollBars", &IVehicleRequests::UpdateAntiRollBars)
                ->Event("UpdateCollisionConfiguration", &IVehicleRequests::UpdateCollisionConfiguration)
                ->Event("UpdateDifferentials", &IVehicleRequests::UpdateDifferentials)
                ->Event("UpdateEngineConfiguration", &IVehicleRequests::UpdateEngineConfiguration)
                ->Event("UpdateRuntimeConfiguration", &IVehicleRequests::UpdateRuntimeConfiguration)
                ->Event("UpdateTransmissionConfiguration", &IVehicleRequests::UpdateTransmissionConfiguration);

            behaviorContext.EBus<MotorcycleRequestBus>("JoltMotorcycleRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("ApplyEngineDamping", &IVehicleRequests::ApplyEngineDamping)
                ->Event("ApplyEngineTorque", &IVehicleRequests::ApplyEngineTorque)
                ->Event("CalculateEngineTorque", &IVehicleRequests::CalculateEngineTorque)
                ->Event("CopyAntiRollBars", &IVehicleRequests::CopyAntiRollBars)
                ->Event("CopyDifferentials", &IVehicleRequests::CopyDifferentials)
                ->Event("CopyWheelStates", &IMotorcycleRequests::CopyWheelStates)
                ->Event("DisableSimulation", &IVehicleRequests::DisableSimulation)
                ->Event("EnableSimulation", &IVehicleRequests::EnableSimulation)
                ->Event("GetCollisionConfiguration", &IVehicleRequests::GetCollisionConfiguration)
                ->Event("GetDifferentialLimitedSlipRatio", &IVehicleRequests::GetDifferentialLimitedSlipRatio)
                ->Event("GetEngineConfiguration", &IVehicleRequests::GetEngineConfiguration)
                ->Event("GetPowertrainState", &IVehicleRequests::GetPowertrainState)
                ->Event("GetRuntimeConfiguration", &IVehicleRequests::GetRuntimeConfiguration)
                ->Event("GetState", &IMotorcycleRequests::GetState)
                ->Event("GetTransmissionConfiguration", &IVehicleRequests::GetTransmissionConfiguration)
                ->Event("GetVehicleHandle", &IVehicleRequests::GetVehicleHandle)
                ->Event("GetWheelLocalBasis", &IVehicleRequests::GetWheelLocalBasis)
                ->Event("GetWheelLocalTransform", &IVehicleRequests::GetWheelLocalTransform)
                ->Event("GetWheelWorldTransform", &IVehicleRequests::GetWheelWorldTransform)
                ->Event("IsSimulationEnabled", &IVehicleRequests::IsSimulationEnabled)
                ->Event("SetInput", &IMotorcycleRequests::SetInput)
                ->Event("SetDifferentialLimitedSlipRatio", &IVehicleRequests::SetDifferentialLimitedSlipRatio)
                ->Event("SetPowertrainControl", &IVehicleRequests::SetPowertrainControl)
                ->Event("SetWheelMotion", &IVehicleRequests::SetWheelMotion)
                ->Event("UpdateAntiRollBars", &IVehicleRequests::UpdateAntiRollBars)
                ->Event("UpdateCollisionConfiguration", &IVehicleRequests::UpdateCollisionConfiguration)
                ->Event("UpdateController", &IMotorcycleRequests::UpdateController)
                ->Event("UpdateDifferentials", &IVehicleRequests::UpdateDifferentials)
                ->Event("UpdateEngineConfiguration", &IVehicleRequests::UpdateEngineConfiguration)
                ->Event("UpdateRuntimeConfiguration", &IVehicleRequests::UpdateRuntimeConfiguration)
                ->Event("UpdateTransmissionConfiguration", &IVehicleRequests::UpdateTransmissionConfiguration);

            behaviorContext.EBus<TrackedVehicleRequestBus>("JoltTrackedVehicleRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("ApplyEngineDamping", &IVehicleRequests::ApplyEngineDamping)
                ->Event("ApplyEngineTorque", &IVehicleRequests::ApplyEngineTorque)
                ->Event("CalculateEngineTorque", &IVehicleRequests::CalculateEngineTorque)
                ->Event("CopyAntiRollBars", &IVehicleRequests::CopyAntiRollBars)
                ->Event("CopyDifferentials", &IVehicleRequests::CopyDifferentials)
                ->Event("CopyWheelStates", &ITrackedVehicleRequests::CopyWheelStates)
                ->Event("DisableSimulation", &IVehicleRequests::DisableSimulation)
                ->Event("EnableSimulation", &IVehicleRequests::EnableSimulation)
                ->Event("GetCollisionConfiguration", &IVehicleRequests::GetCollisionConfiguration)
                ->Event("GetDifferentialLimitedSlipRatio", &IVehicleRequests::GetDifferentialLimitedSlipRatio)
                ->Event("GetEngineConfiguration", &IVehicleRequests::GetEngineConfiguration)
                ->Event("GetPowertrainState", &IVehicleRequests::GetPowertrainState)
                ->Event("GetRuntimeConfiguration", &IVehicleRequests::GetRuntimeConfiguration)
                ->Event("GetState", &ITrackedVehicleRequests::GetState)
                ->Event("GetTrackConfiguration", &ITrackedVehicleRequests::GetTrackConfiguration)
                ->Event("GetTransmissionConfiguration", &IVehicleRequests::GetTransmissionConfiguration)
                ->Event("GetVehicleHandle", &IVehicleRequests::GetVehicleHandle)
                ->Event("GetWheelLocalBasis", &IVehicleRequests::GetWheelLocalBasis)
                ->Event("GetWheelLocalTransform", &IVehicleRequests::GetWheelLocalTransform)
                ->Event("GetWheelWorldTransform", &IVehicleRequests::GetWheelWorldTransform)
                ->Event("IsSimulationEnabled", &IVehicleRequests::IsSimulationEnabled)
                ->Event("SetInput", &ITrackedVehicleRequests::SetInput)
                ->Event("SetDifferentialLimitedSlipRatio", &IVehicleRequests::SetDifferentialLimitedSlipRatio)
                ->Event("SetPowertrainControl", &IVehicleRequests::SetPowertrainControl)
                ->Event("SetTrackAngularVelocity", &ITrackedVehicleRequests::SetTrackAngularVelocity)
                ->Event("SetWheelMotion", &IVehicleRequests::SetWheelMotion)
                ->Event("UpdateAntiRollBars", &IVehicleRequests::UpdateAntiRollBars)
                ->Event("UpdateCollisionConfiguration", &IVehicleRequests::UpdateCollisionConfiguration)
                ->Event("UpdateDifferentials", &IVehicleRequests::UpdateDifferentials)
                ->Event("UpdateEngineConfiguration", &IVehicleRequests::UpdateEngineConfiguration)
                ->Event("UpdateRuntimeConfiguration", &IVehicleRequests::UpdateRuntimeConfiguration)
                ->Event("UpdateTrackConfiguration", &ITrackedVehicleRequests::UpdateTrackConfiguration)
                ->Event("UpdateTransmissionConfiguration", &IVehicleRequests::UpdateTransmissionConfiguration);

            behaviorContext.EBus<VehicleNotificationBus>("JoltVehicleNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<VehicleNotificationBusBehaviorHandler>();
        }
    } // namespace

    bool VehicleComponentBase::ApplyEngineDamping(const float deltaTime)
    {
        return m_system
            && m_system->ApplyVehicleEngineDamping(
                m_worldHandle,
                m_vehicleHandle,
                deltaTime);
    }

    bool VehicleComponentBase::ApplyEngineTorque(
        const float torque,
        const float deltaTime)
    {
        return m_system
            && m_system->ApplyVehicleEngineTorque(
                m_worldHandle,
                m_vehicleHandle,
                torque,
                deltaTime);
    }

    float VehicleComponentBase::CalculateEngineTorque(const float acceleration) const
    {
        float torque = 0.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool calculated = m_system->CalculateVehicleEngineTorque(
                m_worldHandle,
                m_vehicleHandle,
                acceleration,
                torque);
        }
        return torque;
    }

    bool VehicleComponentBase::EnableSimulation()
    {
        m_enabled = true;
        if (m_vehicleHandle)
        {
            return true;
        }
        if (!m_system || !m_entityId.IsValid())
        {
            return false;
        }

        WorldHandle worldHandle;
        BodyHandle bodyHandle;
        BodyRequestBus::EventResult(
            worldHandle,
            m_entityId,
            &IBodyRequests::GetWorldHandle);
        BodyRequestBus::EventResult(
            bodyHandle,
            m_entityId,
            &IBodyRequests::GetBodyHandle);
        if (!worldHandle || !bodyHandle)
        {
            return false;
        }

        m_worldHandle = worldHandle;
        m_bodyHandle = bodyHandle;
        m_vehicleHandle = CreateVehicle(*m_system, worldHandle, bodyHandle);
        if (!m_vehicleHandle)
        {
            m_worldHandle = WorldHandle::Invalid;
            m_bodyHandle = BodyHandle::Invalid;
            return false;
        }
        if ((m_callbackBindings
                && m_callbackBindings->m_callbacks
                && !m_system->SetVehicleCallbacks(
                    m_worldHandle,
                    m_vehicleHandle,
                    m_callbackBindings->m_callbacks))
            || (m_callbackBindings
                && m_callbackBindings->m_collisionFilter
                && !m_system->SetVehicleCollisionFilter(
                    m_worldHandle,
                    m_vehicleHandle,
                    m_callbackBindings->m_collisionFilter)))
        {
            [[maybe_unused]] const bool destroyed =
                m_system->DestroyVehicle(m_worldHandle, m_vehicleHandle);
            m_vehicleHandle = VehicleHandle::Invalid;
            m_bodyHandle = BodyHandle::Invalid;
            m_worldHandle = WorldHandle::Invalid;
            return false;
        }

        VehicleNotificationBus::Event(
            m_entityId,
            &IVehicleNotifications::OnVehicleCreated,
            m_vehicleHandle);
        return true;
    }

    bool VehicleComponentBase::DisableSimulation()
    {
        m_enabled = false;
        if (!m_vehicleHandle)
        {
            return true;
        }

        const VehicleHandle vehicleHandle = m_vehicleHandle;
        VehicleNotificationBus::Event(
            m_entityId,
            &IVehicleNotifications::OnVehicleDestroying,
            vehicleHandle);
        if (!m_system->DestroyVehicle(m_worldHandle, vehicleHandle))
        {
            return false;
        }

        m_vehicleHandle = VehicleHandle::Invalid;
        m_bodyHandle = BodyHandle::Invalid;
        m_worldHandle = WorldHandle::Invalid;
        VehicleNotificationBus::Event(
            m_entityId,
            &IVehicleNotifications::OnVehicleDestroyed,
            vehicleHandle);
        return true;
    }

    bool VehicleComponentBase::IsSimulationEnabled() const
    {
        return m_system && m_vehicleHandle;
    }

    VehicleHandle VehicleComponentBase::GetVehicleHandle() const
    {
        return m_vehicleHandle;
    }

    VehicleRuntimeConfiguration VehicleComponentBase::GetRuntimeConfiguration() const
    {
        VehicleRuntimeConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetVehicleRuntimeConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
        }
        return configuration;
    }

    VehicleCollisionConfiguration VehicleComponentBase::GetCollisionConfiguration() const
    {
        VehicleCollisionConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetVehicleCollisionConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
        }
        return configuration;
    }

    VehicleEngineConfiguration VehicleComponentBase::GetEngineConfiguration() const
    {
        VehicleEngineConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetVehicleEngineConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
        }
        return configuration;
    }

    float VehicleComponentBase::GetDifferentialLimitedSlipRatio() const
    {
        float ratio = 0.0f;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetVehicleDifferentialLimitedSlipRatio(
                m_worldHandle,
                m_vehicleHandle,
                ratio);
        }
        return ratio;
    }

    VehiclePowertrainState VehicleComponentBase::GetPowertrainState() const
    {
        VehiclePowertrainState state;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetVehiclePowertrainState(
                m_worldHandle,
                m_vehicleHandle,
                state);
        }
        return state;
    }

    VehicleTransmissionConfiguration VehicleComponentBase::GetTransmissionConfiguration() const
    {
        VehicleTransmissionConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetVehicleTransmissionConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
        }
        return configuration;
    }

    WheelBasis VehicleComponentBase::GetWheelLocalBasis(const AZ::u32 wheelIndex) const
    {
        WheelBasis basis;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetWheelLocalBasis(
                m_worldHandle,
                m_vehicleHandle,
                wheelIndex,
                basis);
        }
        return basis;
    }

    AZ::Transform VehicleComponentBase::GetWheelLocalTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetWheelLocalTransform(
                m_worldHandle,
                m_vehicleHandle,
                wheelIndex,
                wheelRight,
                wheelUp,
                transform);
        }
        return transform;
    }

    WorldTransform VehicleComponentBase::GetWheelWorldTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        WorldTransform transform;
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetWheelWorldTransform(
                m_worldHandle,
                m_vehicleHandle,
                wheelIndex,
                wheelRight,
                wheelUp,
                transform);
        }
        return transform;
    }

    AZStd::vector<VehicleAntiRollBarConfiguration> VehicleComponentBase::CopyAntiRollBars() const
    {
        if (!m_system)
        {
            return {};
        }

        const QueryResult size = m_system->QueryVehicleAntiRollBars(m_worldHandle, m_vehicleHandle, {});
        AZStd::vector<VehicleAntiRollBarConfiguration> antiRollBars(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult result =
            m_system->QueryVehicleAntiRollBars(m_worldHandle, m_vehicleHandle, antiRollBars);
        return antiRollBars;
    }

    AZStd::vector<VehicleDifferentialConfiguration> VehicleComponentBase::CopyDifferentials() const
    {
        if (!m_system)
        {
            return {};
        }

        const QueryResult size = m_system->QueryVehicleDifferentials(m_worldHandle, m_vehicleHandle, {});
        AZStd::vector<VehicleDifferentialConfiguration> differentials(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult result =
            m_system->QueryVehicleDifferentials(m_worldHandle, m_vehicleHandle, differentials);
        return differentials;
    }

    bool VehicleComponentBase::SetCallbacks(const ExtensionHandle extensionHandle)
    {
        if (!m_system)
        {
            return false;
        }

        ExtensionHandle previousHandle;
        if (m_callbackBindings)
        {
            previousHandle = m_callbackBindings->m_callbacks;
        }
        if (extensionHandle == previousHandle)
        {
            return true;
        }

        AZStd::unique_ptr<CallbackBindings> newBindings;
        if (extensionHandle && !m_callbackBindings)
        {
            newBindings = AZStd::make_unique<CallbackBindings>();
        }
        if (extensionHandle
            && !m_system->RetainExtension(extensionHandle, ExtensionKind::VehicleCallbacks))
        {
            return false;
        }
        if (m_vehicleHandle
            && !m_system->SetVehicleCallbacks(m_worldHandle, m_vehicleHandle, extensionHandle))
        {
            if (extensionHandle)
            {
                m_system->ReleaseExtension(extensionHandle);
            }
            return false;
        }

        if (newBindings)
        {
            m_callbackBindings = AZStd::move(newBindings);
        }
        if (m_callbackBindings)
        {
            m_callbackBindings->m_callbacks = extensionHandle;
            if (!m_callbackBindings->m_callbacks
                && !m_callbackBindings->m_collisionFilter)
            {
                m_callbackBindings.reset();
            }
        }
        if (previousHandle)
        {
            m_system->ReleaseExtension(previousHandle);
        }
        return true;
    }

    bool VehicleComponentBase::SetCollisionFilter(const ExtensionHandle extensionHandle)
    {
        if (!m_system)
        {
            return false;
        }

        ExtensionHandle previousHandle;
        if (m_callbackBindings)
        {
            previousHandle = m_callbackBindings->m_collisionFilter;
        }
        if (extensionHandle == previousHandle)
        {
            return true;
        }

        AZStd::unique_ptr<CallbackBindings> newBindings;
        if (extensionHandle && !m_callbackBindings)
        {
            newBindings = AZStd::make_unique<CallbackBindings>();
        }
        if (extensionHandle
            && !m_system->RetainExtension(extensionHandle, ExtensionKind::VehicleCollisionFilter))
        {
            return false;
        }
        if (m_vehicleHandle
            && !m_system->SetVehicleCollisionFilter(m_worldHandle, m_vehicleHandle, extensionHandle))
        {
            if (extensionHandle)
            {
                m_system->ReleaseExtension(extensionHandle);
            }
            return false;
        }

        if (newBindings)
        {
            m_callbackBindings = AZStd::move(newBindings);
        }
        if (m_callbackBindings)
        {
            m_callbackBindings->m_collisionFilter = extensionHandle;
            if (!m_callbackBindings->m_callbacks
                && !m_callbackBindings->m_collisionFilter)
            {
                m_callbackBindings.reset();
            }
        }
        if (previousHandle)
        {
            m_system->ReleaseExtension(previousHandle);
        }
        return true;
    }

    bool VehicleComponentBase::SetDifferentialLimitedSlipRatio(const float ratio)
    {
        return m_system
            && m_system->SetVehicleDifferentialLimitedSlipRatio(
                m_worldHandle,
                m_vehicleHandle,
                ratio);
    }

    bool VehicleComponentBase::SetPowertrainControl(const VehiclePowertrainControl& control)
    {
        return m_system
            && m_system->SetVehiclePowertrainControl(
                m_worldHandle,
                m_vehicleHandle,
                control);
    }

    bool VehicleComponentBase::SetWheelMotion(
        const AZ::u32 wheelIndex,
        const WheelMotion& motion)
    {
        return m_system
            && m_system->SetWheelMotion(
                m_worldHandle,
                m_vehicleHandle,
                wheelIndex,
                motion);
    }

    bool VehicleComponentBase::UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration)
    {
        return m_system && m_system->UpdateVehicleRuntimeConfiguration(m_worldHandle, m_vehicleHandle, configuration);
    }

    bool VehicleComponentBase::UpdateAntiRollBars(
        const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars)
    {
        return m_system
            && m_system->UpdateVehicleAntiRollBars(
                m_worldHandle,
                m_vehicleHandle,
                antiRollBars);
    }

    bool VehicleComponentBase::UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration)
    {
        return m_system
            && m_system->UpdateVehicleCollisionConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
    }

    bool VehicleComponentBase::UpdateDifferentials(
        const AZStd::vector<VehicleDifferentialConfiguration>& differentials)
    {
        return m_system
            && m_system->UpdateVehicleDifferentials(
                m_worldHandle,
                m_vehicleHandle,
                differentials);
    }

    bool VehicleComponentBase::UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration)
    {
        return m_system
            && m_system->UpdateVehicleEngineConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
    }

    bool VehicleComponentBase::UpdateTransmissionConfiguration(
        const VehicleTransmissionConfiguration& configuration)
    {
        return m_system
            && m_system->UpdateVehicleTransmissionConfiguration(
                m_worldHandle,
                m_vehicleHandle,
                configuration);
    }

    void VehicleComponentBase::ActivateVehicle(
        const AZ::EntityId entityId,
        const bool enabled)
    {
        m_entityId = entityId;
        m_enabled = enabled;
        m_system = GetRuntime();
        m_dependencyManager = AZ::Interface<IComponentDependencyManager>::Get();
        if (!m_system || !m_dependencyManager)
        {
            return;
        }

        m_dependencyManager->RegisterBody(entityId, *this);
        if (enabled)
        {
            EnableSimulation();
        }
    }

    void VehicleComponentBase::DeactivateVehicle()
    {
        DisableSimulation();
        if (m_dependencyManager)
        {
            m_dependencyManager->UnregisterBody(m_entityId, *this);
        }
        if (m_callbackBindings)
        {
            if (m_callbackBindings->m_callbacks)
            {
                m_system->ReleaseExtension(m_callbackBindings->m_callbacks);
            }
            if (m_callbackBindings->m_collisionFilter)
            {
                m_system->ReleaseExtension(m_callbackBindings->m_collisionFilter);
            }
            m_callbackBindings.reset();
        }
        m_dependencyManager = nullptr;
        m_system = nullptr;
        m_entityId.SetInvalid();
    }

    RuntimeImplementation* VehicleComponentBase::GetSystem() const
    {
        return m_system;
    }

    WorldHandle VehicleComponentBase::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    void VehicleComponentBase::OnBodyDependencyCreated(
        [[maybe_unused]] const WorldHandle worldHandle,
        [[maybe_unused]] const BodyHandle bodyHandle)
    {
        if (m_enabled)
        {
            EnableSimulation();
        }
    }

    bool VehicleComponentBase::OnBodyDependencyDestroying(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        if (worldHandle == m_worldHandle && bodyHandle == m_bodyHandle)
        {
            const bool enabled = m_enabled;
            const bool disabled = DisableSimulation();
            m_enabled = enabled;
            return disabled;
        }
        return true;
    }

    WheeledVehicleComponent::WheeledVehicleComponent() = default;

    WheeledVehicleComponent::WheeledVehicleComponent(
        WheeledVehicleComponentConfiguration configuration)
        : m_configuration(
            AZStd::make_unique<WheeledVehicleComponentConfiguration>(AZStd::move(configuration)))
    {
    }

    void WheeledVehicleComponent::Reflect(
        AZ::ReflectContext* context)
    {
        WheeledVehicleComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<WheelState>>();

            serializeContext
                ->Class<WheeledVehicleComponent, AZ::Component>()
                ->Field("Configuration", &WheeledVehicleComponent::m_configuration);
        }
        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            ReflectVehicleBehavior(*behaviorContext);
            behaviorContext->Class<WheeledVehicleComponent>("Jolt::WheeledVehicleComponent")
                ->RequestBus("JoltWheeledVehicleRequestBus");
        }
    }

    void WheeledVehicleComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        GetVehicleProvidedServices(provided);
    }

    void WheeledVehicleComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        GetVehicleIncompatibleServices(incompatible);
    }

    void WheeledVehicleComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        GetVehicleRequiredServices(required);
    }

    bool WheeledVehicleComponent::ApplyEngineDamping(const float deltaTime)
    {
        return VehicleComponentBase::ApplyEngineDamping(deltaTime);
    }

    bool WheeledVehicleComponent::ApplyEngineTorque(
        const float torque,
        const float deltaTime)
    {
        return VehicleComponentBase::ApplyEngineTorque(torque, deltaTime);
    }

    float WheeledVehicleComponent::CalculateEngineTorque(const float acceleration) const
    {
        return VehicleComponentBase::CalculateEngineTorque(acceleration);
    }

    bool WheeledVehicleComponent::EnableSimulation()
    {
        if (!VehicleComponentBase::EnableSimulation())
        {
            return false;
        }
        m_configuration->m_enabled = true;
        return true;
    }

    bool WheeledVehicleComponent::DisableSimulation()
    {
        if (!VehicleComponentBase::DisableSimulation())
        {
            return false;
        }
        m_configuration->m_enabled = false;
        return true;
    }

    bool WheeledVehicleComponent::IsSimulationEnabled() const
    {
        return VehicleComponentBase::IsSimulationEnabled();
    }

    VehicleHandle WheeledVehicleComponent::GetVehicleHandle() const
    {
        return VehicleComponentBase::GetVehicleHandle();
    }

    VehicleRuntimeConfiguration WheeledVehicleComponent::GetRuntimeConfiguration() const
    {
        return VehicleComponentBase::GetRuntimeConfiguration();
    }

    VehicleCollisionConfiguration WheeledVehicleComponent::GetCollisionConfiguration() const
    {
        return VehicleComponentBase::GetCollisionConfiguration();
    }

    VehicleEngineConfiguration WheeledVehicleComponent::GetEngineConfiguration() const
    {
        return VehicleComponentBase::GetEngineConfiguration();
    }

    float WheeledVehicleComponent::GetDifferentialLimitedSlipRatio() const
    {
        return VehicleComponentBase::GetDifferentialLimitedSlipRatio();
    }

    VehiclePowertrainState WheeledVehicleComponent::GetPowertrainState() const
    {
        return VehicleComponentBase::GetPowertrainState();
    }

    VehicleTransmissionConfiguration WheeledVehicleComponent::GetTransmissionConfiguration() const
    {
        return VehicleComponentBase::GetTransmissionConfiguration();
    }

    WheelBasis WheeledVehicleComponent::GetWheelLocalBasis(const AZ::u32 wheelIndex) const
    {
        return VehicleComponentBase::GetWheelLocalBasis(wheelIndex);
    }

    AZ::Transform WheeledVehicleComponent::GetWheelLocalTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        return VehicleComponentBase::GetWheelLocalTransform(wheelIndex, wheelRight, wheelUp);
    }

    WorldTransform WheeledVehicleComponent::GetWheelWorldTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        return VehicleComponentBase::GetWheelWorldTransform(wheelIndex, wheelRight, wheelUp);
    }

    AZStd::vector<VehicleAntiRollBarConfiguration> WheeledVehicleComponent::CopyAntiRollBars() const
    {
        return VehicleComponentBase::CopyAntiRollBars();
    }

    AZStd::vector<VehicleDifferentialConfiguration> WheeledVehicleComponent::CopyDifferentials() const
    {
        return VehicleComponentBase::CopyDifferentials();
    }

    bool WheeledVehicleComponent::SetCallbacks(const ExtensionHandle extensionHandle)
    {
        return VehicleComponentBase::SetCallbacks(extensionHandle);
    }

    bool WheeledVehicleComponent::SetCollisionFilter(const ExtensionHandle extensionHandle)
    {
        return VehicleComponentBase::SetCollisionFilter(extensionHandle);
    }

    bool WheeledVehicleComponent::SetDifferentialLimitedSlipRatio(const float ratio)
    {
        if (!VehicleComponentBase::SetDifferentialLimitedSlipRatio(ratio))
        {
            return false;
        }
        m_configuration->m_vehicle.m_differentialLimitedSlipRatio = ratio;
        return true;
    }

    bool WheeledVehicleComponent::SetPowertrainControl(const VehiclePowertrainControl& control)
    {
        return VehicleComponentBase::SetPowertrainControl(control);
    }

    QueryResult WheeledVehicleComponent::QueryState(
        WheeledVehicleState& state,
        const AZStd::span<WheelState> wheels) const
    {
        RuntimeImplementation* system = GetSystem();
        if (!system)
        {
            return {};
        }
        return system->GetWheeledVehicleState(
            GetWorldHandle(),
            GetVehicleHandle(),
            state,
            wheels);
    }

    WheeledVehicleState WheeledVehicleComponent::GetState() const
    {
        WheeledVehicleState state;
        [[maybe_unused]] const QueryResult result = QueryState(state, {});
        return state;
    }

    AZStd::vector<WheelState> WheeledVehicleComponent::CopyWheelStates() const
    {
        WheeledVehicleState state;
        const QueryResult size = QueryState(state, {});
        AZStd::vector<WheelState> wheels(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult result = QueryState(state, wheels);
        return wheels;
    }

    bool WheeledVehicleComponent::SetInput(const WheeledVehicleInput& input)
    {
        RuntimeImplementation* system = GetSystem();
        return system
            && system->SetWheeledVehicleInput(GetWorldHandle(), GetVehicleHandle(), input);
    }

    bool WheeledVehicleComponent::SetWheelMotion(
        const AZ::u32 wheelIndex,
        const WheelMotion& motion)
    {
        return VehicleComponentBase::SetWheelMotion(
            wheelIndex,
            motion);
    }

    bool WheeledVehicleComponent::UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateRuntimeConfiguration(configuration))
        {
            return false;
        }
        ApplyRuntimeConfiguration(m_configuration->m_vehicle, configuration);
        return true;
    }

    bool WheeledVehicleComponent::UpdateAntiRollBars(
        const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars)
    {
        if (!VehicleComponentBase::UpdateAntiRollBars(antiRollBars))
        {
            return false;
        }
        m_configuration->m_vehicle.m_antiRollBars = antiRollBars;
        return true;
    }

    bool WheeledVehicleComponent::UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateCollisionConfiguration(configuration))
        {
            return false;
        }
        ApplyCollisionConfiguration(m_configuration->m_vehicle, configuration);
        return true;
    }

    bool WheeledVehicleComponent::UpdateDifferentials(
        const AZStd::vector<VehicleDifferentialConfiguration>& differentials)
    {
        if (!VehicleComponentBase::UpdateDifferentials(differentials))
        {
            return false;
        }
        m_configuration->m_vehicle.m_differentials = differentials;
        return true;
    }

    bool WheeledVehicleComponent::UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateEngineConfiguration(configuration))
        {
            return false;
        }
        m_configuration->m_vehicle.m_engine = configuration;
        return true;
    }

    bool WheeledVehicleComponent::UpdateTransmissionConfiguration(
        const VehicleTransmissionConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateTransmissionConfiguration(configuration))
        {
            return false;
        }
        m_configuration->m_vehicle.m_transmission = configuration;
        return true;
    }

    void WheeledVehicleComponent::Activate()
    {
        if (!m_configuration)
        {
            m_configuration = AZStd::make_unique<WheeledVehicleComponentConfiguration>(
                WheeledVehicleComponentConfiguration::CreateDefault());
        }

        WheeledVehicleRequestBus::Handler::BusConnect(GetEntityId());
        ActivateVehicle(GetEntityId(), m_configuration->m_enabled);
    }

    void WheeledVehicleComponent::Deactivate()
    {
        DeactivateVehicle();
        WheeledVehicleRequestBus::Handler::BusDisconnect();
    }

    VehicleHandle WheeledVehicleComponent::CreateVehicle(
        RuntimeImplementation& system,
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        WheeledVehicleConfiguration configuration = m_configuration->m_vehicle;
        configuration.m_bodyHandle = bodyHandle;
        return system.CreateWheeledVehicle(worldHandle, configuration);
    }

    MotorcycleComponent::MotorcycleComponent() = default;

    MotorcycleComponent::MotorcycleComponent(
        MotorcycleComponentConfiguration configuration)
        : m_configuration(
            AZStd::make_unique<MotorcycleComponentConfiguration>(AZStd::move(configuration)))
    {
    }

    void MotorcycleComponent::Reflect(
        AZ::ReflectContext* context)
    {
        MotorcycleComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<WheelState>>();

            serializeContext
                ->Class<MotorcycleComponent, AZ::Component>()
                ->Field("Configuration", &MotorcycleComponent::m_configuration);
        }
        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            ReflectVehicleBehavior(*behaviorContext);
            behaviorContext->Class<MotorcycleComponent>("Jolt::MotorcycleComponent")
                ->RequestBus("JoltMotorcycleRequestBus");
        }
    }

    void MotorcycleComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        GetVehicleProvidedServices(provided);
    }

    void MotorcycleComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        GetVehicleIncompatibleServices(incompatible);
    }

    void MotorcycleComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        GetVehicleRequiredServices(required);
    }

    bool MotorcycleComponent::ApplyEngineDamping(const float deltaTime)
    {
        return VehicleComponentBase::ApplyEngineDamping(deltaTime);
    }

    bool MotorcycleComponent::ApplyEngineTorque(
        const float torque,
        const float deltaTime)
    {
        return VehicleComponentBase::ApplyEngineTorque(torque, deltaTime);
    }

    float MotorcycleComponent::CalculateEngineTorque(const float acceleration) const
    {
        return VehicleComponentBase::CalculateEngineTorque(acceleration);
    }

    bool MotorcycleComponent::EnableSimulation()
    {
        if (!VehicleComponentBase::EnableSimulation())
        {
            return false;
        }
        m_configuration->m_enabled = true;
        return true;
    }

    bool MotorcycleComponent::DisableSimulation()
    {
        if (!VehicleComponentBase::DisableSimulation())
        {
            return false;
        }
        m_configuration->m_enabled = false;
        return true;
    }

    bool MotorcycleComponent::IsSimulationEnabled() const
    {
        return VehicleComponentBase::IsSimulationEnabled();
    }

    VehicleHandle MotorcycleComponent::GetVehicleHandle() const
    {
        return VehicleComponentBase::GetVehicleHandle();
    }

    VehicleRuntimeConfiguration MotorcycleComponent::GetRuntimeConfiguration() const
    {
        return VehicleComponentBase::GetRuntimeConfiguration();
    }

    VehicleCollisionConfiguration MotorcycleComponent::GetCollisionConfiguration() const
    {
        return VehicleComponentBase::GetCollisionConfiguration();
    }

    VehicleEngineConfiguration MotorcycleComponent::GetEngineConfiguration() const
    {
        return VehicleComponentBase::GetEngineConfiguration();
    }

    float MotorcycleComponent::GetDifferentialLimitedSlipRatio() const
    {
        return VehicleComponentBase::GetDifferentialLimitedSlipRatio();
    }

    VehiclePowertrainState MotorcycleComponent::GetPowertrainState() const
    {
        return VehicleComponentBase::GetPowertrainState();
    }

    VehicleTransmissionConfiguration MotorcycleComponent::GetTransmissionConfiguration() const
    {
        return VehicleComponentBase::GetTransmissionConfiguration();
    }

    WheelBasis MotorcycleComponent::GetWheelLocalBasis(const AZ::u32 wheelIndex) const
    {
        return VehicleComponentBase::GetWheelLocalBasis(wheelIndex);
    }

    AZ::Transform MotorcycleComponent::GetWheelLocalTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        return VehicleComponentBase::GetWheelLocalTransform(wheelIndex, wheelRight, wheelUp);
    }

    WorldTransform MotorcycleComponent::GetWheelWorldTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        return VehicleComponentBase::GetWheelWorldTransform(wheelIndex, wheelRight, wheelUp);
    }

    AZStd::vector<VehicleAntiRollBarConfiguration> MotorcycleComponent::CopyAntiRollBars() const
    {
        return VehicleComponentBase::CopyAntiRollBars();
    }

    AZStd::vector<VehicleDifferentialConfiguration> MotorcycleComponent::CopyDifferentials() const
    {
        return VehicleComponentBase::CopyDifferentials();
    }

    bool MotorcycleComponent::SetCallbacks(const ExtensionHandle extensionHandle)
    {
        return VehicleComponentBase::SetCallbacks(extensionHandle);
    }

    bool MotorcycleComponent::SetCollisionFilter(const ExtensionHandle extensionHandle)
    {
        return VehicleComponentBase::SetCollisionFilter(extensionHandle);
    }

    bool MotorcycleComponent::SetDifferentialLimitedSlipRatio(const float ratio)
    {
        if (!VehicleComponentBase::SetDifferentialLimitedSlipRatio(ratio))
        {
            return false;
        }
        m_configuration->m_motorcycle.m_wheeled.m_differentialLimitedSlipRatio = ratio;
        return true;
    }

    bool MotorcycleComponent::SetPowertrainControl(const VehiclePowertrainControl& control)
    {
        return VehicleComponentBase::SetPowertrainControl(control);
    }

    QueryResult MotorcycleComponent::QueryState(
        MotorcycleState& state,
        const AZStd::span<WheelState> wheels) const
    {
        RuntimeImplementation* system = GetSystem();
        if (!system)
        {
            return {};
        }
        return system->GetMotorcycleState(
            GetWorldHandle(),
            GetVehicleHandle(),
            state,
            wheels);
    }

    MotorcycleState MotorcycleComponent::GetState() const
    {
        MotorcycleState state;
        [[maybe_unused]] const QueryResult result = QueryState(state, {});
        return state;
    }

    AZStd::vector<WheelState> MotorcycleComponent::CopyWheelStates() const
    {
        MotorcycleState state;
        const QueryResult size = QueryState(state, {});
        AZStd::vector<WheelState> wheels(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult result = QueryState(state, wheels);
        return wheels;
    }

    bool MotorcycleComponent::SetInput(const WheeledVehicleInput& input)
    {
        RuntimeImplementation* system = GetSystem();
        return system
            && system->SetWheeledVehicleInput(GetWorldHandle(), GetVehicleHandle(), input);
    }

    bool MotorcycleComponent::SetWheelMotion(
        const AZ::u32 wheelIndex,
        const WheelMotion& motion)
    {
        return VehicleComponentBase::SetWheelMotion(
            wheelIndex,
            motion);
    }

    bool MotorcycleComponent::UpdateController(
        const MotorcycleControllerUpdateConfiguration& configuration)
    {
        RuntimeImplementation* system = GetSystem();
        if (!system || !system->UpdateMotorcycleController(GetWorldHandle(), GetVehicleHandle(), configuration))
        {
            return false;
        }

        MotorcycleControllerConfiguration& controller = m_configuration->m_motorcycle.m_controller;
        controller.m_leanSmoothingFactor = configuration.m_leanSmoothingFactor;
        controller.m_springConstant = configuration.m_springConstant;
        controller.m_springDamping = configuration.m_springDamping;
        controller.m_springIntegrationCoefficient = configuration.m_springIntegrationCoefficient;
        controller.m_springIntegrationCoefficientDecay = configuration.m_springIntegrationCoefficientDecay;
        controller.m_enableLeanController = configuration.m_enableLeanController;
        controller.m_enableLeanSteeringLimit = configuration.m_enableLeanSteeringLimit;
        return true;
    }

    bool MotorcycleComponent::UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateRuntimeConfiguration(configuration))
        {
            return false;
        }
        ApplyRuntimeConfiguration(m_configuration->m_motorcycle.m_wheeled, configuration);
        return true;
    }

    bool MotorcycleComponent::UpdateAntiRollBars(
        const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars)
    {
        if (!VehicleComponentBase::UpdateAntiRollBars(antiRollBars))
        {
            return false;
        }
        m_configuration->m_motorcycle.m_wheeled.m_antiRollBars = antiRollBars;
        return true;
    }

    bool MotorcycleComponent::UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateCollisionConfiguration(configuration))
        {
            return false;
        }
        ApplyCollisionConfiguration(m_configuration->m_motorcycle.m_wheeled, configuration);
        return true;
    }

    bool MotorcycleComponent::UpdateDifferentials(
        const AZStd::vector<VehicleDifferentialConfiguration>& differentials)
    {
        if (!VehicleComponentBase::UpdateDifferentials(differentials))
        {
            return false;
        }
        m_configuration->m_motorcycle.m_wheeled.m_differentials = differentials;
        return true;
    }

    bool MotorcycleComponent::UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateEngineConfiguration(configuration))
        {
            return false;
        }
        m_configuration->m_motorcycle.m_wheeled.m_engine = configuration;
        return true;
    }

    bool MotorcycleComponent::UpdateTransmissionConfiguration(
        const VehicleTransmissionConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateTransmissionConfiguration(configuration))
        {
            return false;
        }
        m_configuration->m_motorcycle.m_wheeled.m_transmission = configuration;
        return true;
    }

    void MotorcycleComponent::Activate()
    {
        if (!m_configuration)
        {
            m_configuration = AZStd::make_unique<MotorcycleComponentConfiguration>(
                MotorcycleComponentConfiguration::CreateDefault());
        }

        MotorcycleRequestBus::Handler::BusConnect(GetEntityId());
        ActivateVehicle(GetEntityId(), m_configuration->m_enabled);
    }

    void MotorcycleComponent::Deactivate()
    {
        DeactivateVehicle();
        MotorcycleRequestBus::Handler::BusDisconnect();
    }

    VehicleHandle MotorcycleComponent::CreateVehicle(
        RuntimeImplementation& system,
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        MotorcycleConfiguration configuration = m_configuration->m_motorcycle;
        configuration.m_wheeled.m_bodyHandle = bodyHandle;
        return system.CreateMotorcycle(worldHandle, configuration);
    }

    TrackedVehicleComponent::TrackedVehicleComponent() = default;

    TrackedVehicleComponent::TrackedVehicleComponent(
        TrackedVehicleComponentConfiguration configuration)
        : m_configuration(
            AZStd::make_unique<TrackedVehicleComponentConfiguration>(AZStd::move(configuration)))
    {
    }

    void TrackedVehicleComponent::Reflect(
        AZ::ReflectContext* context)
    {
        TrackedVehicleComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<WheelState>>();

            serializeContext
                ->Class<TrackedVehicleComponent, AZ::Component>()
                ->Field("Configuration", &TrackedVehicleComponent::m_configuration);
        }
        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            ReflectVehicleBehavior(*behaviorContext);
            behaviorContext->Class<TrackedVehicleComponent>("Jolt::TrackedVehicleComponent")
                ->RequestBus("JoltTrackedVehicleRequestBus");
        }
    }

    void TrackedVehicleComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        GetVehicleProvidedServices(provided);
    }

    void TrackedVehicleComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        GetVehicleIncompatibleServices(incompatible);
    }

    void TrackedVehicleComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        GetVehicleRequiredServices(required);
    }

    bool TrackedVehicleComponent::ApplyEngineDamping(const float deltaTime)
    {
        return VehicleComponentBase::ApplyEngineDamping(deltaTime);
    }

    bool TrackedVehicleComponent::ApplyEngineTorque(
        const float torque,
        const float deltaTime)
    {
        return VehicleComponentBase::ApplyEngineTorque(torque, deltaTime);
    }

    float TrackedVehicleComponent::CalculateEngineTorque(const float acceleration) const
    {
        return VehicleComponentBase::CalculateEngineTorque(acceleration);
    }

    bool TrackedVehicleComponent::EnableSimulation()
    {
        if (!VehicleComponentBase::EnableSimulation())
        {
            return false;
        }
        m_configuration->m_enabled = true;
        return true;
    }

    bool TrackedVehicleComponent::DisableSimulation()
    {
        if (!VehicleComponentBase::DisableSimulation())
        {
            return false;
        }
        m_configuration->m_enabled = false;
        return true;
    }

    bool TrackedVehicleComponent::IsSimulationEnabled() const
    {
        return VehicleComponentBase::IsSimulationEnabled();
    }

    VehicleHandle TrackedVehicleComponent::GetVehicleHandle() const
    {
        return VehicleComponentBase::GetVehicleHandle();
    }

    VehicleRuntimeConfiguration TrackedVehicleComponent::GetRuntimeConfiguration() const
    {
        return VehicleComponentBase::GetRuntimeConfiguration();
    }

    VehicleCollisionConfiguration TrackedVehicleComponent::GetCollisionConfiguration() const
    {
        return VehicleComponentBase::GetCollisionConfiguration();
    }

    VehicleEngineConfiguration TrackedVehicleComponent::GetEngineConfiguration() const
    {
        return VehicleComponentBase::GetEngineConfiguration();
    }

    float TrackedVehicleComponent::GetDifferentialLimitedSlipRatio() const
    {
        return VehicleComponentBase::GetDifferentialLimitedSlipRatio();
    }

    VehiclePowertrainState TrackedVehicleComponent::GetPowertrainState() const
    {
        return VehicleComponentBase::GetPowertrainState();
    }

    VehicleTransmissionConfiguration TrackedVehicleComponent::GetTransmissionConfiguration() const
    {
        return VehicleComponentBase::GetTransmissionConfiguration();
    }

    VehicleTrackConfiguration TrackedVehicleComponent::GetTrackConfiguration(const AZ::u32 trackIndex) const
    {
        VehicleTrackConfiguration configuration;
        RuntimeImplementation* system = GetSystem();
        if (system)
        {
            [[maybe_unused]] const bool found = system->GetVehicleTrackConfiguration(
                GetWorldHandle(),
                GetVehicleHandle(),
                trackIndex,
                configuration);
        }
        return configuration;
    }

    WheelBasis TrackedVehicleComponent::GetWheelLocalBasis(const AZ::u32 wheelIndex) const
    {
        return VehicleComponentBase::GetWheelLocalBasis(wheelIndex);
    }

    AZ::Transform TrackedVehicleComponent::GetWheelLocalTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        return VehicleComponentBase::GetWheelLocalTransform(wheelIndex, wheelRight, wheelUp);
    }

    WorldTransform TrackedVehicleComponent::GetWheelWorldTransform(
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp) const
    {
        return VehicleComponentBase::GetWheelWorldTransform(wheelIndex, wheelRight, wheelUp);
    }

    AZStd::vector<VehicleAntiRollBarConfiguration> TrackedVehicleComponent::CopyAntiRollBars() const
    {
        return VehicleComponentBase::CopyAntiRollBars();
    }

    AZStd::vector<VehicleDifferentialConfiguration> TrackedVehicleComponent::CopyDifferentials() const
    {
        return VehicleComponentBase::CopyDifferentials();
    }

    bool TrackedVehicleComponent::SetCallbacks(const ExtensionHandle extensionHandle)
    {
        return VehicleComponentBase::SetCallbacks(extensionHandle);
    }

    bool TrackedVehicleComponent::SetCollisionFilter(const ExtensionHandle extensionHandle)
    {
        return VehicleComponentBase::SetCollisionFilter(extensionHandle);
    }

    bool TrackedVehicleComponent::SetDifferentialLimitedSlipRatio(const float ratio)
    {
        return VehicleComponentBase::SetDifferentialLimitedSlipRatio(ratio);
    }

    bool TrackedVehicleComponent::SetPowertrainControl(const VehiclePowertrainControl& control)
    {
        return VehicleComponentBase::SetPowertrainControl(control);
    }

    QueryResult TrackedVehicleComponent::QueryState(
        TrackedVehicleState& state,
        const AZStd::span<WheelState> wheels) const
    {
        RuntimeImplementation* system = GetSystem();
        if (!system)
        {
            return {};
        }
        return system->GetTrackedVehicleState(
            GetWorldHandle(),
            GetVehicleHandle(),
            state,
            wheels);
    }

    TrackedVehicleState TrackedVehicleComponent::GetState() const
    {
        TrackedVehicleState state;
        [[maybe_unused]] const QueryResult result = QueryState(state, {});
        return state;
    }

    AZStd::vector<WheelState> TrackedVehicleComponent::CopyWheelStates() const
    {
        TrackedVehicleState state;
        const QueryResult size = QueryState(state, {});
        AZStd::vector<WheelState> wheels(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult result = QueryState(state, wheels);
        return wheels;
    }

    bool TrackedVehicleComponent::SetInput(const TrackedVehicleInput& input)
    {
        RuntimeImplementation* system = GetSystem();
        return system
            && system->SetTrackedVehicleInput(GetWorldHandle(), GetVehicleHandle(), input);
    }

    bool TrackedVehicleComponent::SetTrackAngularVelocity(
        const AZ::u32 trackIndex,
        const float angularVelocity)
    {
        RuntimeImplementation* system = GetSystem();
        return system
            && system->SetVehicleTrackAngularVelocity(
                GetWorldHandle(),
                GetVehicleHandle(),
                trackIndex,
                angularVelocity);
    }

    bool TrackedVehicleComponent::SetWheelMotion(
        const AZ::u32 wheelIndex,
        const WheelMotion& motion)
    {
        return VehicleComponentBase::SetWheelMotion(
            wheelIndex,
            motion);
    }

    bool TrackedVehicleComponent::UpdateRuntimeConfiguration(const VehicleRuntimeConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateRuntimeConfiguration(configuration))
        {
            return false;
        }
        ApplyRuntimeConfiguration(m_configuration->m_vehicle, configuration);
        return true;
    }

    bool TrackedVehicleComponent::UpdateAntiRollBars(
        const AZStd::vector<VehicleAntiRollBarConfiguration>& antiRollBars)
    {
        if (!VehicleComponentBase::UpdateAntiRollBars(antiRollBars))
        {
            return false;
        }
        m_configuration->m_vehicle.m_antiRollBars = antiRollBars;
        return true;
    }

    bool TrackedVehicleComponent::UpdateCollisionConfiguration(const VehicleCollisionConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateCollisionConfiguration(configuration))
        {
            return false;
        }
        ApplyCollisionConfiguration(m_configuration->m_vehicle, configuration);
        return true;
    }

    bool TrackedVehicleComponent::UpdateDifferentials(
        const AZStd::vector<VehicleDifferentialConfiguration>& differentials)
    {
        return VehicleComponentBase::UpdateDifferentials(differentials);
    }

    bool TrackedVehicleComponent::UpdateEngineConfiguration(const VehicleEngineConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateEngineConfiguration(configuration))
        {
            return false;
        }
        m_configuration->m_vehicle.m_engine = configuration;
        return true;
    }

    bool TrackedVehicleComponent::UpdateTransmissionConfiguration(
        const VehicleTransmissionConfiguration& configuration)
    {
        if (!VehicleComponentBase::UpdateTransmissionConfiguration(configuration))
        {
            return false;
        }
        m_configuration->m_vehicle.m_transmission = configuration;
        return true;
    }

    bool TrackedVehicleComponent::UpdateTrackConfiguration(
        const AZ::u32 trackIndex,
        const VehicleTrackConfiguration& configuration)
    {
        RuntimeImplementation* system = GetSystem();
        if (!system
            || trackIndex >= m_configuration->m_vehicle.m_tracks.size()
            || !system->UpdateVehicleTrackConfiguration(
                GetWorldHandle(),
                GetVehicleHandle(),
                trackIndex,
                configuration))
        {
            return false;
        }
        m_configuration->m_vehicle.m_tracks[trackIndex] = configuration;
        return true;
    }

    void TrackedVehicleComponent::Activate()
    {
        if (!m_configuration)
        {
            m_configuration = AZStd::make_unique<TrackedVehicleComponentConfiguration>(
                TrackedVehicleComponentConfiguration::CreateDefault());
        }

        TrackedVehicleRequestBus::Handler::BusConnect(GetEntityId());
        ActivateVehicle(GetEntityId(), m_configuration->m_enabled);
    }

    void TrackedVehicleComponent::Deactivate()
    {
        DeactivateVehicle();
        TrackedVehicleRequestBus::Handler::BusDisconnect();
    }

    VehicleHandle TrackedVehicleComponent::CreateVehicle(
        RuntimeImplementation& system,
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        TrackedVehicleConfiguration configuration = m_configuration->m_vehicle;
        configuration.m_bodyHandle = bodyHandle;
        return system.CreateTrackedVehicle(worldHandle, configuration);
    }
} // namespace Jolt
