/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/Collision.h>
#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    class BodyId final
    {
    public:
        AZ_TYPE_INFO(BodyId, BodyIdTypeId);

        static constexpr AZ::u32 MaximumIndex = (AZ::u32{1} << 23) - 1;

        constexpr BodyId() noexcept = default;

        constexpr BodyId(
            const AZ::u32 index,
            const AZ::u8 sequenceNumber) noexcept
        {
            if (index <= MaximumIndex)
            {
                m_value = index | (static_cast<AZ::u32>(sequenceNumber) << 23);
            }
        }

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != AZStd::numeric_limits<AZ::u32>::max();
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        friend constexpr bool operator==(BodyId, BodyId) noexcept = default;

        [[nodiscard]]
        constexpr AZ::u32 GetIndex() const noexcept
        {
            if (!IsValid())
            {
                return AZStd::numeric_limits<AZ::u32>::max();
            }

            return m_value & MaximumIndex;
        }

        [[nodiscard]]
        constexpr AZ::u8 GetSequenceNumber() const noexcept
        {
            if (!IsValid())
            {
                return 0;
            }

            return static_cast<AZ::u8>(m_value >> 23);
        }

    private:
        AZ::u32 m_value = AZStd::numeric_limits<AZ::u32>::max();
    };

    enum class BodyKind : AZ::u8
    {
        None = 0,
        Rigid,
        Soft,
    };

    enum class MassPropertiesMode : AZ::u8
    {
        None = 0,
        CalculateMassAndInertia,
        CalculateInertia,
        Provided,
    };

    struct MassPropertiesConfiguration final
    {
        AZ_TYPE_INFO(MassPropertiesConfiguration, MassPropertiesConfigurationTypeId);

        AZ::Matrix3x3 m_inertia = AZ::Matrix3x3::CreateIdentity();
        float m_inertiaMultiplier = 1.0f;
        float m_mass = 1.0f;
        MassPropertiesMode m_mode = MassPropertiesMode::CalculateMassAndInertia;
    };

    struct BodyConfiguration final
    {
        AZ_TYPE_INFO(BodyConfiguration, BodyConfigurationTypeId);

        ShapeHandle m_shapeHandle;
        WorldTransform m_transform;
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        CollisionGroupConfiguration m_collisionGroup;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        AllowedDofs m_allowedDofs = AllowedDofs::All;
        MassPropertiesConfiguration m_massProperties;
        MotionQuality m_motionQuality = MotionQuality::Discrete;
        MotionType m_motionType = MotionType::Dynamic;

        float m_friction = 0.2f;
        float m_restitution = 0.0f;
        float m_linearDamping = 0.05f;
        float m_angularDamping = 0.05f;
        float m_maximumLinearVelocity = 500.0f;
        float m_maximumAngularVelocity = 0.25f * AZ::Constants::Pi * 60.0f;
        float m_gravityFactor = 1.0f;

        AZ::u32 m_velocityStepCount = 0;
        AZ::u32 m_positionStepCount = 0;

        bool m_activate : 1 = true;
        bool m_allowDynamicOrKinematic : 1 = false;
        bool m_allowSleeping : 1 = true;
        bool m_applyGyroscopicForce : 1 = false;
        bool m_collideKinematicVsNonDynamic : 1 = false;
        bool m_enhancedInternalEdgeRemoval : 1 = false;
        bool m_isSensor : 1 = false;
        bool m_startInSimulation : 1 = true;
        bool m_useManifoldReduction : 1 = true;
    };

    struct BuoyancyConfiguration final
    {
        AZ_TYPE_INFO(BuoyancyConfiguration, BuoyancyConfigurationTypeId);

        WorldPosition m_surfacePosition;
        AZ::Vector3 m_fluidVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_gravity = AZ::Vector3(0.0f, 0.0f, -9.81f);
        AZ::Vector3 m_surfaceNormal = AZ::Vector3::CreateAxisZ();
        float m_angularDrag = 0.0f;
        float m_buoyancy = 1.0f;
        float m_deltaTime = 1.0f / 60.0f;
        float m_linearDrag = 0.0f;
    };

    struct BodyRuntimeConfiguration final
    {
        AZ_TYPE_INFO(BodyRuntimeConfiguration, BodyRuntimeConfigurationTypeId);

        MassPropertiesConfiguration m_massProperties;

        float m_angularDamping = 0.05f;
        float m_friction = 0.2f;
        float m_gravityFactor = 1.0f;
        float m_linearDamping = 0.05f;
        float m_maximumAngularVelocity = 0.25f * AZ::Constants::Pi * 60.0f;
        float m_maximumLinearVelocity = 500.0f;
        float m_restitution = 0.0f;

        AllowedDofs m_allowedDofs = AllowedDofs::All;
        MotionQuality m_motionQuality = MotionQuality::Discrete;
        AZ::u8 m_positionStepCount = 0;
        AZ::u8 m_velocityStepCount = 0;

        bool m_allowSleeping = true;
        bool m_applyGyroscopicForce = false;
        bool m_collideKinematicVsNonDynamic = false;
        bool m_enhancedInternalEdgeRemoval = false;
        bool m_isSensor = false;
        bool m_useManifoldReduction = true;
    };

    struct BodyState final
    {
        AZ_TYPE_INFO(BodyState, BodyStateTypeId);

        WorldTransform m_transform;
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        CollisionGroupConfiguration m_collisionGroup;
        ShapeHandle m_shapeHandle;
        BodyKind m_kind = BodyKind::None;
        MotionType m_motionType = MotionType::None;

        bool m_isActive = false;
        bool m_isInSimulation = false;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::BodyKind, "{389EC3FD-045E-49CD-82C6-7C2434B35640}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::MassPropertiesMode, "{BABBA4A1-8ADE-49D6-A321-D77D94289720}");
