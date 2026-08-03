/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color64.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ
{
    void Color64::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            // Serialized as the packed integer rather than as four floats. That is exact, compact,
            // and endian-stable through the u64 rather than through the union's byte view.
            serializeContext->Class<Color64>()
                ->Version(1)
                ->Field("packed", &Color64::m_packed);
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->Class<Color64>()
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "math")
                ->Attribute(AZ::Script::Attributes::Storage, AZ::Script::Attributes::StorageType::Value)
                ->Constructor<AZ::u16, AZ::u16, AZ::u16, AZ::u16>()
                ->Property("r", &Color64::GetR, &Color64::SetR)
                ->Property("g", &Color64::GetG, &Color64::SetG)
                ->Property("b", &Color64::GetB, &Color64::SetB)
                ->Property("a", &Color64::GetA, &Color64::SetA)
                ->Method("GetLuminance", &Color64::GetLuminance)
                ->Method("Inverted", &Color64::Inverted)
                ->Method("WithAlpha", &Color64::WithAlpha)
                ->Method("IsOpaque", &Color64::IsOpaque)
                ->Method("IsTransparent", &Color64::IsTransparent)
                ->Method("AsU64Rgba", &Color64::AsU64Rgba)
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::ExcludeFlags::All)
                ->Method("Equal", &Color64::operator==)
                    ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal)
                ;
        }
    }
} // namespace AZ
