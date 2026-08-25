/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/Constraint.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct GearConstraintComponentConfiguration final
    {
        AZ_TYPE_INFO(GearConstraintComponentConfiguration, GearConstraintComponentConfigurationTypeId);

        AZ::EntityId m_firstHingeEntityId = AZ::EntityId();
        AZ::EntityId m_secondHingeEntityId = AZ::EntityId();
        AZ::Vector3 m_firstHingeAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondHingeAxis = AZ::Vector3::CreateAxisX();
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct PathConstraintComponentConfiguration final
    {
        AZ_TYPE_INFO(PathConstraintComponentConfiguration, PathConstraintComponentConfigurationTypeId);

        AZ::EntityId m_pathEntityId = AZ::EntityId();
        AZ::Vector3 m_pathPosition = AZ::Vector3::CreateZero();
        AZ::Quaternion m_pathRotation = AZ::Quaternion::CreateIdentity();
        MotorConfiguration m_positionMotor;
        float m_maximumFrictionForce = 0.0f;
        float m_pathFraction = 0.0f;
        float m_targetPathFraction = 0.0f;
        float m_targetVelocity = 0.0f;
        PathRotationConstraint m_rotationConstraint = PathRotationConstraint::Free;
    };

    struct RackAndPinionConstraintComponentConfiguration final
    {
        AZ_TYPE_INFO(RackAndPinionConstraintComponentConfiguration, RackAndPinionConstraintComponentConfigurationTypeId);

        AZ::EntityId m_pinionConstraintEntityId = AZ::EntityId();
        AZ::EntityId m_rackConstraintEntityId = AZ::EntityId();
        AZ::Vector3 m_hingeAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_sliderAxis = AZ::Vector3::CreateAxisX();
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    using ConstraintComponentGeometry = AZStd::variant<
        ConeConstraintConfiguration,
        CustomConstraintConfiguration,
        DistanceConstraintConfiguration,
        FixedConstraintConfiguration,
        GearConstraintComponentConfiguration,
        HingeConstraintConfiguration,
        PathConstraintComponentConfiguration,
        PointConstraintConfiguration,
        PulleyConstraintConfiguration,
        RackAndPinionConstraintComponentConfiguration,
        SixDofConstraintConfiguration,
        SliderConstraintConfiguration,
        SwingTwistConstraintConfiguration>;

    struct ConstraintComponentConfiguration final
    {
        AZ_TYPE_INFO(ConstraintComponentConfiguration, ConstraintComponentConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ::EntityId m_firstBodyEntityId = AZ::EntityId();
        AZ::EntityId m_secondBodyEntityId = AZ::EntityId();
        ConstraintComponentGeometry m_geometry;

        AZ::u64 m_userData = 0;
        AZ::u32 m_priority = 0;
        AZ::u8 m_positionStepCount = 0;
        AZ::u8 m_velocityStepCount = 0;
        bool m_enabled = true;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::ConstraintComponentGeometry, Jolt::ConstraintComponentGeometryTypeId);
