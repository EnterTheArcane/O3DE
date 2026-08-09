/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>

#include <Box3D/TypeIds.h>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    enum class BodyType : AZ::u8
    {
        Static,
        Kinematic,
        Dynamic,
    };

    struct MotionLocks final
    {
        AZ_TYPE_INFO(MotionLocks, "{ECE993C5-D80B-49F7-BD24-1FC9D2D431D0}");

        bool m_linearX = false;
        bool m_linearY = false;
        bool m_linearZ = false;
        bool m_angularX = false;
        bool m_angularY = false;
        bool m_angularZ = false;
    };

    //! Initial state and simulation policy for one body.
    struct RigidBodyConfiguration final
    {
        AZ_TYPE_INFO(RigidBodyConfiguration, RigidBodyConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();
        AZ::EntityId m_entityId{};
        AZ::Name m_name;
        float m_linearDamping = 0.0f;
        float m_angularDamping = 0.0f;
        float m_gravityScale = 1.0f;
        float m_sleepThreshold = 0.05f;
        MotionLocks m_motionLocks;
        BodyType m_bodyType = BodyType::Dynamic;
        bool m_enableSleep = true;
        bool m_startAwake = true;
        bool m_isBullet = false;
        bool m_isEnabled = true;
        bool m_allowFastRotation = false;
        bool m_enableContactRecycling = true;
    };
} // namespace Box3D

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Box3D::BodyType, "{51B953E3-71B0-4C21-8C99-C2B8F6E22567}");
}
