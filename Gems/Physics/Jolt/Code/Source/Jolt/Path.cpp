/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Path.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    HermitePathConfiguration HermitePathConfiguration::CreateDefault()
    {
        HermitePathConfiguration configuration;
        configuration.m_points = {
            HermitePathPoint{
                .m_position = AZ::Vector3(-0.5f, 0.0f, 0.0f),
            },
            HermitePathPoint{
                .m_position = AZ::Vector3(0.5f, 0.0f, 0.0f),
            },
        };
        return configuration;
    }

    void HermitePathConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<HermitePathConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<HermitePathPoint>()
                ->Field("Normal", &HermitePathPoint::m_normal)
                ->Field("Position", &HermitePathPoint::m_position)
                ->Field("Tangent", &HermitePathPoint::m_tangent);

            serializeContext
                ->Class<HermitePathConfiguration>()
                ->Field("Points", &HermitePathConfiguration::m_points)
                ->Field("IsLooping", &HermitePathConfiguration::m_isLooping);

            serializeContext
                ->Class<CustomPathConfiguration>()
                ->Field("Data", &CustomPathConfiguration::m_data)
                ->Field("ProviderId", &CustomPathConfiguration::m_providerId)
                ->Field("IsLooping", &CustomPathConfiguration::m_isLooping);

            serializeContext
                ->Class<CustomPathPoint>()
                ->Field("Normal", &CustomPathPoint::m_normal)
                ->Field("Position", &CustomPathPoint::m_position)
                ->Field("Tangent", &CustomPathPoint::m_tangent);

            serializeContext
                ->Class<CustomPathInfo>()
                ->Field("ProviderId", &CustomPathInfo::m_providerId)
                ->Field("ProviderVersion", &CustomPathInfo::m_providerVersion)
                ->Field("SourceHash", &CustomPathInfo::m_sourceHash);
        }
    }
} // namespace Jolt
