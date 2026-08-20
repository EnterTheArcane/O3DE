/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Material.h>
#include <Jolt/SoftBody.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    struct JOLT_API SoftBodyComponentConfiguration final
    {
        AZ_TYPE_INFO(SoftBodyComponentConfiguration, SoftBodyComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(SoftBodyComponentConfiguration, AZ::SystemAllocator);

        SoftBodyComponentConfiguration() = default;

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static SoftBodyComponentConfiguration CreateDefault();

        SoftBodyDefinitionConfiguration m_definition;
        SoftBodyConfiguration m_body;
        AZStd::vector<MaterialConfiguration> m_materials;
        bool m_enabled = true;
    };
} // namespace Jolt
