/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ConstraintComponentConfiguration.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void ConstraintComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        WorldPosition::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<ConstraintComponentConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SpringConfiguration>()
                ->Field("Mode", &SpringConfiguration::m_mode)
                ->Field("FrequencyOrStiffness", &SpringConfiguration::m_frequencyOrStiffness)
                ->Field("Damping", &SpringConfiguration::m_damping);

            serializeContext
                ->Class<MotorConfiguration>()
                ->Field("Spring", &MotorConfiguration::m_spring)
                ->Field("MaximumForce", &MotorConfiguration::m_maximumForce)
                ->Field("MaximumTorque", &MotorConfiguration::m_maximumTorque)
                ->Field("MinimumForce", &MotorConfiguration::m_minimumForce)
                ->Field("MinimumTorque", &MotorConfiguration::m_minimumTorque)
                ->Field("State", &MotorConfiguration::m_state);

            serializeContext
                ->Class<ConeConstraintConfiguration>()
                ->Field("FirstPoint", &ConeConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &ConeConstraintConfiguration::m_secondPoint)
                ->Field("FirstTwistAxis", &ConeConstraintConfiguration::m_firstTwistAxis)
                ->Field("SecondTwistAxis", &ConeConstraintConfiguration::m_secondTwistAxis)
                ->Field("HalfConeAngle", &ConeConstraintConfiguration::m_halfConeAngle)
                ->Field("Space", &ConeConstraintConfiguration::m_space);

            serializeContext
                ->Class<CustomConstraintBodyState>()
                ->Field("CenterOfMassTransform", &CustomConstraintBodyState::m_centerOfMassTransform)
                ->Field("AngularVelocity", &CustomConstraintBodyState::m_angularVelocity)
                ->Field("LinearVelocity", &CustomConstraintBodyState::m_linearVelocity);

            serializeContext
                ->Class<CustomConstraintContext>()
                ->Field("FirstBody", &CustomConstraintContext::m_firstBody)
                ->Field("SecondBody", &CustomConstraintContext::m_secondBody)
                ->Field("FirstFrame", &CustomConstraintContext::m_firstFrame)
                ->Field("SecondFrame", &CustomConstraintContext::m_secondFrame)
                ->Field("Baumgarte", &CustomConstraintContext::m_baumgarte)
                ->Field("DeltaTime", &CustomConstraintContext::m_deltaTime);

            serializeContext
                ->Class<CustomConstraintRow>()
                ->Field("FirstAngular", &CustomConstraintRow::m_firstAngular)
                ->Field("FirstLinear", &CustomConstraintRow::m_firstLinear)
                ->Field("SecondAngular", &CustomConstraintRow::m_secondAngular)
                ->Field("SecondLinear", &CustomConstraintRow::m_secondLinear)
                ->Field("Bias", &CustomConstraintRow::m_bias)
                ->Field("Error", &CustomConstraintRow::m_error)
                ->Field("MaximumImpulse", &CustomConstraintRow::m_maximumImpulse)
                ->Field("MinimumImpulse", &CustomConstraintRow::m_minimumImpulse);

            serializeContext
                ->Class<CustomConstraintInfo>()
                ->Field("ProviderId", &CustomConstraintInfo::m_providerId)
                ->Field("ProviderVersion", &CustomConstraintInfo::m_providerVersion)
                ->Field("MaximumRowCount", &CustomConstraintInfo::m_maximumRowCount)
                ->Field("StateByteCount", &CustomConstraintInfo::m_stateByteCount);

            serializeContext
                ->Class<CustomConstraintConfiguration>()
                ->Field("Data", &CustomConstraintConfiguration::m_data)
                ->Field("FirstFrame", &CustomConstraintConfiguration::m_firstFrame)
                ->Field("SecondFrame", &CustomConstraintConfiguration::m_secondFrame)
                ->Field("ProviderId", &CustomConstraintConfiguration::m_providerId)
                ->Field("Space", &CustomConstraintConfiguration::m_space);

            serializeContext
                ->Class<DistanceConstraintConfiguration>()
                ->Field("FirstPoint", &DistanceConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &DistanceConstraintConfiguration::m_secondPoint)
                ->Field("LimitSpring", &DistanceConstraintConfiguration::m_limitSpring)
                ->Field("MaximumDistance", &DistanceConstraintConfiguration::m_maximumDistance)
                ->Field("MinimumDistance", &DistanceConstraintConfiguration::m_minimumDistance)
                ->Field("Space", &DistanceConstraintConfiguration::m_space);

            serializeContext
                ->Class<FixedConstraintConfiguration>()
                ->Field("FirstPoint", &FixedConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &FixedConstraintConfiguration::m_secondPoint)
                ->Field("FirstAxisX", &FixedConstraintConfiguration::m_firstAxisX)
                ->Field("FirstAxisY", &FixedConstraintConfiguration::m_firstAxisY)
                ->Field("SecondAxisX", &FixedConstraintConfiguration::m_secondAxisX)
                ->Field("SecondAxisY", &FixedConstraintConfiguration::m_secondAxisY)
                ->Field("Space", &FixedConstraintConfiguration::m_space)
                ->Field("AutoDetectPoint", &FixedConstraintConfiguration::m_autoDetectPoint);

            serializeContext
                ->Class<GearConstraintComponentConfiguration>()
                ->Field("FirstHingeEntityId", &GearConstraintComponentConfiguration::m_firstHingeEntityId)
                ->Field("SecondHingeEntityId", &GearConstraintComponentConfiguration::m_secondHingeEntityId)
                ->Field("FirstHingeAxis", &GearConstraintComponentConfiguration::m_firstHingeAxis)
                ->Field("SecondHingeAxis", &GearConstraintComponentConfiguration::m_secondHingeAxis)
                ->Field("Ratio", &GearConstraintComponentConfiguration::m_ratio)
                ->Field("Space", &GearConstraintComponentConfiguration::m_space);

            serializeContext
                ->Class<HingeConstraintConfiguration>()
                ->Field("FirstPoint", &HingeConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &HingeConstraintConfiguration::m_secondPoint)
                ->Field("FirstHingeAxis", &HingeConstraintConfiguration::m_firstHingeAxis)
                ->Field("FirstNormalAxis", &HingeConstraintConfiguration::m_firstNormalAxis)
                ->Field("SecondHingeAxis", &HingeConstraintConfiguration::m_secondHingeAxis)
                ->Field("SecondNormalAxis", &HingeConstraintConfiguration::m_secondNormalAxis)
                ->Field("LimitSpring", &HingeConstraintConfiguration::m_limitSpring)
                ->Field("Motor", &HingeConstraintConfiguration::m_motor)
                ->Field("MaximumFrictionTorque", &HingeConstraintConfiguration::m_maximumFrictionTorque)
                ->Field("MaximumLimit", &HingeConstraintConfiguration::m_maximumLimit)
                ->Field("MinimumLimit", &HingeConstraintConfiguration::m_minimumLimit)
                ->Field("TargetAngle", &HingeConstraintConfiguration::m_targetAngle)
                ->Field("TargetAngularVelocity", &HingeConstraintConfiguration::m_targetAngularVelocity)
                ->Field("Space", &HingeConstraintConfiguration::m_space);

            serializeContext
                ->Class<PathConstraintComponentConfiguration>()
                ->Field("PathEntityId", &PathConstraintComponentConfiguration::m_pathEntityId)
                ->Field("PathPosition", &PathConstraintComponentConfiguration::m_pathPosition)
                ->Field("PathRotation", &PathConstraintComponentConfiguration::m_pathRotation)
                ->Field("PositionMotor", &PathConstraintComponentConfiguration::m_positionMotor)
                ->Field("MaximumFrictionForce", &PathConstraintComponentConfiguration::m_maximumFrictionForce)
                ->Field("PathFraction", &PathConstraintComponentConfiguration::m_pathFraction)
                ->Field("TargetPathFraction", &PathConstraintComponentConfiguration::m_targetPathFraction)
                ->Field("TargetVelocity", &PathConstraintComponentConfiguration::m_targetVelocity)
                ->Field("RotationConstraint", &PathConstraintComponentConfiguration::m_rotationConstraint);

            serializeContext
                ->Class<PointConstraintConfiguration>()
                ->Field("FirstPoint", &PointConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &PointConstraintConfiguration::m_secondPoint)
                ->Field("Space", &PointConstraintConfiguration::m_space);

            serializeContext
                ->Class<PulleyConstraintConfiguration>()
                ->Field("FirstBodyPoint", &PulleyConstraintConfiguration::m_firstBodyPoint)
                ->Field("FirstFixedPoint", &PulleyConstraintConfiguration::m_firstFixedPoint)
                ->Field("SecondBodyPoint", &PulleyConstraintConfiguration::m_secondBodyPoint)
                ->Field("SecondFixedPoint", &PulleyConstraintConfiguration::m_secondFixedPoint)
                ->Field("MaximumLength", &PulleyConstraintConfiguration::m_maximumLength)
                ->Field("MinimumLength", &PulleyConstraintConfiguration::m_minimumLength)
                ->Field("Ratio", &PulleyConstraintConfiguration::m_ratio)
                ->Field("Space", &PulleyConstraintConfiguration::m_space);

            serializeContext
                ->Class<RackAndPinionConstraintComponentConfiguration>()
                ->Field("PinionConstraintEntityId", &RackAndPinionConstraintComponentConfiguration::m_pinionConstraintEntityId)
                ->Field("RackConstraintEntityId", &RackAndPinionConstraintComponentConfiguration::m_rackConstraintEntityId)
                ->Field("HingeAxis", &RackAndPinionConstraintComponentConfiguration::m_hingeAxis)
                ->Field("SliderAxis", &RackAndPinionConstraintComponentConfiguration::m_sliderAxis)
                ->Field("Ratio", &RackAndPinionConstraintComponentConfiguration::m_ratio)
                ->Field("Space", &RackAndPinionConstraintComponentConfiguration::m_space);

            serializeContext
                ->Class<SixDofAxisConfiguration>()
                ->Field("LimitSpring", &SixDofAxisConfiguration::m_limitSpring)
                ->Field("Motor", &SixDofAxisConfiguration::m_motor)
                ->Field("MaximumFriction", &SixDofAxisConfiguration::m_maximumFriction)
                ->Field("MaximumLimit", &SixDofAxisConfiguration::m_maximumLimit)
                ->Field("MinimumLimit", &SixDofAxisConfiguration::m_minimumLimit)
                ->Field("Mode", &SixDofAxisConfiguration::m_mode);

            serializeContext
                ->Class<SixDofConstraintConfiguration>()
                ->Field("FirstPoint", &SixDofConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &SixDofConstraintConfiguration::m_secondPoint)
                ->Field("FirstAxisX", &SixDofConstraintConfiguration::m_firstAxisX)
                ->Field("FirstAxisY", &SixDofConstraintConfiguration::m_firstAxisY)
                ->Field("SecondAxisX", &SixDofConstraintConfiguration::m_secondAxisX)
                ->Field("SecondAxisY", &SixDofConstraintConfiguration::m_secondAxisY)
                ->Field("RotationX", &SixDofConstraintConfiguration::m_rotationX)
                ->Field("RotationY", &SixDofConstraintConfiguration::m_rotationY)
                ->Field("RotationZ", &SixDofConstraintConfiguration::m_rotationZ)
                ->Field("TranslationX", &SixDofConstraintConfiguration::m_translationX)
                ->Field("TranslationY", &SixDofConstraintConfiguration::m_translationY)
                ->Field("TranslationZ", &SixDofConstraintConfiguration::m_translationZ)
                ->Field("TargetAngularVelocity", &SixDofConstraintConfiguration::m_targetAngularVelocity)
                ->Field("TargetPosition", &SixDofConstraintConfiguration::m_targetPosition)
                ->Field("TargetOrientation", &SixDofConstraintConfiguration::m_targetOrientation)
                ->Field("TargetVelocity", &SixDofConstraintConfiguration::m_targetVelocity)
                ->Field("Space", &SixDofConstraintConfiguration::m_space)
                ->Field("SwingType", &SixDofConstraintConfiguration::m_swingType);

            serializeContext
                ->Class<SliderConstraintConfiguration>()
                ->Field("FirstPoint", &SliderConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &SliderConstraintConfiguration::m_secondPoint)
                ->Field("FirstNormalAxis", &SliderConstraintConfiguration::m_firstNormalAxis)
                ->Field("FirstSliderAxis", &SliderConstraintConfiguration::m_firstSliderAxis)
                ->Field("SecondNormalAxis", &SliderConstraintConfiguration::m_secondNormalAxis)
                ->Field("SecondSliderAxis", &SliderConstraintConfiguration::m_secondSliderAxis)
                ->Field("LimitSpring", &SliderConstraintConfiguration::m_limitSpring)
                ->Field("Motor", &SliderConstraintConfiguration::m_motor)
                ->Field("MaximumFrictionForce", &SliderConstraintConfiguration::m_maximumFrictionForce)
                ->Field("MaximumLimit", &SliderConstraintConfiguration::m_maximumLimit)
                ->Field("MinimumLimit", &SliderConstraintConfiguration::m_minimumLimit)
                ->Field("TargetPosition", &SliderConstraintConfiguration::m_targetPosition)
                ->Field("TargetVelocity", &SliderConstraintConfiguration::m_targetVelocity)
                ->Field("Space", &SliderConstraintConfiguration::m_space)
                ->Field("AutoDetectPoint", &SliderConstraintConfiguration::m_autoDetectPoint);

            serializeContext
                ->Class<SwingTwistConstraintConfiguration>()
                ->Field("FirstPoint", &SwingTwistConstraintConfiguration::m_firstPoint)
                ->Field("SecondPoint", &SwingTwistConstraintConfiguration::m_secondPoint)
                ->Field("FirstPlaneAxis", &SwingTwistConstraintConfiguration::m_firstPlaneAxis)
                ->Field("FirstTwistAxis", &SwingTwistConstraintConfiguration::m_firstTwistAxis)
                ->Field("SecondPlaneAxis", &SwingTwistConstraintConfiguration::m_secondPlaneAxis)
                ->Field("SecondTwistAxis", &SwingTwistConstraintConfiguration::m_secondTwistAxis)
                ->Field("SwingMotor", &SwingTwistConstraintConfiguration::m_swingMotor)
                ->Field("TwistMotor", &SwingTwistConstraintConfiguration::m_twistMotor)
                ->Field("MaximumFrictionTorque", &SwingTwistConstraintConfiguration::m_maximumFrictionTorque)
                ->Field("NormalHalfConeAngle", &SwingTwistConstraintConfiguration::m_normalHalfConeAngle)
                ->Field("PlaneHalfConeAngle", &SwingTwistConstraintConfiguration::m_planeHalfConeAngle)
                ->Field("TwistMaximumAngle", &SwingTwistConstraintConfiguration::m_twistMaximumAngle)
                ->Field("TwistMinimumAngle", &SwingTwistConstraintConfiguration::m_twistMinimumAngle)
                ->Field("TargetAngularVelocity", &SwingTwistConstraintConfiguration::m_targetAngularVelocity)
                ->Field("TargetOrientation", &SwingTwistConstraintConfiguration::m_targetOrientation)
                ->Field("Space", &SwingTwistConstraintConfiguration::m_space)
                ->Field("SwingType", &SwingTwistConstraintConfiguration::m_swingType);

            serializeContext
                ->Class<GearConstraintConfiguration>()
                ->Field("FirstHingeConstraintHandle", &GearConstraintConfiguration::m_firstHingeConstraintHandle)
                ->Field("SecondHingeConstraintHandle", &GearConstraintConfiguration::m_secondHingeConstraintHandle)
                ->Field("FirstHingeAxis", &GearConstraintConfiguration::m_firstHingeAxis)
                ->Field("SecondHingeAxis", &GearConstraintConfiguration::m_secondHingeAxis)
                ->Field("Ratio", &GearConstraintConfiguration::m_ratio)
                ->Field("Space", &GearConstraintConfiguration::m_space);

            serializeContext
                ->Class<PathConstraintConfiguration>()
                ->Field("PathHandle", &PathConstraintConfiguration::m_pathHandle)
                ->Field("PathPosition", &PathConstraintConfiguration::m_pathPosition)
                ->Field("PathRotation", &PathConstraintConfiguration::m_pathRotation)
                ->Field("PositionMotor", &PathConstraintConfiguration::m_positionMotor)
                ->Field("MaximumFrictionForce", &PathConstraintConfiguration::m_maximumFrictionForce)
                ->Field("PathFraction", &PathConstraintConfiguration::m_pathFraction)
                ->Field("TargetPathFraction", &PathConstraintConfiguration::m_targetPathFraction)
                ->Field("TargetVelocity", &PathConstraintConfiguration::m_targetVelocity)
                ->Field("RotationConstraint", &PathConstraintConfiguration::m_rotationConstraint);

            serializeContext
                ->Class<RackAndPinionConstraintConfiguration>()
                ->Field("PinionConstraintHandle", &RackAndPinionConstraintConfiguration::m_pinionConstraintHandle)
                ->Field("RackConstraintHandle", &RackAndPinionConstraintConfiguration::m_rackConstraintHandle)
                ->Field("HingeAxis", &RackAndPinionConstraintConfiguration::m_hingeAxis)
                ->Field("SliderAxis", &RackAndPinionConstraintConfiguration::m_sliderAxis)
                ->Field("Ratio", &RackAndPinionConstraintConfiguration::m_ratio)
                ->Field("Space", &RackAndPinionConstraintConfiguration::m_space);

            serializeContext
                ->Class<ConstraintConfiguration>()
                ->Field("FirstBodyHandle", &ConstraintConfiguration::m_firstBodyHandle)
                ->Field("SecondBodyHandle", &ConstraintConfiguration::m_secondBodyHandle)
                ->Field("Geometry", &ConstraintConfiguration::m_geometry)
                ->Field("EntityId", &ConstraintConfiguration::m_entityId)
                ->Field("Name", &ConstraintConfiguration::m_name)
                ->Field("UserData", &ConstraintConfiguration::m_userData)
                ->Field("Priority", &ConstraintConfiguration::m_priority)
                ->Field("PositionStepCount", &ConstraintConfiguration::m_positionStepCount)
                ->Field("VelocityStepCount", &ConstraintConfiguration::m_velocityStepCount)
                ->Field("Enabled", &ConstraintConfiguration::m_enabled)
                ->Field("StartInSimulation", &ConstraintConfiguration::m_startInSimulation);

            serializeContext
                ->Class<ConstraintSolverConfiguration>()
                ->Field("Priority", &ConstraintSolverConfiguration::m_priority)
                ->Field("PositionStepCount", &ConstraintSolverConfiguration::m_positionStepCount)
                ->Field("VelocityStepCount", &ConstraintSolverConfiguration::m_velocityStepCount);

            serializeContext
                ->Class<ConstraintState>()
                ->Field("FirstBodyHandle", &ConstraintState::m_firstBodyHandle)
                ->Field("SecondBodyHandle", &ConstraintState::m_secondBodyHandle)
                ->Field("EntityId", &ConstraintState::m_entityId)
                ->Field("Name", &ConstraintState::m_name)
                ->Field("UserData", &ConstraintState::m_userData)
                ->Field("Kind", &ConstraintState::m_kind)
                ->Field("Solver", &ConstraintState::m_solver)
                ->Field("Active", &ConstraintState::m_active)
                ->Field("Enabled", &ConstraintState::m_enabled)
                ->Field("IsInSimulation", &ConstraintState::m_isInSimulation);

            serializeContext
                ->Class<ConeMeasurements>()
                ->Field("PositionImpulse", &ConeMeasurements::m_positionImpulse)
                ->Field("RotationImpulse", &ConeMeasurements::m_rotationImpulse);

            serializeContext
                ->Class<CustomConstraintMeasurements>()
                ->Field("RowCount", &CustomConstraintMeasurements::m_rowCount);

            serializeContext
                ->Class<DistanceMeasurements>()
                ->Field("PositionImpulse", &DistanceMeasurements::m_positionImpulse);

            serializeContext
                ->Class<FixedMeasurements>()
                ->Field("PositionImpulse", &FixedMeasurements::m_positionImpulse)
                ->Field("RotationImpulse", &FixedMeasurements::m_rotationImpulse);

            serializeContext
                ->Class<GearMeasurements>()
                ->Field("RotationImpulse", &GearMeasurements::m_rotationImpulse);

            serializeContext
                ->Class<HingeMeasurements>()
                ->Field("PositionImpulse", &HingeMeasurements::m_positionImpulse)
                ->Field("RotationImpulse", &HingeMeasurements::m_rotationImpulse)
                ->Field("Angle", &HingeMeasurements::m_angle)
                ->Field("LimitImpulse", &HingeMeasurements::m_limitImpulse)
                ->Field("MotorImpulse", &HingeMeasurements::m_motorImpulse);

            serializeContext
                ->Class<PathMeasurements>()
                ->Field("PositionImpulse", &PathMeasurements::m_positionImpulse)
                ->Field("RotationHingeImpulse", &PathMeasurements::m_rotationHingeImpulse)
                ->Field("RotationImpulse", &PathMeasurements::m_rotationImpulse)
                ->Field("Fraction", &PathMeasurements::m_fraction)
                ->Field("MotorImpulse", &PathMeasurements::m_motorImpulse)
                ->Field("PositionLimitImpulse", &PathMeasurements::m_positionLimitImpulse);

            serializeContext
                ->Class<PointMeasurements>()
                ->Field("PositionImpulse", &PointMeasurements::m_positionImpulse);

            serializeContext
                ->Class<PulleyMeasurements>()
                ->Field("Length", &PulleyMeasurements::m_length)
                ->Field("PositionImpulse", &PulleyMeasurements::m_positionImpulse);

            serializeContext
                ->Class<RackAndPinionMeasurements>()
                ->Field("Impulse", &RackAndPinionMeasurements::m_impulse);

            serializeContext
                ->Class<SixDofMeasurements>()
                ->Field("MotorRotationImpulse", &SixDofMeasurements::m_motorRotationImpulse)
                ->Field("MotorTranslationImpulse", &SixDofMeasurements::m_motorTranslationImpulse)
                ->Field("PositionImpulse", &SixDofMeasurements::m_positionImpulse)
                ->Field("RotationImpulse", &SixDofMeasurements::m_rotationImpulse);

            serializeContext
                ->Class<SliderMeasurements>()
                ->Field("PositionImpulse", &SliderMeasurements::m_positionImpulse)
                ->Field("RotationImpulse", &SliderMeasurements::m_rotationImpulse)
                ->Field("MotorImpulse", &SliderMeasurements::m_motorImpulse)
                ->Field("Position", &SliderMeasurements::m_position)
                ->Field("PositionLimitImpulse", &SliderMeasurements::m_positionLimitImpulse);

            serializeContext
                ->Class<SwingTwistMeasurements>()
                ->Field("Orientation", &SwingTwistMeasurements::m_orientation)
                ->Field("MotorImpulse", &SwingTwistMeasurements::m_motorImpulse)
                ->Field("PositionImpulse", &SwingTwistMeasurements::m_positionImpulse)
                ->Field("SwingYImpulse", &SwingTwistMeasurements::m_swingYImpulse)
                ->Field("SwingZImpulse", &SwingTwistMeasurements::m_swingZImpulse)
                ->Field("TwistImpulse", &SwingTwistMeasurements::m_twistImpulse);

            serializeContext
                ->Class<ConstraintComponentConfiguration>()
                ->Field("FirstBodyEntityId", &ConstraintComponentConfiguration::m_firstBodyEntityId)
                ->Field("SecondBodyEntityId", &ConstraintComponentConfiguration::m_secondBodyEntityId)
                ->Field("Geometry", &ConstraintComponentConfiguration::m_geometry)
                ->Field("UserData", &ConstraintComponentConfiguration::m_userData)
                ->Field("Priority", &ConstraintComponentConfiguration::m_priority)
                ->Field("PositionStepCount", &ConstraintComponentConfiguration::m_positionStepCount)
                ->Field("VelocityStepCount", &ConstraintComponentConfiguration::m_velocityStepCount)
                ->Field("Enabled", &ConstraintComponentConfiguration::m_enabled);
        }
    }
} // namespace Jolt
