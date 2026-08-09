/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>

#include <Box3D/Collision.h>
#include <Box3D/Handle.h>
#include <Box3D/TypeIds.h>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    //! Capsule mover geometry, filtering, movement limits, and interaction behavior.
    struct CharacterConfiguration final
    {
        AZ_RTTI(CharacterConfiguration, CharacterConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);
        static constexpr AZ::u32 MaximumIterationCount = 64;
        static constexpr AZ::u32 MaximumContactPlaneCount = 4096;

        AZ::Vector3 m_basePosition = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_upDirection = AZ::Vector3::CreateAxisZ();
        AZ::EntityId m_entityId{};
        AZ::Name m_name;
        CollisionFilter m_collisionFilter;
        float m_height = 1.0f;
        float m_radius = 0.25f;
        float m_maximumSlopeAngle = 30.0f;
        float m_stepHeight = 0.5f;
        float m_minimumMovementDistance = 0.001f;
        float m_maximumSpeed = 100.0f;
        float m_groundStickDistance = 0.05f;
        float m_interactionScale = 0.8f;
        AZ::u32 m_maximumIterations = 5;
        AZ::u32 m_maximumContactPlanes = 256;
        bool m_applyMoveOnFixedTick = true;
    };

    enum class CharacterSupportState : AZ::u8
    {
        Unsupported,
        Supported,
        Sliding,
    };

    //! Support selected after the most recent character movement.
    struct CharacterSupport final
    {
        AZ_TYPE_INFO(CharacterSupport, CharacterSupportTypeId);

        CharacterSupportState m_state = CharacterSupportState::Unsupported;
        BodyHandle m_bodyHandle;
        ShapeHandle m_shapeHandle;
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_velocity = AZ::Vector3::CreateZero();
    };

    struct CharacterState final
    {
        AZ_TYPE_INFO(CharacterState, CharacterStateTypeId);

        AZ::Vector3 m_basePosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_centerPosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_velocity = AZ::Vector3::CreateZero();
        CharacterSupport m_support;
    };
} // namespace Box3D
