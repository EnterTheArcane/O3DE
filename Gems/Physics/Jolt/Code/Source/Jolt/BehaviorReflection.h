/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/std/typetraits/underlying_type.h>
#include <AzCore/std/utility/move.h>

namespace Jolt::Internal
{
    template<auto Member>
    struct BehaviorValuePropertyByValue final
    {
    };

    template<class ClassType, class ValueType, ValueType ClassType::* Member>
    struct BehaviorValuePropertyByValue<Member> final
    {
        [[nodiscard]]
        static ValueType Get(
            const ClassType* instance)
        {
            return instance->*Member;
        }

        static void Set(
            ClassType* instance,
            ValueType value)
        {
            instance->*Member = AZStd::move(value);
        }
    };

    template<auto Value>
    void ReflectEnum(
        AZ::BehaviorContext& context,
        const char* name)
    {
        context.EnumProperty<Value>(name)
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt");
    }
} // namespace Jolt::Internal

#define JOLT_BEHAVIOR_VALUE_PROPERTY(member) \
    &Jolt::Internal::BehaviorValuePropertyByValue<member>::Get, \
    &Jolt::Internal::BehaviorValuePropertyByValue<member>::Set

#define JOLT_BEHAVIOR_READONLY_PROPERTY(member) \
    &Jolt::Internal::BehaviorValuePropertyByValue<member>::Get, \
    nullptr

#define JOLT_BEHAVIOR_ENUM(context, type, value) \
    Jolt::Internal::ReflectEnum<static_cast<AZStd::underlying_type_t<type>>(type::value)>(context, #type "_" #value)
