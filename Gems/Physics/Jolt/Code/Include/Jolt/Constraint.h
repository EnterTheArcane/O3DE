/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/CustomConstraint.h>
#include <Jolt/Handle.h>
#include <Jolt/Path.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace Jolt
{
    enum class ConstraintSpace : AZ::u8
    {
        None = 0,
        LocalToCenterOfMass,
        World,
    };

    enum class ConstraintKind : AZ::u8
    {
        None = 0,
        Cone,
        Custom,
        Distance,
        Fixed,
        Gear,
        Hinge,
        Path,
        Point,
        Pulley,
        RackAndPinion,
        SixDof,
        Slider,
        SwingTwist,
    };

    enum class SpringMode : AZ::u8
    {
        None = 0,
        FrequencyAndDamping,
        MassNormalizedStiffnessAndDamping,
        StiffnessAndDamping,
    };

    enum class MotorState : AZ::u8
    {
        None = 0,
        Off,
        Position,
        PositionAndVelocity,
        Velocity,
    };

    enum class SwingType : AZ::u8
    {
        None = 0,
        Cone,
        Pyramid,
    };

    enum class SixDofAxisMode : AZ::u8
    {
        None = 0,
        Fixed,
        Free,
        Limited,
    };

    enum class PathRotationConstraint : AZ::u8
    {
        None = 0,
        ConstrainAroundBinormal,
        ConstrainAroundNormal,
        ConstrainAroundTangent,
        ConstrainToPath,
        Free,
        FullyConstrained,
    };

    struct SpringConfiguration final
    {
        AZ_TYPE_INFO(SpringConfiguration, SpringConfigurationTypeId);

        SpringMode m_mode = SpringMode::FrequencyAndDamping;
        float m_frequencyOrStiffness = 0.0f;
        float m_damping = 0.0f;
    };

    struct MotorConfiguration final
    {
        AZ_TYPE_INFO(MotorConfiguration, MotorConfigurationTypeId);

        SpringConfiguration m_spring = {
            .m_mode = SpringMode::FrequencyAndDamping,
            .m_frequencyOrStiffness = 2.0f,
            .m_damping = 1.0f,
        };
        float m_maximumForce = AZStd::numeric_limits<float>::max();
        float m_maximumTorque = AZStd::numeric_limits<float>::max();
        float m_minimumForce = -AZStd::numeric_limits<float>::max();
        float m_minimumTorque = -AZStd::numeric_limits<float>::max();
        MotorState m_state = MotorState::Off;
    };

    struct ConeConstraintConfiguration final
    {
        AZ_TYPE_INFO(ConeConstraintConfiguration, ConeConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        AZ::Vector3 m_firstTwistAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondTwistAxis = AZ::Vector3::CreateAxisX();
        float m_halfConeAngle = 0.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct DistanceConstraintConfiguration final
    {
        AZ_TYPE_INFO(DistanceConstraintConfiguration, DistanceConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        SpringConfiguration m_limitSpring;
        float m_maximumDistance = -1.0f;
        float m_minimumDistance = -1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct CustomConstraintConfiguration final
    {
        AZ_TYPE_INFO(CustomConstraintConfiguration, CustomConstraintConfigurationTypeId);

        AZStd::vector<AZ::u8> m_data;
        WorldTransform m_firstFrame;
        WorldTransform m_secondFrame;
        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct FixedConstraintConfiguration final
    {
        AZ_TYPE_INFO(FixedConstraintConfiguration, FixedConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        AZ::Vector3 m_firstAxisX = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_firstAxisY = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_secondAxisX = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondAxisY = AZ::Vector3::CreateAxisY();
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
        bool m_autoDetectPoint = true;
    };

    struct HingeConstraintConfiguration final
    {
        AZ_TYPE_INFO(HingeConstraintConfiguration, HingeConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        AZ::Vector3 m_firstHingeAxis = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_firstNormalAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondHingeAxis = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_secondNormalAxis = AZ::Vector3::CreateAxisX();

        SpringConfiguration m_limitSpring;
        MotorConfiguration m_motor;
        float m_maximumFrictionTorque = 0.0f;
        float m_maximumLimit = AZ::Constants::Pi;
        float m_minimumLimit = -AZ::Constants::Pi;
        float m_targetAngle = 0.0f;
        float m_targetAngularVelocity = 0.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct GearConstraintConfiguration final
    {
        AZ_TYPE_INFO(GearConstraintConfiguration, GearConstraintConfigurationTypeId);

        ConstraintHandle m_firstHingeConstraintHandle;
        ConstraintHandle m_secondHingeConstraintHandle;
        AZ::Vector3 m_firstHingeAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondHingeAxis = AZ::Vector3::CreateAxisX();
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct PointConstraintConfiguration final
    {
        AZ_TYPE_INFO(PointConstraintConfiguration, PointConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct PathConstraintConfiguration final
    {
        AZ_TYPE_INFO(PathConstraintConfiguration, PathConstraintConfigurationTypeId);

        PathHandle m_pathHandle;
        AZ::Vector3 m_pathPosition = AZ::Vector3::CreateZero();
        AZ::Quaternion m_pathRotation = AZ::Quaternion::CreateIdentity();
        MotorConfiguration m_positionMotor;
        float m_maximumFrictionForce = 0.0f;
        float m_pathFraction = 0.0f;
        float m_targetPathFraction = 0.0f;
        float m_targetVelocity = 0.0f;
        PathRotationConstraint m_rotationConstraint = PathRotationConstraint::Free;
    };

    struct PulleyConstraintConfiguration final
    {
        AZ_TYPE_INFO(PulleyConstraintConfiguration, PulleyConstraintConfigurationTypeId);

        WorldPosition m_firstBodyPoint;
        WorldPosition m_firstFixedPoint;
        WorldPosition m_secondBodyPoint;
        WorldPosition m_secondFixedPoint;
        float m_maximumLength = -1.0f;
        float m_minimumLength = 0.0f;
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct RackAndPinionConstraintConfiguration final
    {
        AZ_TYPE_INFO(RackAndPinionConstraintConfiguration, RackAndPinionConstraintConfigurationTypeId);

        ConstraintHandle m_pinionConstraintHandle;
        ConstraintHandle m_rackConstraintHandle;
        AZ::Vector3 m_hingeAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_sliderAxis = AZ::Vector3::CreateAxisX();
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct SliderConstraintConfiguration final
    {
        AZ_TYPE_INFO(SliderConstraintConfiguration, SliderConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        AZ::Vector3 m_firstNormalAxis = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_firstSliderAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondNormalAxis = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_secondSliderAxis = AZ::Vector3::CreateAxisX();

        SpringConfiguration m_limitSpring;
        MotorConfiguration m_motor;
        float m_maximumFrictionForce = 0.0f;
        float m_maximumLimit = AZStd::numeric_limits<float>::max();
        float m_minimumLimit = -AZStd::numeric_limits<float>::max();
        float m_targetPosition = 0.0f;
        float m_targetVelocity = 0.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
        bool m_autoDetectPoint = false;
    };

    struct SixDofAxisConfiguration final
    {
        AZ_TYPE_INFO(SixDofAxisConfiguration, SixDofAxisConfigurationTypeId);

        SpringConfiguration m_limitSpring;
        MotorConfiguration m_motor;
        float m_maximumFriction = 0.0f;
        float m_maximumLimit = 0.0f;
        float m_minimumLimit = 0.0f;
        SixDofAxisMode m_mode = SixDofAxisMode::Free;
    };

    struct SixDofAxisLimitConfiguration final
    {
        AZ_TYPE_INFO(SixDofAxisLimitConfiguration, SixDofAxisLimitConfigurationTypeId);

        SpringConfiguration m_spring;
        float m_maximumFriction = 0.0f;
        float m_maximumLimit = 0.0f;
        float m_minimumLimit = 0.0f;
        SixDofAxisMode m_mode = SixDofAxisMode::Free;
    };

    struct SixDofConstraintConfiguration final
    {
        AZ_TYPE_INFO(SixDofConstraintConfiguration, SixDofConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        AZ::Vector3 m_firstAxisX = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_firstAxisY = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_secondAxisX = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondAxisY = AZ::Vector3::CreateAxisY();

        SixDofAxisConfiguration m_rotationX;
        SixDofAxisConfiguration m_rotationY;
        SixDofAxisConfiguration m_rotationZ;

        SixDofAxisConfiguration m_translationX;
        SixDofAxisConfiguration m_translationY;
        SixDofAxisConfiguration m_translationZ;

        AZ::Vector3 m_targetAngularVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_targetPosition = AZ::Vector3::CreateZero();
        AZ::Quaternion m_targetOrientation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_targetVelocity = AZ::Vector3::CreateZero();

        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
        SwingType m_swingType = SwingType::Cone;
    };

    struct SwingTwistConstraintConfiguration final
    {
        AZ_TYPE_INFO(SwingTwistConstraintConfiguration, SwingTwistConstraintConfigurationTypeId);

        WorldPosition m_firstPoint;
        WorldPosition m_secondPoint;
        AZ::Vector3 m_firstPlaneAxis = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_firstTwistAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondPlaneAxis = AZ::Vector3::CreateAxisY();
        AZ::Vector3 m_secondTwistAxis = AZ::Vector3::CreateAxisX();

        MotorConfiguration m_swingMotor;
        MotorConfiguration m_twistMotor;
        float m_maximumFrictionTorque = 0.0f;
        float m_normalHalfConeAngle = 0.0f;
        float m_planeHalfConeAngle = 0.0f;
        float m_twistMaximumAngle = 0.0f;
        float m_twistMinimumAngle = 0.0f;
        AZ::Vector3 m_targetAngularVelocity = AZ::Vector3::CreateZero();
        AZ::Quaternion m_targetOrientation = AZ::Quaternion::CreateIdentity();
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
        SwingType m_swingType = SwingType::Cone;
    };

    using ConstraintGeometry = AZStd::variant<
        ConeConstraintConfiguration,
        CustomConstraintConfiguration,
        DistanceConstraintConfiguration,
        FixedConstraintConfiguration,
        GearConstraintConfiguration,
        HingeConstraintConfiguration,
        PathConstraintConfiguration,
        PointConstraintConfiguration,
        PulleyConstraintConfiguration,
        RackAndPinionConstraintConfiguration,
        SixDofConstraintConfiguration,
        SliderConstraintConfiguration,
        SwingTwistConstraintConfiguration>;

    struct ConstraintConfiguration final
    {
        AZ_TYPE_INFO(ConstraintConfiguration, ConstraintConfigurationTypeId);

        BodyHandle m_firstBodyHandle;
        BodyHandle m_secondBodyHandle;
        ConstraintGeometry m_geometry;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        AZ::u32 m_priority = 0;
        AZ::u8 m_positionStepCount = 0;
        AZ::u8 m_velocityStepCount = 0;
        bool m_enabled = true;
        bool m_startInSimulation = true;
    };

    struct ConstraintSolverConfiguration final
    {
        AZ_TYPE_INFO(ConstraintSolverConfiguration, ConstraintSolverConfigurationTypeId);

        AZ::u32 m_priority = 0;
        AZ::u8 m_positionStepCount = 0;
        AZ::u8 m_velocityStepCount = 0;
    };

    struct ConstraintState final
    {
        AZ_TYPE_INFO(ConstraintState, ConstraintStateTypeId);

        BodyHandle m_firstBodyHandle;
        BodyHandle m_secondBodyHandle;
        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        ConstraintKind m_kind = ConstraintKind::None;
        ConstraintSolverConfiguration m_solver;
        bool m_active = false;
        bool m_enabled = false;
        bool m_isInSimulation = false;
    };

    struct ConeMeasurements final
    {
        AZ_TYPE_INFO(ConeMeasurements, ConeMeasurementsTypeId);

        AZ::Vector3 m_positionImpulse = AZ::Vector3::CreateZero();
        float m_rotationImpulse = 0.0f;
    };

    struct DistanceMeasurements final
    {
        AZ_TYPE_INFO(DistanceMeasurements, DistanceMeasurementsTypeId);

        float m_positionImpulse = 0.0f;
    };

    struct CustomConstraintMeasurements final
    {
        AZ_TYPE_INFO(CustomConstraintMeasurements, CustomConstraintMeasurementsTypeId);

        AZ::u32 m_rowCount = 0;
    };

    struct FixedMeasurements final
    {
        AZ_TYPE_INFO(FixedMeasurements, FixedMeasurementsTypeId);

        AZ::Vector3 m_positionImpulse = AZ::Vector3::CreateZero();
        AZ::Vector3 m_rotationImpulse = AZ::Vector3::CreateZero();
    };

    struct GearMeasurements final
    {
        AZ_TYPE_INFO(GearMeasurements, GearMeasurementsTypeId);

        float m_rotationImpulse = 0.0f;
    };

    struct HingeMeasurements final
    {
        AZ_TYPE_INFO(HingeMeasurements, HingeMeasurementsTypeId);

        AZ::Vector3 m_positionImpulse = AZ::Vector3::CreateZero();
        AZ::Vector2 m_rotationImpulse = AZ::Vector2::CreateZero();
        float m_angle = 0.0f;
        float m_limitImpulse = 0.0f;
        float m_motorImpulse = 0.0f;
    };

    struct PathMeasurements final
    {
        AZ_TYPE_INFO(PathMeasurements, PathMeasurementsTypeId);

        AZ::Vector2 m_positionImpulse = AZ::Vector2::CreateZero();
        AZ::Vector2 m_rotationHingeImpulse = AZ::Vector2::CreateZero();
        AZ::Vector3 m_rotationImpulse = AZ::Vector3::CreateZero();
        float m_fraction = 0.0f;
        float m_motorImpulse = 0.0f;
        float m_positionLimitImpulse = 0.0f;
    };

    struct PointMeasurements final
    {
        AZ_TYPE_INFO(PointMeasurements, PointMeasurementsTypeId);

        AZ::Vector3 m_positionImpulse = AZ::Vector3::CreateZero();
    };

    struct PulleyMeasurements final
    {
        AZ_TYPE_INFO(PulleyMeasurements, PulleyMeasurementsTypeId);

        float m_length = 0.0f;
        float m_positionImpulse = 0.0f;
    };

    struct RackAndPinionMeasurements final
    {
        AZ_TYPE_INFO(RackAndPinionMeasurements, RackAndPinionMeasurementsTypeId);

        float m_impulse = 0.0f;
    };

    struct SixDofMeasurements final
    {
        AZ_TYPE_INFO(SixDofMeasurements, SixDofMeasurementsTypeId);

        AZ::Vector3 m_motorRotationImpulse = AZ::Vector3::CreateZero();
        AZ::Vector3 m_motorTranslationImpulse = AZ::Vector3::CreateZero();
        AZ::Vector3 m_positionImpulse = AZ::Vector3::CreateZero();
        AZ::Vector3 m_rotationImpulse = AZ::Vector3::CreateZero();
    };

    struct SliderMeasurements final
    {
        AZ_TYPE_INFO(SliderMeasurements, SliderMeasurementsTypeId);

        AZ::Vector2 m_positionImpulse = AZ::Vector2::CreateZero();
        AZ::Vector3 m_rotationImpulse = AZ::Vector3::CreateZero();
        float m_motorImpulse = 0.0f;
        float m_position = 0.0f;
        float m_positionLimitImpulse = 0.0f;
    };

    struct SwingTwistMeasurements final
    {
        AZ_TYPE_INFO(SwingTwistMeasurements, SwingTwistMeasurementsTypeId);

        AZ::Quaternion m_orientation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_motorImpulse = AZ::Vector3::CreateZero();
        AZ::Vector3 m_positionImpulse = AZ::Vector3::CreateZero();
        float m_swingYImpulse = 0.0f;
        float m_swingZImpulse = 0.0f;
        float m_twistImpulse = 0.0f;
    };

    using ConstraintMeasurements = AZStd::variant<
        ConeMeasurements,
        CustomConstraintMeasurements,
        DistanceMeasurements,
        FixedMeasurements,
        GearMeasurements,
        HingeMeasurements,
        PathMeasurements,
        PointMeasurements,
        PulleyMeasurements,
        RackAndPinionMeasurements,
        SixDofMeasurements,
        SliderMeasurements,
        SwingTwistMeasurements>;
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::ConstraintKind, "{FFAE8B48-9210-451A-9EA3-1420B534DB8D}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::ConstraintGeometry, Jolt::ConstraintGeometryTypeId);
AZ_TYPE_INFO_SPECIALIZE(Jolt::ConstraintSpace, "{225622F8-3984-474E-8927-D10487D09C1F}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::MotorState, "{A88A87C8-984B-4295-95CF-09B9934504F7}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::PathRotationConstraint, "{2B7A8888-005E-4039-9527-6B2BA4BD1D7D}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::SpringMode, "{29E4C676-92AC-45F5-9B10-8C5C38247F5B}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::SixDofAxisMode, "{B6E2E3D3-720E-4311-8B2D-6F1C28234FE8}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::SwingType, "{FFB22A49-0D6D-4F3B-9B26-C28DCA925546}");
