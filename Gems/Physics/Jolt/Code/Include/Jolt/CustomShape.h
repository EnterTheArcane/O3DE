/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    //! Authoring data routed to a registered custom convex-shape provider during cooking.
    struct CustomConvexShapeConfiguration final
    {
        AZ_TYPE_INFO(CustomConvexShapeConfiguration, CustomConvexShapeConfigurationTypeId);

        AZStd::vector<AZ::u8> m_data;
        AZ::Aabb m_editorBounds = AZ::Aabb::CreateNull();
        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
    };

    //! Immutable geometry produced once at the cooking boundary.
    struct CustomConvexShapeData final
    {
        AZStd::vector<AZ::Vector3> m_points;
        float m_hullTolerance = 1.0e-3f;
        float m_maximumConvexRadius = 0.05f;
        float m_maximumConvexRadiusError = 0.05f;
    };

    //! Converts provider-specific data into immutable convex geometry.
    class ICustomConvexShapeProvider
    {
    public:
        AZ_RTTI(ICustomConvexShapeProvider, ICustomConvexShapeProviderTypeId);

        virtual ~ICustomConvexShapeProvider() = default;

        //! Identifies the provider in serialized authoring data.
        [[nodiscard]]
        virtual AZ::TypeId GetId() const = 0;

        //! Changes whenever identical input data may produce different cooked geometry.
        [[nodiscard]]
        virtual AZ::u64 GetVersion() const = 0;

        //! May be called concurrently. The implementation must be deterministic and must not call ICooking.
        [[nodiscard]]
        virtual bool Cook(
            AZStd::span<const AZ::u8> input,
            CustomConvexShapeData& output) const = 0;
    };

    struct CustomConvexShapeInfo final
    {
        AZ_TYPE_INFO(CustomConvexShapeInfo, CustomConvexShapeInfoTypeId);

        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        AZ::u64 m_providerVersion = 0;
        AZ::u64 m_sourceHash = 0;
    };
} // namespace Jolt
