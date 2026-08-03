/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/ColorSwatch.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ
{
    void ColorSwatch::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            // Serialized as the packed integer rather than as four floats. That is exact, compact,
            // and endian-stable through the u32 rather than through the union's byte view.
            serializeContext->Class<ColorSwatch>()
                ->Version(1)
                ->Field("packed", &ColorSwatch::m_packed);
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->Class<ColorSwatch>()
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "math")
                ->Attribute(AZ::Script::Attributes::Storage, AZ::Script::Attributes::StorageType::Value)
                ->Constructor<AZ::u8, AZ::u8, AZ::u8, AZ::u8>()
                ->Property("r", &ColorSwatch::GetR, nullptr)
                ->Property("g", &ColorSwatch::GetG, nullptr)
                ->Property("b", &ColorSwatch::GetB, nullptr)
                ->Property("a", &ColorSwatch::GetA, nullptr)
                ->Method("GetLuminance", &ColorSwatch::GetLuminance)
                ->Method("Inverted", &ColorSwatch::Inverted)
                ->Method("WithAlpha", &ColorSwatch::WithAlpha)
                ->Method("IsOpaque", &ColorSwatch::IsOpaque)
                ->Method("IsTransparent", &ColorSwatch::IsTransparent)
                ->Method("AsU32Rgba", &ColorSwatch::AsU32Rgba)
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::ExcludeFlags::All)
                ->Method("Equal", &ColorSwatch::operator==)
                    ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal)
                ;
        }
    }
} // namespace AZ
