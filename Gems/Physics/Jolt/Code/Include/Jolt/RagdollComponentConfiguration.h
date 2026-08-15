/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/ColliderBus.h>
#include <Jolt/Ragdoll.h>
#include <Jolt/RigidBodyConfiguration.h>
#include <Jolt/Skeleton.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>

namespace Jolt
{
    struct RagdollPartComponentConfiguration final
    {
        AZ_TYPE_INFO(RagdollPartComponentConfiguration, RagdollPartComponentConfigurationTypeId);

        AZStd::vector<ColliderShapeConfiguration> m_shapes{ColliderShapeConfiguration{}};
        RigidBodyConfiguration m_body;
        AZ::Transform m_modelTransform = AZ::Transform::CreateIdentity();
        AZStd::optional<ConstraintGeometry> m_toParent;
        MotionType m_motionType = MotionType::Dynamic;
    };

    struct RagdollComponentConfiguration final
    {
        AZ_TYPE_INFO(RagdollComponentConfiguration, RagdollComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(RagdollComponentConfiguration, AZ::SystemAllocator);

        RagdollComponentConfiguration() = default;

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static RagdollComponentConfiguration CreateDefault();

        SkeletonDefinitionConfiguration m_skeleton;
        AZStd::vector<RagdollPartComponentConfiguration> m_parts;
        AZStd::vector<AdditionalRagdollConstraint> m_additionalConstraints;

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
