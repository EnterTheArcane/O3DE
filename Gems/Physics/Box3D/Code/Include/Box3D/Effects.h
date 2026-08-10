/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/limits.h>

#include <Box3D/Handle.h>
#include <Box3D/TypeIds.h>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    struct ExplosionConfiguration final
    {
        AZ_TYPE_INFO(ExplosionConfiguration, "{22D9A3A0-2E24-4A58-A429-707F2A386FB6}");

        static void Reflect(AZ::ReflectContext* context);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();

        AZ::u64 m_maskBits = AZStd::numeric_limits<AZ::u64>::max();

        float m_radius = 1.0f;
        float m_falloff = 1.0f;
        float m_impulsePerArea = 1.0f;
    };

    struct WindConfiguration final
    {
        AZ_TYPE_INFO(WindConfiguration, "{17EAC067-783D-4870-AD06-D706968D3384}");

        static void Reflect(AZ::ReflectContext* context);

        AZ::Vector3 m_velocity = AZ::Vector3::CreateZero();

        float m_drag = 1.0f;
        float m_lift = 0.0f;
        float m_maximumSpeed = 10.0f;

        bool m_wake = true;
    };

    class IEffects
    {
    public:
        AZ_RTTI(IEffects, IEffectsTypeId);
        virtual ~IEffects() = default;

        [[nodiscard]]
        virtual bool ApplyWind(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WindConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool Explode(
            WorldHandle worldHandle,
            const ExplosionConfiguration& configuration) = 0;
    };
} // namespace Box3D
