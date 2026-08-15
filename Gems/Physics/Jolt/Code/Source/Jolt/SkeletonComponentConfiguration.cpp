/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SkeletonComponentConfiguration.h>

#include <Jolt/Reflection.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void SkeletonComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        SkeletonAsset::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SkeletonComponentConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SkeletonComponentConfiguration>()
                ->Field("Asset", &SkeletonComponentConfiguration::m_asset);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SkeletonComponentConfiguration>("Skeleton", "Cooked skeleton and animation resources.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SkeletonComponentConfiguration::m_asset,
                        "Asset",
                        "A compiled .joltskeleton asset.");
            }
        }
    }
} // namespace Jolt
