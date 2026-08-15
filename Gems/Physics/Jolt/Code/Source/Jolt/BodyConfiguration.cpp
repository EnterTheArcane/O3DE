/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/BodyConfiguration.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void BodyId::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<BodyId>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<BodyId>()
                ->Field("Value", &BodyId::m_value);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<BodyId>("JoltBodyId")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "BodyId")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BodyId")
                ->Constructor<AZ::u32, AZ::u8>()
                ->Method("IsValid", &BodyId::IsValid)
                ->Method("GetIndex", &BodyId::GetIndex)
                ->Method("GetSequenceNumber", &BodyId::GetSequenceNumber);
        }
    }
} // namespace Jolt
