/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color64.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <type_traits>

namespace AZ
{
    static_assert(sizeof(Color64) == 8, "Color64 must be exactly 8 bytes");
    static_assert(alignof(Color64) == 8, "Color64 must be 8 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<Color64>, "Color64 must be trivially copyable");
    static_assert(std::is_standard_layout_v<Color64>, "Color64 must be standard layout");

    void Color64::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<Color64>()->
                Version(1)->
                Field("r", &Color64::m_r)->
                Field("g", &Color64::m_g)->
                Field("b", &Color64::m_b)->
                Field("a", &Color64::m_a);
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->Class<Color64>()->
                Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)->
                Attribute(AZ::Script::Attributes::Module, "math")->
                Attribute(AZ::Script::Attributes::Storage, AZ::Script::Attributes::StorageType::Value)->
                Constructor<AZ::u16, AZ::u16, AZ::u16, AZ::u16>()->
                Property("r", &Color64::GetR, &Color64::SetR)->
                Property("g", &Color64::GetG, &Color64::SetG)->
                Property("b", &Color64::GetB, &Color64::SetB)->
                Property("a", &Color64::GetA, &Color64::SetA)->
                Method("GetLuminance", &Color64::GetLuminance)->
                Method("Inverted", &Color64::Inverted)->
                Method("WithAlpha", &Color64::WithAlpha)->
                Method("IsOpaque", &Color64::IsOpaque)->
                Method("IsTransparent", &Color64::IsTransparent)->
                Method("Equal", &Color64::operator==)->
                    Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
        }
    }
} // namespace AZ
