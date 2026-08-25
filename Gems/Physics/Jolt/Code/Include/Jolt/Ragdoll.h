/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Constraint.h>
#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    enum class RagdollDriveResult : AZ::u8
    {
        None = 0,
        InvalidDeltaTime,
        InvalidHandle,
        InvalidPose,
        Success,
        UnsupportedConstraint,
    };

    struct RagdollConstraintConfiguration final
    {
        AZ_TYPE_INFO(RagdollConstraintConfiguration, RagdollConstraintConfigurationTypeId);

        ConstraintGeometry m_geometry;
        AZ::Uuid m_id = AZ::Uuid::CreateNull();
        AZ::Uuid m_firstLinkedConstraintId = AZ::Uuid::CreateNull();
        AZ::Uuid m_secondLinkedConstraintId = AZ::Uuid::CreateNull();
    };

    struct RagdollPartConfiguration final
    {
        AZ_TYPE_INFO(RagdollPartConfiguration, RagdollPartConfigurationTypeId);

        BodyConfiguration m_body;
        RagdollConstraintConfiguration m_parentConstraint;
        bool m_hasParentConstraint = false;
    };

    struct AdditionalRagdollConstraint final
    {
        AZ_TYPE_INFO(AdditionalRagdollConstraint, AdditionalRagdollConstraintTypeId);

        RagdollConstraintConfiguration m_constraint;
        AZ::u32 m_firstPartIndex = 0;
        AZ::u32 m_secondPartIndex = 0;
    };

    struct RagdollDefinitionConfiguration final
    {
        AZ_TYPE_INFO(RagdollDefinitionConfiguration, RagdollDefinitionConfigurationTypeId);

        SkeletonDefinitionHandle m_skeletonHandle;
        AZStd::vector<RagdollPartConfiguration> m_parts;
        AZStd::vector<AdditionalRagdollConstraint> m_additionalConstraints;

        AZ::u32 m_baseConstraintPriority = 0;
        float m_minimumCollisionSeparation = 0.0f;

        bool m_calculateConstraintPriorities = true;
        bool m_disableSelfCollisions = true;
        bool m_stabilize = true;
    };

    struct RagdollConfiguration final
    {
        AZ_TYPE_INFO(RagdollConfiguration, RagdollConfigurationTypeId);

        RagdollDefinitionHandle m_definitionHandle;
        WorldPosition m_rootPosition;
        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u32 m_collisionGroupId = 0;
        bool m_activate = true;
    };

    struct RagdollState final
    {
        AZ_TYPE_INFO(RagdollState, RagdollStateTypeId);

        WorldTransform m_rootTransform;
        AZ::Aabb m_bounds = AZ::Aabb::CreateNull();
        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        RagdollDefinitionHandle m_definitionHandle;
        AZ::u32 m_bodyCount = 0;
        AZ::u32 m_collisionGroupId = 0;
        AZ::u32 m_constraintCount = 0;

        bool m_isActive = false;
        bool m_isInSimulation = false;
    };

    struct RagdollConstraintBodyPair final
    {
        AZ_TYPE_INFO(RagdollConstraintBodyPair, RagdollConstraintBodyPairTypeId);

        AZ::s32 m_firstBodyIndex = -1;
        AZ::s32 m_secondBodyIndex = -1;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::RagdollDriveResult, Jolt::RagdollDriveResultTypeId);
