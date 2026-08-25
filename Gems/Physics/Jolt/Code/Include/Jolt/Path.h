/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct HermitePathPoint final
    {
        AZ_TYPE_INFO(HermitePathPoint, HermitePathPointTypeId);

        AZ::Vector3 m_normal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_tangent = AZ::Vector3::CreateAxisX();
    };

    struct HermitePathConfiguration final
    {
        AZ_TYPE_INFO(HermitePathConfiguration, HermitePathConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        JOLT_API static HermitePathConfiguration CreateDefault();

        [[nodiscard]]
        JOLT_API bool IsValid() const;

        AZStd::vector<HermitePathPoint> m_points;
        bool m_isLooping = false;
    };

    struct CustomPathConfiguration final
    {
        AZ_TYPE_INFO(CustomPathConfiguration, CustomPathConfigurationTypeId);

        AZStd::vector<AZ::u8> m_data;
        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        bool m_isLooping = false;
    };

    struct CustomPathPoint final
    {
        AZ_TYPE_INFO(CustomPathPoint, CustomPathPointTypeId);

        AZ::Vector3 m_normal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_tangent = AZ::Vector3::CreateAxisX();
    };

    //! Supplies an immutable provider-defined path without exposing native path types.
    //! Callbacks may run concurrently during simulation and must be deterministic, allocation-free, and must not call runtime capabilities.
    class ICustomPathProvider
    {
    public:
        AZ_RTTI(ICustomPathProvider, ICustomPathProviderTypeId);

        virtual ~ICustomPathProvider() = default;

        [[nodiscard]]
        virtual AZ::TypeId GetId() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetVersion() const = 0;

        [[nodiscard]]
        virtual float GetMaximumFraction(AZStd::span<const AZ::u8> data) const = 0;

        [[nodiscard]]
        virtual bool FindClosestFraction(
            AZStd::span<const AZ::u8> data,
            const AZ::Vector3& position,
            float fractionHint,
            float& fraction) const = 0;

        [[nodiscard]]
        virtual bool Sample(
            AZStd::span<const AZ::u8> data,
            float fraction,
            CustomPathPoint& point) const = 0;
    };

    struct CustomPathInfo final
    {
        AZ_TYPE_INFO(CustomPathInfo, CustomPathInfoTypeId);

        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        AZ::u64 m_providerVersion = 0;
        AZ::u64 m_sourceHash = 0;
    };

    struct PathSample final
    {
        AZ_TYPE_INFO(PathSample, PathSampleTypeId);

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept
        {
            return m_valid;
        }

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_tangent = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_binormal = AZ::Vector3::CreateZero();
        float m_fraction = 0.0f;
        bool m_valid = false;
    };

    struct PathState final
    {
        AZ_TYPE_INFO(PathState, PathStateTypeId);

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        float m_maximumFraction = 0.0f;
        bool m_isLooping = false;
    };
} // namespace Jolt
