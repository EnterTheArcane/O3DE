/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/TypeIds.h>

#include <AzCore/Math/Quaternion.h>
#include <AzCore/RTTI/TypeInfo.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct WorldPosition final
    {
        AZ_TYPE_INFO(WorldPosition, WorldPositionTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        constexpr bool operator==(const WorldPosition&) const = default;

        double m_x = 0.0;
        double m_y = 0.0;
        double m_z = 0.0;
    };

    struct WorldTransform final
    {
        AZ_TYPE_INFO(WorldTransform, WorldTransformTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        WorldPosition m_position;
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
    };
} // namespace Jolt
