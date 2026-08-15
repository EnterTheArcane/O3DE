/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/WorldTypes.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void WorldPosition::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<WorldPosition>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<WorldPosition>()
                ->Field("X", &WorldPosition::m_x)
                ->Field("Y", &WorldPosition::m_y)
                ->Field("Z", &WorldPosition::m_z);
        }
    }

    void WorldTransform::Reflect(
        AZ::ReflectContext* context)
    {
        WorldPosition::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<WorldTransform>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<WorldTransform>()
                ->Field("Position", &WorldTransform::m_position)
                ->Field("Rotation", &WorldTransform::m_rotation);
        }
    }
} // namespace Jolt
