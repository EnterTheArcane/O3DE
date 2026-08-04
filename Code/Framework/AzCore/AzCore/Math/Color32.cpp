/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color32.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <type_traits>

namespace AZ
{
    static_assert(sizeof(Color32) == 4, "Color32 must be exactly 4 bytes");
    static_assert(alignof(Color32) == 4, "Color32 must be 4 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<Color32>, "Color32 must be trivially copyable");
    static_assert(std::is_standard_layout_v<Color32>, "Color32 must be standard layout");

    void Color32::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<Color32>()->
                Version(1)->
                Field("r", &Color32::m_r)->
                Field("g", &Color32::m_g)->
                Field("b", &Color32::m_b)->
                Field("a", &Color32::m_a);
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->Class<Color32>()->
                Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)->
                Attribute(AZ::Script::Attributes::Module, "math")->
                Attribute(AZ::Script::Attributes::Storage, AZ::Script::Attributes::StorageType::Value)->
                Constructor<AZ::u8, AZ::u8, AZ::u8, AZ::u8>()->
                Property("r", &Color32::GetR, &Color32::SetR)->
                Property("g", &Color32::GetG, &Color32::SetG)->
                Property("b", &Color32::GetB, &Color32::SetB)->
                Property("a", &Color32::GetA, &Color32::SetA)->
                Method("GetLuminance", &Color32::GetLuminance)->
                Method("Inverted", &Color32::Inverted)->
                Method("WithAlpha", &Color32::WithAlpha)->
                Method("IsOpaque", &Color32::IsOpaque)->
                Method("IsTransparent", &Color32::IsTransparent)->
                Method("ToRgba", &Color32::ToRgba)->
                    Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::ExcludeFlags::All)->
                Method("Equal", &Color32::operator==)->
                    Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
        }
    }
} // namespace AZ
