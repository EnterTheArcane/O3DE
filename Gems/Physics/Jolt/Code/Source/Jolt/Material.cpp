/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Material.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void MaterialConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<MaterialConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<MaterialConfiguration>()
                ->Field("DebugName", &MaterialConfiguration::m_debugName)
                ->Field("DebugColor", &MaterialConfiguration::m_debugColor);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<MaterialConfiguration>("Material", "Debug identity for native collision material callbacks.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_debugName, "Debug name", "")
                    ->DataElement(AZ::Edit::UIHandlers::Color, &MaterialConfiguration::m_debugColor, "Debug color", "");
            }
        }
    }
} // namespace Jolt
