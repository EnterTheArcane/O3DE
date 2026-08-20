/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/Character.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct CharacterComponentConfiguration final
    {
        AZ_TYPE_INFO(CharacterComponentConfiguration, CharacterComponentConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        CollisionGroupConfiguration m_collisionGroup;
        AZ::u64 m_userData = 0;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        AllowedDofs m_allowedDofs =
            AllowedDofs::TranslationX
            | AllowedDofs::TranslationY
            | AllowedDofs::TranslationZ;

        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_friction = 0.2f;
        float m_gravityFactor = 1.0f;
        float m_mass = 80.0f;
        float m_maximumPenetrationDepth = 0.05f;
        float m_maximumSeparationDistance = 0.05f;
        float m_maximumSlopeAngle = 0.87266463f;
        float m_supportingPlaneDistance = -1.0e10f;

        bool m_activate = true;
        bool m_enhancedInternalEdgeRemoval = false;
    };

    struct VirtualCharacterComponentConfiguration final
    {
        AZ_TYPE_INFO(VirtualCharacterComponentConfiguration, VirtualCharacterComponentConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        VirtualCharacterUpdateConfiguration m_update;

        AZ::u64 m_userData = 0;
        ObjectLayer m_innerBodyObjectLayer = DefaultLayers::Moving;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        AZ::Vector3 m_shapeOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_characterPadding = 0.02f;
        float m_collisionTolerance = 1.0e-3f;
        float m_hitReductionCosMaximumAngle = 0.999f;
        float m_mass = 70.0f;
        float m_maximumPenetrationDepth = 0.05f;
        float m_maximumSlopeAngle = 0.87266463f;
        float m_maximumStrength = 100.0f;
        float m_minimumTimeRemaining = 1.0e-4f;
        float m_penetrationRecoverySpeed = 1.0f;
        float m_predictiveContactDistance = 0.1f;
        float m_supportingPlaneDistance = -1.0e10f;

        AZ::u32 m_maximumCollisionIterations = 5;
        AZ::u32 m_maximumConstraintIterations = 15;
        AZ::u32 m_maximumHitCount = 256;

        bool m_collideWithBackFaces = true;
        bool m_createInnerBody = false;
        bool m_enabled = true;
        bool m_enhancedInternalEdgeRemoval = false;
    };
} // namespace Jolt
