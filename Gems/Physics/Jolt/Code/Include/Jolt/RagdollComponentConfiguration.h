/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/ColliderBus.h>
#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/RigidBodyConfiguration.h>
#include <Jolt/Skeleton.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    struct RagdollGearConstraintConfiguration final
    {
        AZ_TYPE_INFO(RagdollGearConstraintConfiguration, RagdollGearConstraintConfigurationTypeId);

        AZ::Vector3 m_firstHingeAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_secondHingeAxis = AZ::Vector3::CreateAxisX();
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    struct RagdollRackAndPinionConstraintConfiguration final
    {
        AZ_TYPE_INFO(RagdollRackAndPinionConstraintConfiguration, RagdollRackAndPinionConstraintConfigurationTypeId);

        AZ::Vector3 m_hingeAxis = AZ::Vector3::CreateAxisX();
        AZ::Vector3 m_sliderAxis = AZ::Vector3::CreateAxisX();
        float m_ratio = 1.0f;
        ConstraintSpace m_space = ConstraintSpace::LocalToCenterOfMass;
    };

    using RagdollConstraintComponentGeometry = AZStd::variant<
        ConeConstraintConfiguration,
        CustomConstraintConfiguration,
        DistanceConstraintConfiguration,
        FixedConstraintConfiguration,
        RagdollGearConstraintConfiguration,
        HingeConstraintConfiguration,
        PathConstraintComponentConfiguration,
        PointConstraintConfiguration,
        PulleyConstraintConfiguration,
        RagdollRackAndPinionConstraintConfiguration,
        SixDofConstraintConfiguration,
        SliderConstraintConfiguration,
        SwingTwistConstraintConfiguration>;

    struct RagdollConstraintComponentConfiguration final
    {
        AZ_TYPE_INFO(RagdollConstraintComponentConfiguration, RagdollConstraintComponentConfigurationTypeId);

        RagdollConstraintComponentGeometry m_geometry;
        AZ::Uuid m_id = AZ::Uuid::CreateRandom();
        AZ::Uuid m_firstLinkedConstraintId = AZ::Uuid::CreateNull();
        AZ::Uuid m_secondLinkedConstraintId = AZ::Uuid::CreateNull();
    };

    struct AdditionalRagdollConstraintComponentConfiguration final
    {
        AZ_TYPE_INFO(
            AdditionalRagdollConstraintComponentConfiguration,
            AdditionalRagdollConstraintComponentConfigurationTypeId);

        RagdollConstraintComponentConfiguration m_constraint;
        AZ::u32 m_firstPartIndex = 0;
        AZ::u32 m_secondPartIndex = 0;
    };

    struct RagdollPartComponentConfiguration final
    {
        AZ_TYPE_INFO(RagdollPartComponentConfiguration, RagdollPartComponentConfigurationTypeId);

        AZStd::vector<ColliderShapeConfiguration> m_shapes{ColliderShapeConfiguration{}};
        RigidBodyConfiguration m_body;
        AZ::Transform m_modelTransform = AZ::Transform::CreateIdentity();
        RagdollConstraintComponentConfiguration m_parentConstraint;
        MotionType m_motionType = MotionType::Dynamic;
        bool m_hasParentConstraint = false;
    };

    struct JOLT_API RagdollComponentConfiguration final
    {
        AZ_TYPE_INFO(RagdollComponentConfiguration, RagdollComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(RagdollComponentConfiguration, AZ::SystemAllocator);

        RagdollComponentConfiguration() = default;

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static RagdollComponentConfiguration CreateDefault();

        SkeletonDefinitionConfiguration m_skeleton;
        AZStd::vector<RagdollPartComponentConfiguration> m_parts;
        AZStd::vector<AdditionalRagdollConstraintComponentConfiguration> m_additionalConstraints;

        AZ::u32 m_baseConstraintPriority = 0;
        AZ::u32 m_collisionGroupId = 0;
        float m_minimumCollisionSeparation = 0.0f;

        bool m_activate = true;
        bool m_calculateConstraintPriorities = true;
        bool m_disableSelfCollisions = true;
        bool m_enabled = true;
        bool m_stabilize = true;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::RagdollConstraintComponentGeometry, Jolt::RagdollConstraintComponentGeometryTypeId);
