/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Transform.h>

namespace Jolt::Internal
{
    inline WorldTransform ToWorldTransform(
        const AZ::Transform& transform)
    {
        const AZ::Vector3 translation = transform.GetTranslation();
        return {
            .m_position = {
                .m_x = translation.GetX(),
                .m_y = translation.GetY(),
                .m_z = translation.GetZ(),
            },
            .m_rotation = transform.GetRotation(),
        };
    }

    inline AZ::Transform ToEntityTransform(
        const WorldTransform& transform,
        const float uniformScale)
    {
        return AZ::Transform(
            AZ::Vector3(
                static_cast<float>(transform.m_position.m_x),
                static_cast<float>(transform.m_position.m_y),
                static_cast<float>(transform.m_position.m_z)),
            transform.m_rotation,
            uniformScale);
    }
} // namespace Jolt::Internal
