/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/BodyConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct RigidBodyConfiguration final
    {
        AZ_TYPE_INFO(RigidBodyConfiguration, RigidBodyConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        BodyRuntimeConfiguration m_runtime;
        AZ::Vector3 m_initialLinearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_initialAngularVelocity = AZ::Vector3::CreateZero();

        CollisionGroupConfiguration m_collisionGroup;
        AZ::u64 m_userData = 0;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        MotionType m_motionType = MotionType::Dynamic;

        bool m_activate = true;
        bool m_allowDynamicOrKinematic = false;
    };

    struct StaticRigidBodyConfiguration final
    {
        AZ_TYPE_INFO(StaticRigidBodyConfiguration, StaticRigidBodyConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        CollisionGroupConfiguration m_collisionGroup;
        AZ::u64 m_userData = 0;
        ObjectLayer m_objectLayer = DefaultLayers::NonMoving;
        float m_friction = 0.2f;
        float m_restitution = 0.0f;
        bool m_enhancedInternalEdgeRemoval = false;
        bool m_isSensor = false;
        bool m_useManifoldReduction = true;
    };
} // namespace Jolt
