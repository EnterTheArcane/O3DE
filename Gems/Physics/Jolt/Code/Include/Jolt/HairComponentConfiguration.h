/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Hair.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    struct HairComponentConfiguration final
    {
        AZ_TYPE_INFO(HairComponentConfiguration, HairComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(HairComponentConfiguration, AZ::SystemAllocator);

        HairComponentConfiguration() = default;

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static HairComponentConfiguration CreateDefault();

        HairDefinitionConfiguration m_definition;
        AZStd::vector<AZ::Transform> m_jointModelTransforms;
        AZ::Transform m_jointToHair = AZ::Transform::CreateIdentity();
        AZ::Transform m_scalpToHeadTransform = AZ::Transform::CreateIdentity();
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        bool m_autoUpdate = true;
        bool m_enabled = true;
    };
} // namespace Jolt
