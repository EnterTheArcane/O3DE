/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/TypeIds.h>

#include <AzCore/Math/Color.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct MaterialConfiguration final
    {
        AZ_TYPE_INFO(MaterialConfiguration, MaterialConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_debugName;
        AZ::Color m_debugColor = AZ::Colors::Grey;
    };
} // namespace Jolt
