/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    [[nodiscard]]
    inline bool ShouldReflect(
        AZ::ReflectContext& context,
        const bool isReflected)
    {
        if (context.IsRemovingReflection())
        {
            return isReflected;
        }

        return !isReflected;
    }

    template<class Type>
    [[nodiscard]]
    bool ShouldReflect(AZ::BehaviorContext& context)
    {
        const bool isReflected = context.FindClassByTypeId(azrtti_typeid<Type>());
        return ShouldReflect(context, isReflected);
    }

    template<class Type>
    [[nodiscard]]
    bool ShouldReflect(AZ::SerializeContext& context)
    {
        const bool isReflected = context.FindClassData(azrtti_typeid<Type>());
        return ShouldReflect(context, isReflected);
    }
} // namespace Jolt
