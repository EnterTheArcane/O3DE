/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/SceneAsset.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>

namespace Jolt
{
    struct SceneComponentConfiguration final
    {
        AZ_TYPE_INFO(SceneComponentConfiguration, SceneComponentConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ::Data::Asset<SceneAsset> m_asset{AZ::Data::AssetLoadBehavior::NoLoad};
    };
} // namespace Jolt
