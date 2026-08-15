/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct ShapeStats final
    {
        AZ_TYPE_INFO(ShapeStats, ShapeStatsTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZ::Aabb m_localBounds = AZ::Aabb::CreateNull();
        AZ::u64 m_memorySize = 0;
        AZ::u32 m_triangleCount = 0;
    };

    struct ShapeProperties final
    {
        AZ_TYPE_INFO(ShapeProperties, ShapePropertiesTypeId);

        AZ::Matrix3x3 m_inertia = AZ::Matrix3x3::CreateZero();
        AZ::Aabb m_localBounds = AZ::Aabb::CreateNull();
        AZ::Vector3 m_centerOfMass = AZ::Vector3::CreateZero();

        float m_innerRadius = 0.0f;
        float m_mass = 0.0f;
        float m_volume = 0.0f;

        AZ::u32 m_subShapeIdBitCount = 0;

        ShapeKind m_kind = ShapeKind::None;

        bool m_mustBeStatic = false;
    };

    struct SubmergedVolumeRequest final
    {
        AZ_TYPE_INFO(SubmergedVolumeRequest, SubmergedVolumeRequestTypeId);

        WorldTransform m_centerOfMassTransform;
        WorldPosition m_surfacePosition;
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        AZ::Vector3 m_surfaceNormal = AZ::Vector3::CreateAxisZ();
    };

    struct SubmergedVolumeResult final
    {
        AZ_TYPE_INFO(SubmergedVolumeResult, SubmergedVolumeResultTypeId);

        WorldPosition m_centerOfBuoyancy;
        float m_submergedVolume = 0.0f;
        float m_totalVolume = 0.0f;
    };
} // namespace Jolt
