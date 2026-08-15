/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SceneComponentConfiguration.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void SceneComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        SceneAsset::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SceneComponentConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SceneComponentConfiguration>()
                ->Field("Asset", &SceneComponentConfiguration::m_asset);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SceneComponentConfiguration>("Scene configuration", "Cooked physics scene asset.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SceneComponentConfiguration::m_asset,
                        "Asset",
                        "Cooked scene to instantiate.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            if (!ShouldReflect<SceneComponentConfiguration>(*behaviorContext))
            {
                return;
            }

            behaviorContext->Class<SceneComponentConfiguration>("SceneComponentConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("asset", JOLT_BEHAVIOR_VALUE_PROPERTY(&SceneComponentConfiguration::m_asset));
        }
    }
} // namespace Jolt
