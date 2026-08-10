/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    //! Body relationship, local frames, solver tuning, and break thresholds shared by every joint kind.
    struct JointCommonConfiguration final
    {
        AZ_TYPE_INFO(JointCommonConfiguration, "{C3C6059B-55B6-44CC-961F-B949810FD6A8}");

        BodyHandle m_parentBody;
        BodyHandle m_childBody;

        AZ::Transform m_parentLocalFrame = AZ::Transform::CreateIdentity();
        AZ::Transform m_childLocalFrame = AZ::Transform::CreateIdentity();

        float m_forceThreshold = AZStd::numeric_limits<float>::max();
        float m_torqueThreshold = AZStd::numeric_limits<float>::max();

        float m_constraintHertz = 60.0f;
        float m_constraintDampingRatio = 2.0f;

        float m_drawScale = 1.0f;

        bool m_collideConnected = false;
    };

    struct ParallelJointConfiguration final
    {
        AZ_TYPE_INFO(ParallelJointConfiguration, "{739073D0-E2E4-4753-921D-2D3EE282B59C}");

        JointCommonConfiguration m_common;

        float m_hertz = 1.0f;
        float m_dampingRatio = 1.0f;
        float m_maxTorque = AZStd::numeric_limits<float>::max();
    };

    struct DistanceJointConfiguration final
    {
        AZ_TYPE_INFO(DistanceJointConfiguration, "{8421484B-E8A0-4609-9DFF-8CDE6028BC45}");

        JointCommonConfiguration m_common;

        float m_length = 1.0f;

        float m_lowerSpringForce = -AZStd::numeric_limits<float>::max();
        float m_upperSpringForce = AZStd::numeric_limits<float>::max();
        float m_hertz = 0.0f;
        float m_dampingRatio = 0.0f;

        float m_minLength = 0.0f;
        float m_maxLength = AZStd::numeric_limits<float>::max();

        float m_maxMotorForce = 0.0f;
        float m_motorSpeed = 0.0f;

        bool m_enableSpring = false;
        bool m_enableLimit = false;
        bool m_enableMotor = false;
    };

    struct FilterJointConfiguration final
    {
        AZ_TYPE_INFO(FilterJointConfiguration, "{DD0BFCB8-BA4D-438E-BA50-6AA6ED31EA4A}");

        JointCommonConfiguration m_common;
    };

    struct MotorJointConfiguration final
    {
        AZ_TYPE_INFO(MotorJointConfiguration, "{0359A77A-5C63-4125-9CD0-631DAB5928C4}");

        JointCommonConfiguration m_common;

        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();
        float m_maxVelocityForce = 0.0f;
        float m_maxVelocityTorque = 0.0f;

        float m_linearHertz = 0.0f;
        float m_linearDampingRatio = 0.0f;
        float m_maxSpringForce = 0.0f;

        float m_angularHertz = 0.0f;
        float m_angularDampingRatio = 0.0f;
        float m_maxSpringTorque = 0.0f;
    };

    struct PrismaticJointConfiguration final
    {
        AZ_TYPE_INFO(PrismaticJointConfiguration, "{E444EE92-4728-45B4-A5A0-700EBAD54B04}");

        JointCommonConfiguration m_common;

        float m_hertz = 0.0f;
        float m_dampingRatio = 0.0f;
        float m_targetTranslation = 0.0f;

        float m_lowerTranslation = 0.0f;
        float m_upperTranslation = 0.0f;

        float m_maxMotorForce = 0.0f;
        float m_motorSpeed = 0.0f;

        bool m_enableSpring = false;
        bool m_enableLimit = false;
        bool m_enableMotor = false;
    };

    struct RevoluteJointConfiguration final
    {
        AZ_TYPE_INFO(RevoluteJointConfiguration, "{A6D4DCA3-0B2C-4226-B9F6-70FF9334794B}");

        JointCommonConfiguration m_common;

        float m_targetAngle = 0.0f;
        float m_hertz = 0.0f;
        float m_dampingRatio = 0.0f;

        float m_lowerAngle = 0.0f;
        float m_upperAngle = 0.0f;

        float m_maxMotorTorque = 0.0f;
        float m_motorSpeed = 0.0f;

        bool m_enableSpring = false;
        bool m_enableLimit = false;
        bool m_enableMotor = false;
    };

    struct SphericalJointConfiguration final
    {
        AZ_TYPE_INFO(SphericalJointConfiguration, "{05D4C018-EA31-4553-8466-EE5D87DB1F29}");

        static constexpr float MaximumConeAngle = AZ::Constants::HalfPi;

        JointCommonConfiguration m_common;

        AZ::Quaternion m_targetRotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_motorVelocity = AZ::Vector3::CreateZero();

        float m_hertz = 0.0f;
        float m_dampingRatio = 0.0f;

        float m_coneAngle = 0.0f;
        float m_lowerTwistAngle = 0.0f;
        float m_upperTwistAngle = 0.0f;

        float m_maxMotorTorque = 0.0f;

        bool m_enableSpring = false;
        bool m_enableConeLimit = false;
        bool m_enableTwistLimit = false;
        bool m_enableMotor = false;
    };

    struct WeldJointConfiguration final
    {
        AZ_TYPE_INFO(WeldJointConfiguration, "{57BCAAE6-28B8-4A83-B5CD-B5F1CC570E4A}");

        JointCommonConfiguration m_common;

        float m_linearHertz = 0.0f;
        float m_angularHertz = 0.0f;

        float m_linearDampingRatio = 0.0f;
        float m_angularDampingRatio = 0.0f;
    };

    struct WheelJointConfiguration final
    {
        AZ_TYPE_INFO(WheelJointConfiguration, "{0ED15073-DA6A-430D-AC22-4DE75B4B1318}");

        JointCommonConfiguration m_common;

        float m_suspensionHertz = 1.0f;
        float m_suspensionDampingRatio = 0.7f;
        float m_lowerSuspensionLimit = 0.0f;
        float m_upperSuspensionLimit = 0.0f;

        float m_maxSpinTorque = 0.0f;
        float m_spinSpeed = 0.0f;

        float m_steeringHertz = 1.0f;
        float m_steeringDampingRatio = 0.7f;
        float m_targetSteeringAngle = 0.0f;
        float m_maxSteeringTorque = 0.0f;
        float m_lowerSteeringLimit = 0.0f;
        float m_upperSteeringLimit = 0.0f;

        bool m_enableSuspensionSpring = true;
        bool m_enableSuspensionLimit = false;
        bool m_enableSpinMotor = false;
        bool m_enableSteering = false;
        bool m_enableSteeringLimit = false;
    };

    using JointConfiguration = AZStd::variant<
        ParallelJointConfiguration,
        DistanceJointConfiguration,
        FilterJointConfiguration,
        MotorJointConfiguration,
        PrismaticJointConfiguration,
        RevoluteJointConfiguration,
        SphericalJointConfiguration,
        WeldJointConfiguration,
        WheelJointConfiguration>;

    struct DistanceJointState final
    {
        AZ_TYPE_INFO(DistanceJointState, "{D8900A78-4704-47EC-9232-8F71A1B56EAA}");

        float m_length = 0.0f;
        float m_motorForce = 0.0f;
    };

    struct PrismaticJointState final
    {
        AZ_TYPE_INFO(PrismaticJointState, "{4CF6A81A-BF0F-4342-A3F4-EB2E0BA34A76}");

        float m_translation = 0.0f;
        float m_speed = 0.0f;
        float m_motorForce = 0.0f;
    };

    struct RevoluteJointState final
    {
        AZ_TYPE_INFO(RevoluteJointState, "{1D74B898-43ED-41E2-A285-E8257C834582}");

        float m_angle = 0.0f;
        float m_motorTorque = 0.0f;
    };

    struct SphericalJointState final
    {
        AZ_TYPE_INFO(SphericalJointState, "{CF5CA7E0-CB48-48B9-BC74-657BD5003AAE}");

        AZ::Vector3 m_motorTorque = AZ::Vector3::CreateZero();
        float m_coneAngle = 0.0f;
        float m_twistAngle = 0.0f;
    };

    struct WheelJointState final
    {
        AZ_TYPE_INFO(WheelJointState, "{DE678B16-AD6D-4430-8DAF-6EED4322E061}");

        float m_spinSpeed = 0.0f;
        float m_spinTorque = 0.0f;
        float m_steeringAngle = 0.0f;
        float m_steeringTorque = 0.0f;
    };

    using JointState = AZStd::variant<
        AZStd::monostate,
        DistanceJointState,
        PrismaticJointState,
        RevoluteJointState,
        SphericalJointState,
        WheelJointState>;

    struct JointMeasurements final
    {
        AZ_TYPE_INFO(JointMeasurements, "{6837485F-2706-4A4C-8862-487E777FC5E2}");

        AZ::Vector3 m_constraintForce = AZ::Vector3::CreateZero();
        AZ::Vector3 m_constraintTorque = AZ::Vector3::CreateZero();

        JointState m_state;
        float m_linearSeparation = 0.0f;
        float m_angularSeparation = 0.0f;
    };

    void ReflectJoints(AZ::ReflectContext* context);
} // namespace Box3D
